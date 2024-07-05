#include "IocpSocketServer.h"
#include <exception>
#include <iostream>
#include "../../utility/Logger.h"

constexpr uint32_t CONNECTOR_SOCKET_START_ID = UINT32_MAX / 2;

void SocketObjectDeleter(TransferStream* ptr)
{
	SocketObjectPool::PushDirect(static_cast<IocpConnectorStream*>(ptr));
}

void SocketInitializer::Initialize(IocpConnectorStream* const socket_stream, const uint64_t id, const uint64_t param)
{
	IocpSocketServer* server = reinterpret_cast<IocpSocketServer*>(param);
	Socket socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (socket == INVALID_SOCKET)
	{
		int last_error = WSAGetLastError();
		std::cout << last_error << std::endl;
		throw std::exception("cannot allocate win socket");
	}

	uint32_t worker_index = id % server->GetServerConfig().worker_count;

	HANDLE iocp_handle = server->GetIocpHandle(worker_index);

	if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), iocp_handle, reinterpret_cast<ULONG_PTR>(socket_stream), 0) == nullptr)
	{
		int last_error = GetLastError();
		std::cout << last_error << std::endl;
		throw std::exception("cannot register socket to iocp");
	}

	bool is_connector_socket = false;

	if (id >= CONNECTOR_SOCKET_START_ID)
	{
		sockaddr_in addr{ 0 };
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;

		if (::bind(socket, (sockaddr*)&addr, sizeof(sockaddr)) == SOCKET_ERROR)
			throw std::exception("failed to bind connector socket");

		is_connector_socket = true;
	}

	socket_stream->Initialize(id, worker_index, server, socket, SocketAddress::Empty(), is_connector_socket);
	socket_stream->SetIocpHandle(iocp_handle);

	const SocketServerConfig& config = server->GetServerConfig();

	socket_stream->InitializeBufferPool(config.read_buffer_pool_size, config.read_buffer_capacity, config.write_buffer_pool_size, config.write_buffer_capacity);
}

bool IocpSocketServer::AcceptSocket()
{
	IocpTransferStream* new_socket_stream = _acceptor_socket_pool->Pop();
	if (new_socket_stream == nullptr)
		return false;

	if (!_acceptor_stream->TransmitAccept(new_socket_stream))
	{
		_acceptor_socket_pool->Push(static_cast<IocpConnectorStream*>(new_socket_stream));
		return false;
	}

	return true;
}

void IocpSocketServer::TickMain()
{
	if (_config.update_tick == 0)
		return;

	IoContextObjectPool* tick_memory_pool = new IoContextObjectPool(_config.worker_count * 8, _config.worker_count, 1024);

	while (_start.load(std::memory_order_acquire))
	{
		for (HANDLE handle : _io_handles)
		{
			IoContext* io_context = static_cast<IoContext*>(tick_memory_pool->Pop());

			io_context->operation = IoOperation::Tick;

			// send tick
			PostQueuedCompletionStatus(handle, 0, 0, io_context);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_config.update_tick));
	}
}

void IocpSocketServer::AcceptorMain()
{
	DWORD bytes_transferred = 0;
	ULONG_PTR completion_key; // IocpTransferStream ptr
	OVERLAPPED* overlapped;   // overlapped for IoContext 

	// wait socket count
	uint32_t hang_acceptor_count = 0;

	while (true)
	{
		BOOL result = GetQueuedCompletionStatus(_acceptor_handle, &bytes_transferred, &completion_key, &overlapped, 500);

		if (hang_acceptor_count > 0)
		{
			for (; hang_acceptor_count > 0; hang_acceptor_count--)
			{
				if (!AcceptSocket())
					break;
			}
		}

		if (!result)
		{
			DWORD last_error = GetLastError();

			switch (last_error)
			{
				// 대상 Socket Handle이 아직 TIME_WAIT 상태일 때
				case ERROR_DUP_NAME:

				// IOCP Timeout
				case WAIT_TIMEOUT:

				case ERROR_NETNAME_DELETED:

				case ERROR_NETWORK_UNREACHABLE:

				case ERROR_IO_PENDING:
					continue;
			}

			assert(false);
		}

		// per io operation scope context
		IoContext* io_context = static_cast<IoContext*>(overlapped);

		if (io_context->operation != IoOperation::Accept)
		{

		}

		IoAccept(io_context, bytes_transferred);

		IocpTransferStream* stream = static_cast<IocpTransferStream*>(io_context->operation_stream._UnsafePtr());

		// send to io worker thread
		PostQueuedCompletionStatus(stream->GetIocpHandle(), 0, reinterpret_cast<ULONG_PTR>(stream), io_context);

		if (!AcceptSocket())
			hang_acceptor_count++;
	}
}

