#pragma once

#include "AgencyWorker.h"
#include <engine/SocketSession.h>
#include <engine/SocketContext.h>
#include <engine/protocol/Packet.h>

struct ObjectCache
{
	Fixture fixture;
	ObjectType object_type;
};

class PacketHandler
{
private:
	AgencyWorker* _agency;

	std::unordered_map <uint64_t, std::unordered_map<uint64_t, Fixture>> _object_cache;

	packet_object_list_ps MakeObjectListPacket(const uint64_t sector_id);
public:

	PacketHandler(AgencyWorker* agency) : _agency(agency) {}

	// void OnUserGameEnterReq(UniversalSessionInfo* const session_info, const PacketHeader& header, const packet_game_enter_request_rq& packet);

	void OnUserMoveReq(UniversalSessionInfo* const session_info, const PacketHeader& header, packet_character_move_rq& packet);
	void OnUserStartSkillReq(UniversalSessionInfo* const session_info, const PacketHeader& header, const packet_start_skill_rq& packet);

	// user action packet
	void OnPlaySpawnCharacterRes(IntraServerInfo* const session_info, const PacketHeader& header, packet_spawn_character_rs& packet);
	void OnPlayRemoveCharacterRes(IntraServerInfo* const session_info, const PacketHeader& header, const packet_remove_character_rs& packet);
	void OnPlayMoveCharacterRes(IntraServerInfo* const server, const PacketHeader& header, const packet_character_move_rs& packet);
	
	// user action packet

	// on connect play server packet
	void OnPlayObjectListPush(IntraServerInfo* const server, const PacketHeader& header, packet_object_list_ps& packet);

	// other object packet
	void OnPlayObjectMovePush(IntraServerInfo* const server, const PacketHeader& header, packet_object_move_ps& packet);
	void OnPlayObjectStartSkillPush(IntraServerInfo* const server, const PacketHeader& header, const packet_object_start_skill_ps& packet);
	void OnPlayObjectCreatePush(IntraServerInfo* const server, const PacketHeader& header, const packet_create_object_ps& packet);
	void OnPlayObjectRemovePush(IntraServerInfo* const server, const PacketHeader& header, const packet_remove_object_ps& packet);
	void OnPlayPromoteObjectReq(IntraServerInfo* const server, const PacketHeader& header, const packet_promote_object_rq& packet);

};