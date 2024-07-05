#pragma once

#include "Meta.h"
#include "TransferStream.h"
#include <string>

// �ʼ�����
// stream�� ��� �۾��� ������ Network �����忡�� �̷�������Ѵ� (EventHandler�� ȣ��Ǵ� �����尡 ���ƾ���)
// buffer�� SharedObjectPool�� ���ؼ� �Ҵ���� buffer�� ��� �����ϴ�

class SocketEventHandler
{
public:
	virtual void OnAccept(const TransferStreamPtr& stream) {}

	// stream�� nullptr �� ��� ���� connector socket�� ����
	virtual void OnConnect(const TransferStreamPtr& stream, const uint64_t attachment) {}

	// buffer ���� �Ǵ� ���� �۾� �ʿ�
	virtual void OnRead(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) {}

	// buffer ���� �Ǵ� ���� �۾� �ʿ�
	virtual void OnWrite(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) {}

	// ȣ�� ���� ���� �ش� stream ��� �Ұ�
	virtual void OnDisconnect(const TransferStreamPtr& stream, const uint64_t attachment) {}

	// Tick ���� �� ���� �ֱ�� ȣ��Ǵ� handler
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