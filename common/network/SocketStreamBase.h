#pragma once

#include "../memory/Buffer.h"
#include "../memory/RefCounter.h"
#include "Meta.h"

class SocketServer;

class SocketStreamBase
{
protected:
	uint64_t _id = 0;

	SocketServer* _server = nullptr;
	Socket _socket;

	SocketAddress _socket_address;
	std::atomic<bool> _connected = false;
	bool _is_connector_socket = false;
	uint32_t _worker_index = 0;

	uint64_t _user_data = 0;
	
	// 멀티 스레드에서 값이 변경될 수 있는 변수
	

public:

	const SocketServer* GetSocketServer() const
	{
		return _server;
	}

	void Initialize(const uint64_t id, const uint32_t worker_index, SocketServer* const server, const Socket socket, const SocketAddress& socket_address, const bool is_connector_socket)
	{
		assert(_server == nullptr);

		_server = server;
		_worker_index = worker_index;
		_id = id;
		_socket = socket;
		_socket_address = socket_address;
		_is_connector_socket = is_connector_socket;
	}

	bool SetConnectionStatus(const bool connect)
	{
		bool expected = !connect;
		return _connected.compare_exchange_strong(expected, connect, std::memory_order_acq_rel);
	}

	void SetSocketAddress(const SocketAddress& socket_address)
	{
		_socket_address = socket_address;
	}

	Socket GetSocket() const
	{
		return _socket;
	}

	uint64_t GetId() const
	{
		return _id;
	}

	uint32_t GetWorkerIndex() const
	{
		return _worker_index;
	}

	const SocketAddress& GetSocketAddress() const
	{
		return _socket_address;
	}

	void SetUserData(const uint64_t user_data)
	{
		_user_data = user_data;
	}

	uint64_t GetUserData() const
	{
		return _user_data;
	}

	bool IsConnected() const
	{
		return _connected.load(std::memory_order_acquire);
	}

	bool IsConnectorSocket() const
	{
		return _is_connector_socket;
	}

	virtual ~SocketStreamBase() { }
};

