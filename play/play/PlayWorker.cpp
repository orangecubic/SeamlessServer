#include "PlayWorker.h"
#include "PacketHandler.h"
#include <engine/NetworkEngine.h>

bool PlayWorker::InitializeGameServer()
{
	_world = new World(_sector_posting_manager, utility::Milliseconds(_game_config.fixture_transform_cooltime_ms));
	return true;
}

void PlayWorker::SetPacketHandler(PacketHandler* const packet_handler)
{
	_packet_handler = packet_handler;
}

bool PlayWorker::Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time)
{
	_world->Update(current_time, delta_time);
	_packet_throttler->ForceFlushPacket();

	if (delta_time.count() > 100)
	{
		LOG(LogLevel::Warn, "delta time is upper than %lld", delta_time.count());
	}
	return true;
}

void PlayWorker::OnSupervisorResetMinionServer()
{
	_world->ClearSector();
}

void PlayWorker::OnSupervisorSectorAllocation(const packet_sector_allocation_ps& packet)
{
	for (const SectorAllocationInfo& info : packet.deallocation_info)
	{
		if (info.server_info.server_id == _my_server_info.server_id)
		{
			_world->RemoveSector(info.sector_id);
		}
	}

	for (const SectorAllocationInfo& info : packet.allocation_info)
	{
		if (info.server_info.server_id == _my_server_info.server_id)
		{
			_world->AddSector(info.server_info.server_id, info.sector_id);
		}
	}
}

void PlayWorker::OnIntraServerConnected(IntraServerInfo* server_session)
{

}

void PlayWorker::OnIntraServerAccepted(IntraServerInfo* server_session)
{
	if (server_session->server_info.server_type == ServerType::Agency)
	{
		packet_object_list_ps packet;

		const auto& all_sectors = _world->GetAllSectors();

		for (const auto& pair : all_sectors)
		{
			if (pair.second == nullptr)
				continue;

			const auto& all_objects = pair.second->GetAllFixtures();
			packet.sector_id = pair.first;

			for (const auto& fixture : all_objects)
			{
				// Object* object = reinterpret_cast<Object*>(fixture.second->user_data);

				PacketObjectInfo object_info;
				object_info.fixture = *fixture.second;
				object_info.object_id = fixture.second->id; // object->GetId();
				object_info.sector_id = pair.first;
				// object_info.vital = *object->GetVital();
				object_info.type = ObjectType::Character;

				packet.object_list.push_back(object_info);
			}

			_packet_throttler->PostPacket(server_session->session, packet, PacketPostingPolicy::Throttle);

			packet.object_list.clear();
		}
	}
}

void PlayWorker::OnIntraServerClosed(IntraServerInfo* server_session)
{

}

void PlayWorker::OnIntraServerData(IntraServerInfo* server_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer)
{
	switch (header.packet_type)
	{
		PACKET_CASE(server_session, packet_spawn_character_rq, _packet_handler->OnAgencySpawnCharacterReq);
		PACKET_CASE(server_session, packet_character_move_rq, _packet_handler->OnAgencyUserMoveReq);
		PACKET_CASE(server_session, packet_remove_character_rq, _packet_handler->OnAgencyRemoveCharacterReq);
		PACKET_CASE(server_session, packet_create_observing_object_rq, _packet_handler->OnPlayCreateObservingObjectReq);
		PACKET_CASE(server_session, packet_remove_observing_object_rq, _packet_handler->OnPlayRemoveObservingObjectReq);
		PACKET_CASE(server_session, packet_promote_object_rq, _packet_handler->OnPlayPromoteObjectReq);
	}
}
