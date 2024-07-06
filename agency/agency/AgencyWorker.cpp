#include "AgencyWorker.h"
#include "PacketHandler.h"
#include <engine/protocol/Protocol.h>
#include <engine/SocketContext.h>
#include <utility/Logger.h>
#include <sstream>

constexpr uint64_t InitialSectorId = CenterSector.sector_id;
constexpr Vector2 InitialCharacterPosition = GetSectorFixtureRectangle(InitialSectorId).Center();

void AgencyWorker::SetPacketHandler(PacketHandler* packet_handler)
{
	_packet_handler = packet_handler;
}

bool AgencyWorker::Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time)
{
	_packet_throttler->ForceFlushPacket();

	if (delta_time.count() > 100)
	{
		LOG(LogLevel::Warn, "delta time is upper than %lld", delta_time.count());
	}

	return true;
}

thread_local std::stringstream string_stream;

void AgencyWorker::OnClientAccepted(UniversalSessionInfo* client_session)
{
	string_stream << _my_server_info.server_id << "_" << client_session->universal_session_id;

	std::string nickname = string_stream.str();

	string_stream.str("");

	const SocketSessionPtr& play_session = _sector_posting_manager->GetSectorOwnerSession(InitialSectorId);
	if (!play_session.Valid())
	{
		LOG(LogLevel::Warn, "sector owner not found %llu", InitialSectorId);
		client_session->session->CloseSession();
		return;
	}

	uint64_t user_id = client_session->universal_session_id;

	_sector_posting_manager->LinkSession(user_id, client_session->session);

	packet_spawn_character_rq request;
	request.object_info.sector_id = InitialSectorId;
	request.object_info.fixture.position = InitialCharacterPosition;
	request.object_info.fixture.size = Size{ 100, 100 };
	request.user_id = user_id;

	_packet_throttler->PostPacket(play_session, request, PacketPostingPolicy::Throttle);
}

void AgencyWorker::OnClientClosed(UniversalSessionInfo* client_session)
{
	uint64_t object_id = client_session->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::ObjectId);
	uint64_t sector_id = client_session->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::SectorId);

	a1++;

	// not entered user
	if (object_id == 0)
		return;

	a2++;

	std::string* nickname = client_session->session->GetUserData<AgencySessionValue, std::string*>(AgencySessionValue::Nickname);

	packet_remove_character_rq packet;

	packet.user_id = client_session->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::UserId);
	packet.object_id = object_id;
	packet.sector_id = sector_id;

	_sector_posting_manager->SendToSectorOwner(packet, sector_id);

	_sector_posting_manager->UnsetAll(client_session->universal_session_id);
	_object_sessions.erase(packet.object_id);

	delete nickname;
}

void AgencyWorker::OnIntraServerConnected(IntraServerInfo* server_session)
{

}

void AgencyWorker::OnIntraServerAccepted(IntraServerInfo* server_session)
{

}

void AgencyWorker::OnIntraServerClosed(IntraServerInfo* server_session)
{
	if (server_session->server_info.server_type == ServerType::Play)
	{
		_sector_posting_manager->GetOwnershipSectors(server_session->server_info.server_id);
	}
}

void AgencyWorker::OnClientData(UniversalSessionInfo* client_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer)
{
	uint64_t object_id = client_session->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::ObjectId);
	uint64_t current_sector_id = client_session->session->GetUserData<AgencySessionValue, uint64_t>(AgencySessionValue::SectorId);

	if (object_id == 0)
	{
		LOG(LogLevel::Warn, "not authorized user %llu", client_session->universal_session_id);
		client_session->session->CloseSession();
		return;
	}

	switch (header.packet_type)
	{
		// PACKET_CASE(client_session, packet_game_enter_request_rq, _packet_handler->OnUserGameEnterReq);
		PACKET_CASE(client_session, packet_character_move_rq, _packet_handler->OnUserMoveReq);
	}
}

void AgencyWorker::OnIntraServerData(IntraServerInfo* server_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer)
{
	switch (header.packet_type)
	{
		PACKET_CASE(server_session, packet_spawn_character_rs, _packet_handler->OnPlaySpawnCharacterRes);
		PACKET_CASE(server_session, packet_remove_character_rs, _packet_handler->OnPlayRemoveCharacterRes);
		PACKET_CASE(server_session, packet_character_move_rs, _packet_handler->OnPlayMoveCharacterRes);
		PACKET_CASE(server_session, packet_create_object_ps, _packet_handler->OnPlayObjectCreatePush);
		PACKET_CASE(server_session, packet_remove_object_ps, _packet_handler->OnPlayObjectRemovePush);
		PACKET_CASE(server_session, packet_object_move_ps, _packet_handler->OnPlayObjectMovePush);
		PACKET_CASE(server_session, packet_promote_object_rq, _packet_handler->OnPlayPromoteObjectReq);
	}
}