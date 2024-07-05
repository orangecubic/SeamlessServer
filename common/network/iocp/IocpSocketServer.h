#pragma once

#include "../../memory/ObjectPool.h"
#include "../SocketServer.h"
#include "IocpTransferStream.h"
#include "IocpAcceptorStream.h"
#include "IocpConnectorStream.h"
#include "IoContext.h"
#include <thread>
#include <vector>

void SocketObjectDeleter(TransferStream* ptr);

class SocketInitializer
{
public:
	static void Initialize(IocpConnectorStream* const socket_stream, const uint64_t id, const uint64_t param);
};

using SocketObjectPool = ObjectPool<IocpConnectorStream, true, SocketInitializer>;

class IocpSocketServer : public SocketServer
{
private:
	SocketObjectPool* _acceptor_socket_pool = nullptr;
	SocketObjectPool* _connector_socket_pool = nullptr;
	
	// Network I/O HANDLE
	std::vector<HANDLE> _io_handles;
	std::vector<std::thread> _io_workers;

	// Acceptor HANDLE
	IocpAcceptorStream* _acceptor_stream = nullptr;
	HANDLE _acceptor_handle = nullptr;
	std::thread _acceptor_worker;

	// Tick Thread
	std::thread _tick_worker;

	bool _is_initialized = false;
	
	std::atomic<bool> _start = false;

	void TickMain();
	void AcceptorMain();
	void IoMain(const uint32_t worker_index);

	bool AcceptSocket();
	void IoAccept(IoContext* const io_context, const DWORD bytes_transferred);
	void IoRead(IoContext* const io_context, const DWORD bytes_transferred);
	void IoWrite(IoContext* const io_context, const DWORD bytes_transferred);
	void IoDisconnect(IoContext* const io_context, const DWORD bytes_transferred);
	void IoConnect(IoContext* const io_context, const DWORD bytes_transferred);

public:

	HANDLE GetIocpHandle(const uint32_t worker_index)
	{
		return _io_handles[worker_index];
	}

	bool InitializeAcceptor(const SocketAddress& bind_address, const SocketOptions& options);

	virtual bool Initialize(const SocketServerConfig& config) override;

	virtual int Start() override;

	virtual void Shutdown(const bool gracefull) override;

	virtual void Connect(const SocketAddress& socket_address, const SocketOptions& options, const uint64_t attachment) override;

	/*
virtual uint64_t RegisterConnector(const SocketAddress& socket_address, const SocketOptions& options, const uint64_t attachment) override;
virtual void UnregisterConnector(const uint64_t connector_id) override;
*/
};