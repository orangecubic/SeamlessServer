#pragma once

#include "Meta.h"
#include "TransferStream.h"
#include <string>

// 필수조건
// stream은 모든 작업이 동일한 Network 쓰레드에서 이루어져야한다 (EventHandler가 호출되는 쓰레드가 같아야함)
// buffer는 SharedObjectPool을 통해서 할당받은 buffer만 사용 가능하다

class SocketEventHandler
{
public:
	virtual void OnAccept(const TransferStreamPtr& stream) {}

	// stream이 nullptr 일 경우 가용 connector socket이 부족
	virtual void OnConnect(const TransferStreamPtr& stream, const uint64_t attachment) {}

	// buffer 재사용 또는 해제 작업 필요
	virtual void OnRead(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) {}

	// buffer 재사용 또는 해제 작업 필요
	virtual void OnWrite(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) {}

	// 호출 종료 이후 해당 stream 사용 불가
	virtual void OnDisconnect(const TransferStreamPtr& stream, const uint64_t attachment) {}

	// Tick 설정 시 일정 주기로 호출되는 handler
	virtual void OnTick(const uint32_t worker_index) {}
};

struct SocketServerConfig
{
	uint32_t parallel_acceptor_count = 1;
	uint32_t max_connectable_socket_count = 256;
	uint32_t max_acceptable_socket_count = 1024;
	uint32_t worker_count = 2;
	uint32_t read_buffer_pool_size = 8;
	uint32_t read_buffer_capacity = 65536;
	uint32_t write_buffer_pool_size = 8;
	uint32_t write_buffer_capacity = 65536;


	uint32_t update_tick = 0;
};

class SocketServer
{
protected:
	SocketEventHandler* _event_handler = nullptr;
	SocketServerConfig _config = { 0 };

public:
	void SetSocketEventHandler(SocketEventHandler* const event_handler)
	{
		_event_handler = event_handler;
	}

	const SocketServerConfig& GetServerConfig()
	{
		return _config;
	}

	virtual bool Initialize(const SocketServerConfig& config) = 0;

	virtual int Start() = 0;
	virtual void Shutdown(const bool gracefull) = 0;
	virtual void Connect(const SocketAddress& socket_address, const SocketOptions& options, const uint64_t attachment) = 0;

	/*
	virtual uint64_t RegisterConnector(const SocketAddress& socket_address, const SocketOptions& options, const uint64_t attachment) = 0;
	virtual void UnregisterConnector(const uint64_t connector_id) = 0;
	*/
};