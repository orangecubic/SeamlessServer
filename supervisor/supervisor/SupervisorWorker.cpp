#include "SupervisorWorker.h"
#include <engine/SocketContext.h>
#include <algorithm>
#include <utility/Logger.h>
#include "Config.h"
#include <engine/PacketThrottler.h>
#include <game/GameServer.h>
#include <utility/Logger.h>

SupervisorWorker::SupervisorWorker(NetworkEngine* engine, const SocketAddress& my_address) : _engine(engine), _recovery_countdown(utility::Milliseconds(9999999999999))
{
	_server_info.address = my_address;
	_server_info.server_id = SUPERVISOR_SERVER_ID;
	_server_info.server_type = ServerType::Supervisor;

	for (const SectorGridRow& grid_row : SectorGrid)
	{
		for (const SectorInfo sector : grid_row)
		{
			_sector_info[sector.sector_id] = ManagedSectorInfo{ sector.sector_id, sector.grid_index, 0 };
		}
	}
}

packet_update_intra_server_info_ps SupervisorWorker::MakeUpdateIntraServerPacket()
{
	packet_update_intra_server_info_ps packet;

	for (const std::pair<uint64_t, ManagedServerInfo>& server_pair : _managed_server_list)
	{
		packet.joined_server_info.push_back(server_pair.second.server_info);
	}

	return packet;
}
packet_sector_allocation_ps SupervisorWorker::MakeSectorAllocationPacket()
{
	packet_sector_allocation_ps packet;

	for (const std::pair<uint64_t, ManagedServerInfo>& server_pair : _managed_server_list)
	{
		for (const uint64_t sector_id : server_pair.second.sector_list)
		{
			packet.allocation_info.push_back(SectorAllocationInfo{ server_pair.second.server_info, sector_id });
		}
	}

	return packet;
}

uint64_t SupervisorWorker::GetNextServerId()
{
	while (true)
	{
		uint64_t server_id = _current_server_id++;

		if (_managed_server_list.find(server_id) == _managed_server_list.end() && _blocked_server_list.find(server_id) == _blocked_server_list.end() && _recoverying_server_list.find(server_id) == _recoverying_server_list.end())
			return server_id;
	}
}

bool SupervisorWorker::Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time)
{
	if (_initial_delay == nullptr || !(*_initial_delay))
		return true;

	if (_recovery_countdown)
	{
		packet_update_intra_server_info_ps packet;

		for (const auto& recovering_server_info : _recoverying_server_list)
		{
			_blocked_server_list[recovering_server_info.first] = recovering_server_info.second;
			packet.leaved_server_info.push_back(recovering_server_info.second);
		}

		_recoverying_server_list.clear();

		BroadcastPacket(packet);

		_recovery_countdown = utility::Countdown<utility::Milliseconds>(utility::Milliseconds(999999999999));
	}

	packet_sector_allocation_ps sector_allocation_packet;

	for (std::pair<const uint64_t, ManagedSectorInfo>& sector_pair : _sector_info)
	{
		if (sector_pair.second.owner_server_id != 0)
			continue;

		size_t current_sector_count = UINT32_MAX;
		ManagedServerInfo* server = nullptr;

		std::for_each(_managed_server_list.begin(), _managed_server_list.end(), [&](auto& data) {
			if (data.second.server_info.server_type == ServerType::Agency)
				return;

			if (current_sector_count > data.second.sector_list.size())
			{
				current_sector_count = data.second.sector_list.size();
				server = &data.second;
			}
		});

		// 연결된 play 서버가 없음
		if (server == nullptr)
			return true;

		// 서버당 sector 수 초과
		if (server->sector_list.size() >= MaxSectorPerPlayServer)
			continue;

		server->sector_list.insert(sector_pair.first);
		sector_allocation_packet.allocation_info.push_back(SectorAllocationInfo{server->server_info, sector_pair.first});
		sector_pair.second.owner_server_id = server->server_info.server_id;
	}

	if (!sector_allocation_packet.allocation_info.empty())
		BroadcastPacket(sector_allocation_packet);

	return true;
}