void IocpSocketServer::IoMain(const uint32_t worker_index)
{
	HANDLE iocp_handle = GetIocpHandle(worker_index);
	DWORD bytes_transferred = 0;
	ULONG_PTR completion_key; // IocpTransferStream ptr
	OVERLAPPED* overlapped;   // overlapped for IoContext 

	while (true)
	{
		BOOL result = GetQueuedCompletionStatus(iocp_handle, &bytes_transferred, &completion_key, &overlapped, INFINITE);

		// OVERLAPPED에서 IoContext 추출
		IoContext* io_context = static_cast<IoContext*>(overlapped);

		IocpConnectorStream* stream = reinterpret_cast<IocpConnectorStream*>(completion_key);

		if (!result)
		{
			int32_t last_error = GetLastError();

			// closed iocp handle
			switch (last_error)
			{
			// 대상 Socket Handle이 아직 TIME_WAIT 상태일 때
			case ERROR_DUP_NAME:

			// R/W operation 도중 상대방이 프로세스 강제 종료와 같은 HardClose를 진행했을 때
			case ERROR_NETNAME_DELETED:

			// 네트워크 단절 등의 이유로 ConnectEX operation에서 timeout이 발생했을 때
			case ERROR_SEM_TIMEOUT:

			// 물리적으로 원격 주소지에 도달할 수 없을 때
			case ERROR_NETWORK_UNREACHABLE:

			// 원격 장비는 켜져 있으나 포트가 비활성화 상태일 때
			case ERROR_PORT_UNREACHABLE:

			// remote 포트가 열려있지 않아 ConnectEX operation에 실패했을 때
			case ERROR_CONNECTION_REFUSED:

			case ERROR_IO_PENDING:
				break;

			// iocp handle이 close 됐을 때
			case ERROR_ABANDONED_WAIT_0:
				return;

			default:
				assert(false);
			}
		}

		switch (io_context->operation)
		{
		case IoOperation::Read:
		{
			IoRead(io_context, bytes_transferred);
			continue;
		}
		case IoOperation::Write:
		{
			IoWrite(io_context, bytes_transferred);
			continue;
		}
		case IoOperation::Disconnect:
		{
			IoDisconnect(io_context, bytes_transferred);
			break;
		}
		case IoOperation::Accept:
		{
			_event_handler->OnAccept(io_context->operation_stream);
			break;
		}

		case IoOperation::Connect:
		{
			IoConnect(io_context, bytes_transferred);
			break;
		}
		case IoOperation::Tick:
			_event_handler->OnTick(worker_index);
			break;
		}

		io_context->operation_stream.Release();
		IoContextObjectPool::PushDirect(io_context);
	}
}

bool IocpSocketServer::Initialize(const SocketServerConfig& config)
{
	if (_is_initialized)
		return false;

	_is_initialized = true;
	_config = config;

	if (_config.read_buffer_capacity == 0 || _config.read_buffer_capacity % 4096 != 0 || _config.write_buffer_capacity == 0 || _config.write_buffer_capacity % 4096 != 0)
		throw std::exception("buffer must align as 4096 byte");

	if (_config.read_buffer_pool_size < 4 || _config.write_buffer_pool_size < 4)
		throw std::exception("need at least of 4 buffers");

	for (uint32_t index = 0; index < _config.worker_count; index++)
	{
		HANDLE iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (iocp_handle == nullptr)
			return false;

		_io_handles.push_back(iocp_handle);
	}

	_connector_socket_pool = new SocketObjectPool(_config.max_connectable_socket_count, _config.max_connectable_socket_count, reinterpret_cast<uint64_t>(this), CONNECTOR_SOCKET_START_ID);

	return true;
}

bool IocpSocketServer::InitializeAcceptor(const SocketAddress& bind_address, const SocketOptions& options)
{
	if (_acceptor_stream != nullptr)
		return false;

	_acceptor_socket_pool = new SocketObjectPool(_config.max_acceptable_socket_count, _config.max_acceptable_socket_count, reinterpret_cast<uint64_t>(this));

	Socket acceptor_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (acceptor_socket == INVALID_SOCKET)
		return false;

	if (SetSocketOptions(acceptor_socket, options) < 0)
		return false;

	if (SetSocketOptions(acceptor_socket, { {SO_CONDITIONAL_ACCEPT, 1} }) < 0)
		return false;

	_acceptor_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (_acceptor_handle == nullptr)
		return false;

	_acceptor_stream = new IocpAcceptorStream();
	_acceptor_stream->Initialize(0, 0, this, acceptor_socket, bind_address, false);

	if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(acceptor_socket), _acceptor_handle, reinterpret_cast<ULONG_PTR>(_acceptor_stream), 0) == nullptr)
		return false;

	return true;
}

int IocpSocketServer::Start()
{
	if (!_is_initialized)
		return -1;

	_start = true;

	if (_acceptor_stream != nullptr)
	{
		Socket acceptor_socket = _acceptor_stream->GetSocket();
		sockaddr_in inet_addr = _acceptor_stream->GetSocketAddress().ToInetSocketAddress();
		
		if (bind(acceptor_socket, (struct sockaddr*)&inet_addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
			return WSAGetLastError();

		if (listen(acceptor_socket, _config.parallel_acceptor_count) == SOCKET_ERROR)
			return WSAGetLastError();

		for (uint32_t count = 0; count < _config.parallel_acceptor_count; count++)
		{
			if (!AcceptSocket())
				return WSAGetLastError();
		}

		_acceptor_worker = std::thread(&IocpSocketServer::AcceptorMain, this);
	}

	for (uint32_t worker_index = 0; worker_index < _config.worker_count; worker_index++)
		_io_workers.emplace_back(&IocpSocketServer::IoMain, this, worker_index);

	_tick_worker = std::thread(&IocpSocketServer::TickMain, this);

	return 0;
}
void IocpSocketServer::Shutdown(const bool gracefull)
{
	_start.exchange(false, std::memory_order_acq_rel);

	for (HANDLE handle : _io_handles)
		CloseHandle(handle);

	CloseHandle(_acceptor_handle);

	if (gracefull)
	{
		for (std::thread& thread : _io_workers)
			thread.join();

		_acceptor_worker.join();
		_tick_worker.join();
	}
}

void IocpSocketServer::Connect(const SocketAddress& socket_address, const SocketOptions& options, const uint64_t attachment)
{
	IocpConnectorStream* socket_stream = _connector_socket_pool->Pop();

	if (socket_stream == nullptr)
	{
		TransferStreamPtr empty_ptr(nullptr);
		_event_handler->OnConnect(empty_ptr, attachment);
		return;
	}

	socket_stream->SetSocketAddress(socket_address);
	socket_stream->TransmitConnect(attachment);
}