#pragma once

#include "../memory/SharedObjectPool.h"
#include "../network/SocketServer.h"
#include "../concurrent/LinearWorker.h"
#include "../concurrent/ConcurrentMap.h"
#include "SocketSession.h"
#include "SocketWorkerInfo.h"
#include "NetworkEngineWorker.h"
#include <concepts>

class SocketSessionInitializer
{
public:
	static void Initialize(SocketSession* const session, const uint64_t id, const uint64_t param);
};

using SocketSessionPool = SharedObjectPool<SocketSession, SocketSessionInitializer>;

class NetworkEngine : public SocketEventHandler
{
private:

	SocketSessionPool* _socket_session_pool;

	SocketServer* _socket_server;
	NetworkEngineWorker* _worker;

	std::vector<SocketWorkerInfo*> _worker_infos;

	ConcurrentMap<uint64_t, SocketSessionPtr> _abandoned_session_list;
	ConcurrentMap<uint64_t, ConnectorInfo> _connector_list;

	std::atomic<uint64_t> _current_connector_id = 1;
	NetworkEngineOption _option;

	inline SocketWorkerInfo* GetWorkerInfo(const uint32_t worker_index)
	{
		assert(worker_index < _worker_infos.size());

		return _worker_infos[worker_index];
	}

	inline SocketWorkerInfo* GetWorkerInfo(const TransferStreamPtr& stream)
	{
		return GetWorkerInfo(stream->GetWorkerIndex());
	}
	
	ContextType ProcessSessionPacket(const TransferStreamPtr& stream, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer, ConnectorInfo* connector_info);

	void SendSocketContext(const DynamicBufferCursor<SocketBuffer>& buffer, const ContextType& context_type, const PacketHeader& header, const SocketSessionPtr& session, const uint64_t attachment);

	SocketContext* PrepareSocketContext(const DynamicBufferCursor<SocketBuffer>& buffer, const ContextType& context_type, const PacketHeader& header, const SocketSessionPtr& session, const uint64_t attachment, uint32_t context_sequence = 0);

	friend SocketSessionInitializer;
public:

	bool Initialize(const NetworkEngineOption& option, SocketServer* const socket_server, NetworkEngineWorker* const worker);

	int Start();

	SocketServer* GetSocketServer();

	uint64_t RegisterConnectorSocket(const SocketAddress& remote_address, const uint64_t attachment);

	void UnregisterConnectorSocket(uint64_t connector_id, bool delete_attachment = false);

	virtual void OnAccept(const TransferStreamPtr& stream) override;

	virtual void OnConnect(const TransferStreamPtr& stream, const uint64_t attachment) override;

	virtual void OnRead(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) override;

	virtual void OnWrite(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment) override;

	virtual void OnDisconnect(const TransferStreamPtr& stream, const uint64_t attachment) override;

	virtual void OnTick(const uint32_t worker_index) override;
};