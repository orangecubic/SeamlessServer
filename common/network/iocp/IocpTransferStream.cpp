#include "IocpTransferStream.h"
#include "IocpSocketServer.h"
#include "IoContext.h"

void IoContextInitializer::Initialize(IoContext* const context, const uint64_t id, const uint64_t param)
{
	context->ptr = new unsigned char[param];
	context->length = 0;

	const uint32_t& capacity_ref = context->capacity;
	const_cast<uint32_t&>(capacity_ref) = static_cast<uint32_t>(param);
}

void IoContextDestructor::Destruct(IoContext* const context)
{
	delete[] context->ptr;
}

void IocpTransferStream::TransmitRead(SocketBuffer* const buffer, const uint64_t attachment)
{
	assert(buffer->capacity >= buffer->length);

	IoContext* context = static_cast<IoContext*>(buffer);
	context->operation = IoOperation::Read;
	context->attachment = attachment;
	context->operation_stream = std::move(IntrusivePtr<TransferStream>(this, SocketObjectDeleter));
	
	if (IsConnected())
	{
		WSABUF wsa_buf;
		wsa_buf.buf = reinterpret_cast<CHAR*>(buffer->ptr + buffer->length);
		wsa_buf.len = static_cast<ULONG>(buffer->capacity - buffer->length);

		DWORD flags = 0, bytes_read;

		int result = WSARecv(GetSocket(), &wsa_buf, 1, &bytes_read, &flags, static_cast<OVERLAPPED*>(context), nullptr);
		if (WSAOperationResult(result))
			return;
	}

	PostQueuedCompletionStatus(_iocp_handle, 0, reinterpret_cast<ULONG_PTR>(this), static_cast<OVERLAPPED*>(context));
}

void IocpTransferStream::TransmitWrite(SocketBuffer* const buffer, const uint32_t offset, const uint64_t attachment)
{
	assert(offset <= buffer->length);

	IoContext* context = static_cast<IoContext*>(buffer);

	context->operation = IoOperation::Write;
	context->attachment = attachment;
	context->operation_stream = std::move(IntrusivePtr<TransferStream>(this, SocketObjectDeleter));

	if (IsConnected())
	{
		WSABUF wsa_buf;
		wsa_buf.buf = reinterpret_cast<CHAR*>(buffer->ptr + offset);
		wsa_buf.len = static_cast<ULONG>(buffer->length - offset);

		DWORD flags = 0, bytes_read;

		int result = WSASend(GetSocket(), &wsa_buf, 1, &bytes_read, flags, static_cast<OVERLAPPED*>(context), nullptr);
		if (WSAOperationResult(result))
			return;
	}

	PostQueuedCompletionStatus(_iocp_handle, 0, reinterpret_cast<ULONG_PTR>(this), static_cast<OVERLAPPED*>(context));
}

void IocpTransferStream::TransmitDisconnect(const uint64_t attachment)
{
	if (SetConnectionStatus(false))
	{
		// Disconnect는 반드시 이루어져야 하기 때문에 Buffer 부족 방지
		IoContext* context = static_cast<IoContext*>(AllocateWriteBuffer(true));

		context->attachment = attachment;
		context->operation = IoOperation::Disconnect;
		context->operation_stream = std::move(IntrusivePtr<TransferStream>(this, SocketObjectDeleter));

		BOOL result = WinSockFnTable::DisconnectEx(GetSocket(), static_cast<OVERLAPPED*>(context), TF_REUSE_SOCKET, 0);
		if (result || WSAGetLastError() == WSA_IO_PENDING)
			return;

		context->operation_stream.Release();
		ReleaseWriteBuffer(context);
	}
}

SocketBuffer* IocpTransferStream::AllocateReadBuffer(bool must_allocation)
{
	IoContext* buffer = _read_buffer_pool->Pop(must_allocation);
	if (buffer != nullptr)
	{
		std::memset(buffer, 0, sizeof(OVERLAPPED) + sizeof(_IoContextStruct));
		buffer->length = 0;
		
		return buffer;
	}

	return nullptr;
}

void IocpTransferStream::ReleaseReadBuffer(SocketBuffer* const buffer)
{
	IoContext* io_context = static_cast<IoContext*>(buffer);
	
	buffer->ReleaseN(buffer->Ref());

	io_context->operation_stream.Release();
	_read_buffer_pool->Push(io_context);
}

SocketBuffer* IocpTransferStream::AllocateWriteBuffer(bool must_allocation)
{
	IoContext* buffer = _write_buffer_pool->Pop(must_allocation);
	if (buffer != nullptr)
	{
		std::memset(buffer, 0, sizeof(OVERLAPPED) + sizeof(_IoContextStruct));
		buffer->length = 0;
		assert(buffer->Ref() == 0);
		return buffer;
	}

	return nullptr;
}

void IocpTransferStream::ReleaseWriteBuffer(SocketBuffer* const buffer)
{
	IoContext* io_context = static_cast<IoContext*>(buffer);
	
	buffer->ReleaseN(buffer->Ref());

	io_context->operation_stream.Release();
	_write_buffer_pool->Push(io_context);
}

void IocpTransferStream::InitializeBufferPool(const uint32_t read_buffer_pool_size, const uint32_t read_buffer_size, const uint32_t write_buffer_pool_size, const uint32_t write_buffer_size)
{
	if (_read_buffer_pool == nullptr)
		_read_buffer_pool = new IoContextObjectPool(read_buffer_pool_size, read_buffer_pool_size / 4, read_buffer_size);

	if (_write_buffer_pool == nullptr)
		_write_buffer_pool = new IoContextObjectPool(write_buffer_pool_size, write_buffer_pool_size / 4, write_buffer_size);
}

void IocpTransferStream::SetIocpHandle(const HANDLE handle)
{
	_iocp_handle = handle;
}

HANDLE IocpTransferStream::GetIocpHandle() const
{
	return _iocp_handle;
}

IocpTransferStream::~IocpTransferStream()
{
	if (_read_buffer_pool != nullptr)
		delete _read_buffer_pool;

	if (_write_buffer_pool != nullptr)
		delete _write_buffer_pool;
}