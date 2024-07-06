#include "PacketHandler.h"

/*
void PacketHandler::OnUserGameEnterReq(UniversalSessionInfo* const session_info, const PacketHeader& header, const packet_game_enter_request_rq& packet)
{
	
	auto iter = _agency->_user_nickname_set.find(packet.nickname);

	if (iter != _agency->_user_nickname_set.end())
	{
		// error
	}

	_agency->_user_nickname_set.insert(packet.nickname);

	const SocketSessionPtr& play_session = _agency->_sector_posting_manager->GetSectorOwnerSession(InitialSectorId);
	if (!play_session.Valid())
	{
		// error
	}

	uint64_t user_id = session_info->universal_session_id;

	_agency->_sector_posting_manager->AddSectorListener(InitialSectorId, session_info->universal_session_id);
	_agency->_sector_posting_manager->LinkSession(user_id, session_info->session);

	session_info->session->SetUserData(AgencySessionValue::SectorId, InitialSectorId);
	session_info->session->SetUserData(AgencySessionValue::Nickname, reinterpret_cast<uint64_t>(new std::string(packet.nickname)));
	session_info->session->SetUserData(AgencySessionValue::UserId, user_id);

	packet_spawn_character_rq request;
	request.object_info.sector_id = InitialSectorId;
	request.object_info.fixture.position = InitialCharacterPosition;
	request.object_info.fixture.size = Size{ 100, 100 };
	request.user_id = user_id;

	_agency->_packet_throttler->PostPacket(play_session, request, PacketPostingPolicy::Throttle);
	

}
*/

void PacketHandler::OnUserMoveReq(UniversalSessionInfo* const session_info, const PacketHeader& header, packet_character_move_rq& packet)
{
	uint64_t current_sector_id = session_info->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::SectorId);
	uint64_t object_id = session_info->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::ObjectId);

	const SocketSessionPtr& play_session = _agency->_sector_posting_manager->GetSectorOwnerSession(current_sector_id);
	if (!play_session.Valid())
	{
		LOG(LogLevel::Error, "play server session not found %llu", current_sector_id);
		session_info->session->CloseSession();
		return;
	}

	packet.sector_id = current_sector_id;
	packet.object_id = object_id;

	packet.to_owner = true;

	_agency->_packet_throttler->PostPacket(play_session, packet, PacketPostingPolicy::Immediate);
}

void PacketHandler::OnPlayMoveCharacterRes(IntraServerInfo* const server, const PacketHeader& header, const packet_character_move_rs& packet)
{
	if (header.error_code != ErrorCode::None)
	{
		LOG(LogLevel::Warn, "failed to process move packet, %llu", packet.object_id);
		return;
	}

	auto session_iter = _agency->_object_sessions.find(packet.object_id);

	if (session_iter == _agency->_object_sessions.end())
	{
		LOG(LogLevel::Warn, "user session is already closed, %llu", packet.object_id);
		return;
	}

	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.sector_id];
	Fixture& fixture = sector_objects[packet.object_id];
	fixture.direction = packet.direction;
	fixture.position = packet.starting_position;
	fixture.last_transform_time = _agency->_current_epoch_timestamp.count();

	session_iter->second->session->Send(packet);
	
	packet_object_move_ps response;
	response.object_id = packet.object_id;
	response.sector_id = packet.sector_id;
	response.direction = packet.direction;
	response.starting_position = packet.starting_position;

	_agency->_sector_posting_manager->SendToSectorWithout(session_iter->second->universal_session_id, response, packet.sector_id);
}

void PacketHandler::OnPlaySpawnCharacterRes(IntraServerInfo* const server, const PacketHeader& header, packet_spawn_character_rs& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.object_info.sector_id];
	packet.object_info.fixture.last_transform_time = _agency->_current_epoch_timestamp.count();

	sector_objects[packet.object_info.object_id] = packet.object_info.fixture;

	auto iter = _agency->_active_sessions.find(packet.user_id);
	if (iter == _agency->_active_sessions.end())
	{
		LOG(LogLevel::Warn, "character is spawned successfully, but user offline %llu, %llu", packet.user_id, packet.object_info.sector_id);

		packet_remove_character_rs request;
		request.object_id = packet.object_info.object_id;
		request.sector_id = packet.object_info.sector_id;
		request.user_id = packet.user_id;
		
		server->session->Send(request);

		return;
	}

	const SocketSessionPtr& client_session = iter->second.session;

	client_session->SetUserData(AgencySessionValue::SectorId, packet.object_info.sector_id);
	client_session->SetUserData(AgencySessionValue::Nickname, reinterpret_cast<uint64_t>(new std::string(packet.nickname)));
	client_session->SetUserData(AgencySessionValue::UserId, packet.user_id);
	client_session->SetUserData(AgencySessionValue::ObjectId, packet.object_info.object_id);

	_agency->_object_sessions[packet.object_info.object_id] = &iter->second;

	packet_create_object_ps broadcast;
	broadcast.object_info = packet.object_info;

	_agency->_sector_posting_manager->SendToSector(broadcast, packet.object_info.sector_id);

	_agency->_sector_posting_manager->AddSectorListener(packet.object_info.sector_id, packet.user_id);

	packet_game_enter_request_rs user_response;
	user_response.nickname = packet.nickname;
	user_response.object_info = packet.object_info;

	client_session->Send(user_response);

	packet_object_list_ps object_push = MakeObjectListPacket(packet.object_info.sector_id);
	client_session->Send(object_push);

}

