#pragma once

#include <game/GameServer.h>
#include <game/object/Character.h>
#include <game/World.h>
#include <map>

class PacketHandler;

class PlayWorker : public GameServer
{
private:
	PacketHandler* _packet_handler;

	World* _world;

	friend PacketHandler;
public:

	bool InitializeGameServer() override;

	void SetPacketHandler(PacketHandler* const packet_handler);

	virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) override;

	virtual void OnSupervisorResetMinionServer() override;

	virtual void OnSupervisorSectorAllocation(const packet_sector_allocation_ps& packet) override;

	virtual void OnIntraServerConnected(IntraServerInfo* server_session) override;
	virtual void OnIntraServerAccepted(IntraServerInfo* server_session) override;
	virtual void OnIntraServerClosed(IntraServerInfo* server_session) override;

	virtual void OnIntraServerData(IntraServerInfo* server_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer) override;
};

struct ConnectorAttachment
{
	uint64_t server_id;
	ServerType server_type;
};

enum class PlaySessionValue
{
	ServerId,
};