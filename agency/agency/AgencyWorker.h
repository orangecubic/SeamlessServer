#pragma once

#include <engine/protocol/Packet.h>
#include <engine/NetworkEngine.h>
#include <game/GameServer.h>
#include <game/object/Character.h>
#include <utility/Union.h>
#include <string>

class PacketHandler;

class AgencyWorker : public GameServer
{
private:
	PacketHandler* _packet_handler = nullptr;

	std::unordered_map<uint64_t, UniversalSessionInfo*> _object_sessions;

	uint32_t a1 = 0;
	uint32_t a2 = 0;

	friend PacketHandler;
public:

	virtual bool InitializeGameServer() override { return true; }
	void SetPacketHandler(PacketHandler* packet_handler);

	virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) override;

	virtual void OnClientAccepted(UniversalSessionInfo* client_session) override;
	virtual void OnClientClosed(UniversalSessionInfo* client_session) override;

	virtual void OnIntraServerConnected(IntraServerInfo* server_session) override;
	virtual void OnIntraServerAccepted(IntraServerInfo* server_session) override;
	virtual void OnIntraServerClosed(IntraServerInfo* server_session) override;

	virtual void OnClientData(UniversalSessionInfo* client_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer) override;
	virtual void OnIntraServerData(IntraServerInfo* server_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer) override;
};


enum class AgencySessionValue
{
	UserId,
	ObjectId,
	SectorId,
	Nickname,
};