void PacketHandler::OnUserStartSkillReq(UniversalSessionInfo* const session_info, const PacketHeader& header, const packet_start_skill_rq& packet)
{

}

void PacketHandler::OnPlayObjectStartSkillPush(IntraServerInfo* const server, const PacketHeader& header, const packet_object_start_skill_ps& packet)
{

}



void PacketHandler::OnPlayRemoveCharacterRes(IntraServerInfo* const session_info, const PacketHeader& header, const packet_remove_character_rs& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.sector_id];

	sector_objects.erase(packet.object_id);

	packet_remove_object_ps response;
	response.object_id = packet.object_id;
	response.sector_id = packet.sector_id;
	response.reason = 0;

	_agency->_sector_posting_manager->SendToSector(response, packet.sector_id);
}

void PacketHandler::OnPlayObjectListPush(IntraServerInfo* const server, const PacketHeader& header, packet_object_list_ps& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.sector_id];
	int64_t now = utility::CurrentTimeEpoch<utility::Milliseconds>().count();

	sector_objects.clear();

	for (auto& object : packet.object_list)
	{
		object.fixture.last_transform_time = now;
		sector_objects[object.object_id] = object.fixture;
	}

	_agency->_sector_posting_manager->SendToSector(packet, packet.sector_id);
}

void PacketHandler::OnPlayObjectMovePush(IntraServerInfo* const server, const PacketHeader& header, packet_object_move_ps& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.sector_id];

	Fixture& cache = sector_objects[packet.object_id];
	
	cache.direction = packet.direction;
	cache.position = packet.starting_position;
	cache.last_transform_time = _agency->_current_epoch_timestamp.count();
	_agency->_sector_posting_manager->SendToSector(packet, packet.sector_id);
}

void PacketHandler::OnPlayObjectCreatePush(IntraServerInfo* const server, const PacketHeader& header, const packet_create_object_ps& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.object_info.sector_id];

	Fixture& cache = sector_objects[packet.object_info.object_id];

	cache = packet.object_info.fixture;
	cache.last_transform_time = _agency->_current_epoch_timestamp.count();

	_agency->_sector_posting_manager->SendToSector(packet, packet.object_info.sector_id);
}

void PacketHandler::OnPlayObjectRemovePush(IntraServerInfo* const server, const PacketHeader& header, const packet_remove_object_ps& packet)
{
	std::unordered_map<uint64_t, Fixture>& sector_objects = _object_cache[packet.sector_id];

	sector_objects.erase(packet.object_id);

	_agency->_sector_posting_manager->SendToSector(packet, packet.sector_id);
}

void PacketHandler::OnPlayPromoteObjectReq(IntraServerInfo* const server, const PacketHeader& header, const packet_promote_object_rq& packet)
{
	const SocketSessionPtr& departing_play_session = _agency->_sector_posting_manager->GetSectorOwnerSession(packet.object_info.sector_id);
	const SocketSessionPtr& origin_play_session = _agency->_sector_posting_manager->GetSectorOwnerSession(packet.origin_sector_id);

	if (!departing_play_session.Valid() || !origin_play_session.Valid())
	{
		LOG(LogLevel::Warn, "failed to promote object, session connectivity is unstable");
		// _object_cache[packet.origin_sector_id].erase(packet.object_info.object_id);
		// _object_cache[packet.object_info.object_id].erase(packet.object_info.object_id);

		return;
	}

	std::unordered_map<uint64_t, Fixture>& origin_sector_objects = _object_cache[packet.origin_sector_id];
	auto origin_object = origin_sector_objects.find(packet.object_info.object_id);
	if (origin_object == origin_sector_objects.end())
	{
		LOG(LogLevel::Warn, "failed to promote object, origin object not found");
		return;
	}

	std::unordered_map<uint64_t, Fixture>& departing_sector_objects = _object_cache[packet.object_info.sector_id];

	departing_sector_objects[origin_object->first] = packet.object_info.fixture;

	auto user_session = _agency->_object_sessions.find(packet.object_info.object_id);
	if (user_session != _agency->_object_sessions.end())
	{
		uint64_t current_sector_id = user_session->second->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::SectorId);

		assert(current_sector_id == packet.origin_sector_id);

		packet_object_list_ps object_push = MakeObjectListPacket(packet.object_info.sector_id);

		user_session->second->session->SetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::SectorId, packet.object_info.sector_id);
		
		user_session->second->session->Send(object_push);

		_agency->_sector_posting_manager->RemoveSectorListener(packet.origin_sector_id, user_session->second->universal_session_id);
		_agency->_sector_posting_manager->AddSectorListener(packet.object_info.sector_id, user_session->second->universal_session_id);
	}
}

packet_object_list_ps PacketHandler::MakeObjectListPacket(const uint64_t sector_id)
{
	auto& sector_objects = _object_cache[sector_id];

	packet_object_list_ps packet;
	packet.sector_id = sector_id;
	for (const auto& pair : sector_objects)
	{
		PacketObjectInfo object;
		object.fixture = pair.second;
		object.fixture.personal_delta_time = 0;

		object.object_id = pair.first;
		object.sector_id = pair.second.sector_id;
		object.type = ObjectType::Character;
		object.vital = {};

		if (object.fixture.direction != Direction::Max)
			object.fixture.personal_delta_time = static_cast<uint16_t>(_agency->_current_epoch_timestamp.count() - object.fixture.last_transform_time);

		packet.object_list.push_back(object);
	}

	return packet;
}