void SupervisorWorker::OnSocketSessionAccepted(SocketContext* context)
{
	
}
void SupervisorWorker::OnSocketSessionClosed(SocketContext* context)
{
	uint64_t server_id = context->session->GetUserData<SessionDataKey, uint64_t>(SessionDataKey::ServerId);
	if (server_id == 0)
		return;
	
	auto iterator = _managed_server_list.find(server_id);

	assert(iterator != _managed_server_list.end());

	packet_update_intra_server_info_ps intra_server_packet;
	intra_server_packet.leaved_server_info.push_back(iterator->second.server_info);

	packet_sector_allocation_ps sector_allocation_packet;

	for (const uint64_t sector_id : iterator->second.sector_list)
	{
		sector_allocation_packet.deallocation_info.push_back({iterator->second.server_info, sector_id});
		_sector_info[sector_id].owner_server_id = 0;
	}
	
	_managed_server_list.erase(server_id);
	BroadcastPacket(sector_allocation_packet, intra_server_packet);

}

void SupervisorWorker::OnSocketSessionData(SocketContext* context)
{
	uint64_t server_id = context->session->GetUserData<SessionDataKey, uint64_t>(SessionDataKey::ServerId);
	if (server_id != 0)
	{
		LOG(LogLevel::Warn, "try register duplicate server_id: %llu", server_id);
		context->session->Disconnect();
		return;
	}

	switch (context->header.packet_type)
	{
	case PacketType::packet_load_server_config_rq: {
		packet_load_server_config_rq packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			std::string format_string = std::string("failed to deserialize ") + typeid(packet_load_server_config_rq).name() + " packet, session: %llu";

			LOG(LogLevel::Warn, format_string.c_str(), context->session->GetSessionId());
			context->session->Disconnect();
			return;
		}

		OnServerLoadServerConfig(context->session, packet);
		break;
	}
	case PacketType::packet_register_minion_server_rq: {
		packet_register_minion_server_rq packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			std::string format_string = std::string("failed to deserialize ") + typeid(packet_register_minion_server_rq).name() + " packet, session: %llu";

			LOG(LogLevel::Warn, format_string.c_str(), context->session->GetSessionId());
			context->session->Disconnect();
			return;
		}
		OnServerRegisterMinionServerReq(context->session, packet);
		break;
	}
	case PacketType::packet_recovery_minion_server_rq: {
		packet_recovery_minion_server_rq packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			std::string format_string = std::string("failed to deserialize ") + typeid(packet_register_minion_server_rq).name() + " packet, session: %llu";

			LOG(LogLevel::Warn, format_string.c_str(), context->session->GetSessionId());
			context->session->Disconnect();
			return;
		}
		OnServerRecoveryMinionServerReq(context->session, packet);
		break;
	}
	}
}

void SupervisorWorker::OnServerLoadServerConfig(const SocketSessionPtr& session, const packet_load_server_config_rq& packet)
{
	packet_load_server_config_rs response;
	switch (packet.server_type)
	{
	case ServerType::Agency:
		response.server_config = AgencyServerConfig;
		// response.server_config.user_listen_port = _server_port++;
		break;
	case ServerType::Play:
		response.server_config = PlayServerConfig;
		response.server_config.intra_server_listen_port = _server_port++;
		break;
	}
	 
	
	session->Send(response);
}

void SupervisorWorker::OnServerRegisterMinionServerReq(const SocketSessionPtr& session, const packet_register_minion_server_rq& packet)
{
	if (session->GetUserData<SessionDataKey, uint64_t>(SessionDataKey::ServerId) != 0)
	{
		session->Disconnect();
		return;
	}

	uint64_t server_id = GetNextServerId();

	session->SetUserData(SessionDataKey::ServerId, server_id);
	session->SetUserData(SessionDataKey::ServerType, packet.server_type);

	ManagedServerInfo managed_server_info;

	managed_server_info.server_info.server_id = server_id;
	managed_server_info.server_info.server_type = packet.server_type;
	managed_server_info.server_info.address = packet.address;
	managed_server_info.session = session;

	packet_register_minion_server_rs response;
	packet_update_intra_server_info_ps intra_server_packet = MakeUpdateIntraServerPacket();
	packet_sector_allocation_ps sector_allocation_packet = MakeSectorAllocationPacket();

	response.server_info = managed_server_info.server_info;

	session->Send(response);
	session->Send(intra_server_packet);
	session->Send(sector_allocation_packet);

	intra_server_packet.joined_server_info.clear();
	intra_server_packet.joined_server_info.push_back(managed_server_info.server_info);

	BroadcastPacket(intra_server_packet);
	_managed_server_list[server_id] = managed_server_info;

	if (_initial_delay == nullptr)
		_initial_delay = new utility::Countdown<utility::Milliseconds>(utility::Milliseconds(2000));
}

