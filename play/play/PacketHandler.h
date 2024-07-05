#pragma once

#include "PlayWorker.h"
#include <engine/SocketSession.h>
#include <engine/SocketContext.h>
#include <engine/protocol/Packet.h>

class PacketHandler
{
private:
	PlayWorker* _play;

public:

	PacketHandler(PlayWorker* play) : _play(play) {}

	void OnAgencySpawnCharacterReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_spawn_character_rq& packet);
	void OnAgencyRemoveCharacterReq(IntraServerInfo* server_session, const PacketHeader& header, packet_remove_character_rq& packet);
	void OnAgencyUserMoveReq(IntraServerInfo* server_session, const PacketHeader& header, packet_character_move_rq& packet);
	
	void OnPlayCreateObservingObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_create_observing_object_rq& packet);
	void OnPlayRemoveObservingObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_remove_observing_object_rq& packet);
	void OnPlayPromoteObjectReq(IntraServerInfo* server_session, const PacketHeader& header, const packet_promote_object_rq& packet);

};