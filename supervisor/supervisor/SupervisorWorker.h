#pragma once

#include <engine/protocol/Packet.h>
#include <game/GameServer.h>
#include <game/coordinate/SectorGrid.h>
#include <game/object/Character.h>
#include <utility/Time.h>
#include <utility/Sampling.h>
#include <string>
#include <unordered_set>

struct ManagedSectorInfo : SectorInfo
{
	uint64_t owner_server_id;
};

struct ManagedServerInfo
{
	SocketSessionPtr session;
	ServerInfo server_info;
	std::unordered_set<uint64_t> sector_list;
	SessionKey key;
};

class SupervisorWorker : public NetworkEngineWorker
{
private:
	NetworkEngine* _engine;

	ServerInfo _server_info;
	ServerConfig _server_config;

	uint64_t _current_server_id = SUPERVISOR_SERVER_ID + 1;

	uint16_t _server_port = 11001;

	std::unordered_map<uint64_t, ManagedSectorInfo> _sector_info;
	std::unordered_map<uint64_t, ManagedServerInfo> _managed_server_list;

	std::unordered_map<uint64_t, ServerInfo> _blocked_server_list;
	std::unordered_map<uint64_t, ServerInfo> _recoverying_server_list;
	utility::Countdown<utility::Milliseconds> _recovery_countdown;

	utility::Countdown<utility::Milliseconds>* _initial_delay = nullptr;

	uint64_t GetNextServerId();

	void OnServerLoadServerConfig(const SocketSessionPtr& session, const packet_load_server_config_rq& packet);
	void OnServerRegisterMinionServerReq(const SocketSessionPtr& session, const packet_register_minion_server_rq& packet);
	void OnServerRecoveryMinionServerReq(const SocketSessionPtr& session, const packet_recovery_minion_server_rq& packet);

	packet_update_intra_server_info_ps MakeUpdateIntraServerPacket();
	packet_sector_allocation_ps MakeSectorAllocationPacket();

	template <NetworkPacketConcept Packet>
	void SendPacket(const SocketSessionPtr& session, const Packet& packet)
	{
		session->Send(packet);
	}

	template <NetworkPacketConcept Packet, NetworkPacketConcept... Packets>
	void SendPacket(const SocketSessionPtr& session, const Packet& packet, Packets... packets)
	{
		session->Send(packet);
		SendPacket(session, packets...);
	}

	template <NetworkPacketConcept... Packets>
	void BroadcastPacket(Packets... packets)
	{
		for (const std::pair<uint32_t, ManagedServerInfo>& server : _managed_server_list)
		{
			SendPacket(server.second.session, packets...);
		}
	}
public:

	SupervisorWorker(NetworkEngine* engine, const SocketAddress& my_address);

	virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) override;

	virtual void OnSocketSessionAccepted(SocketContext* context) override;
	virtual void OnSocketSessionClosed(SocketContext* context) override;
	virtual void OnSocketSessionData(SocketContext* context) override;
};

enum class SessionDataKey: uint64_t
{
	ServerId,
	ServerType,
};