void SupervisorWorker::OnServerRecoveryMinionServerReq(const SocketSessionPtr& session, const packet_recovery_minion_server_rq& packet)
{
	packet_recovery_minion_server_rs response;
	
	if (packet.server_info.server_id <= SUPERVISOR_SERVER_ID)
	{
		LOG(LogLevel::Warn, "recovery failed, server_id  %llu must less than SUPERVISOR_SERVER_ID + 1", packet.server_info.server_id);
		session->Send(response, ErrorCode::RecoveryFailed);
		return;
	}

	auto server_iterator = _managed_server_list.find(packet.server_info.server_id);
	if (server_iterator != _managed_server_list.end())
	{
		LOG(LogLevel::Warn, "recovery failed, duplicate server_id: %llu, session_id: %llu", packet.server_info.server_id, session->GetSessionId());
		session->Send(response, ErrorCode::RecoveryFailed);
		return;
	}

	if (packet.server_info.server_type != ServerType::Play && !packet.sectors.empty())
	{
		LOG(LogLevel::Warn, "recovery failed, only play server can have sectors server_id: %llu, session_id: %llu", packet.server_info.server_id, session->GetSessionId());
		session->Send(response, ErrorCode::RecoveryFailed);
		return;
	}

	for (const uint64_t sector_id : packet.sectors)
	{
		auto sector_iterator = _sector_info.find(sector_id);
		if (sector_iterator == _sector_info.end())
		{
			LOG(LogLevel::Warn, "recovery failed, not found sector id: %llu, server_id: %llu, session_id: %llu", sector_id, packet.server_info.server_id, session->GetSessionId());
			session->Send(response, ErrorCode::RecoveryFailed);
			return;
		}

		if (sector_iterator->second.owner_server_id != 0)
		{
			LOG(LogLevel::Warn, "recovery failed, already owned sector id: %llu, server_id: %llu, session_id: %llu", sector_id, packet.server_info.server_id, session->GetSessionId());
			session->Send(response, ErrorCode::RecoveryFailed);
			return;
		}
	}

	if (_blocked_server_list.find(packet.server_info.server_id) != _blocked_server_list.end())
	{
		LOG(LogLevel::Warn, "recovery failed, blocked server id beacause of timeout server_id: %llu, session_id: %llu", packet.server_info.server_id, session->GetSessionId());
		session->Send(response, ErrorCode::RecoveryFailed);
		return;
	}

	packet_update_intra_server_info_ps intra_server_packet;
	packet_sector_allocation_ps sector_allocation_packet;

	_recoverying_server_list.erase(packet.server_info.server_id);

	for (const auto& recoveried_server_info : packet.intra_servers)
	{
		if (_managed_server_list.find(recoveried_server_info.server_id) == _managed_server_list.end())
		{
			_recoverying_server_list[recoveried_server_info.server_id] = recoveried_server_info;
			_recovery_countdown = utility::Countdown<utility::Milliseconds>(utility::Milliseconds(30000));
		}
	}

	ManagedServerInfo& server = _managed_server_list[packet.server_info.server_id];

	for (const uint64_t sector_id : packet.sectors)
	{
		ManagedSectorInfo& sector_info = _sector_info[sector_id];

		sector_info.owner_server_id = packet.server_info.server_id;

		sector_allocation_packet.allocation_info.push_back({ packet.server_info, sector_id });
	}

	session->SetUserData(SessionDataKey::ServerId, packet.server_info.server_id);
	session->SetUserData(SessionDataKey::ServerType, packet.server_info.server_type);

	server.server_info = packet.server_info;
	server.session = session;
	
	intra_server_packet.joined_server_info.push_back(packet.server_info);

	session->Send(packet_recovery_minion_server_rs{});
	BroadcastPacket(intra_server_packet, sector_allocation_packet);

	if (_initial_delay == nullptr)
		_initial_delay = new utility::Countdown<utility::Milliseconds>(utility::Milliseconds(5000));
}