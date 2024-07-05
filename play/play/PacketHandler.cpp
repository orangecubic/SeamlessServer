#include "PacketHandler.h"
#include <utility/Union.h>
#include <utility/Logger.h>
#include <engine/NetworkEngine.h>

void PacketHandler::OnAgencySpawnCharacterReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_spawn_character_rq& packet)
{
	_play->_world->SpawnCharacter(server_session->session, server_session->server_info.server_id, packet);
}

void PacketHandler::OnAgencyRemoveCharacterReq(IntraServerInfo* server_session, const PacketHeader& header, packet_remove_character_rq& packet)
{
	_play->_world->RemoveCharacter(server_session->session, server_session->server_info.server_id, packet);
}

void PacketHandler::OnAgencyUserMoveReq(IntraServerInfo* server_session, const PacketHeader& header, packet_character_move_rq& packet)
{
	_play->_world->MoveObject(server_session->session, server_session->server_info.server_id, packet);
}

void PacketHandler::OnPlayCreateObservingObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_create_observing_object_rq& packet)
{
	_play->_world->CreateObservingObject(packet);
}

void PacketHandler::OnPlayRemoveObservingObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_remove_observing_object_rq& packet)
{
	_play->_world->RemoveObservingObject(packet);
}

void PacketHandler::OnPlayPromoteObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_promote_object_rq& packet)
{
	_play->_world->PromoteObservingObject(server_session->session, server_session->server_info.server_id, packet);
}