#pragma once

#include "../../common.h"
#include "../../game/coordinate/Fixture.h"
#include "../../game/object/Vital.h"
#include "../Session.h"
#include "PacketEnum.h"
#include "PacketStruct.h"
#include <xmemory>
#include <concepts>

enum class PacketType : uint16_t
{
	packet_heartbeat_rq,
	packet_heartbeat_rs,
	packet_session_create_rq,
	packet_session_create_rs,
	packet_session_auth_rq,
	packet_session_auth_rs,
	packet_session_close_rq,
	packet_bandwith_overflow_ps,
	packet_server_is_busy_ps,

	packet_game_enter_request_rq,
	packet_game_enter_request_rs,
	packet_enter_new_character_ps,
	packet_object_list_ps,
	packet_object_move_ps,
	packet_create_object_ps,
	packet_remove_object_ps,
	packet_object_start_skill_ps,
	packet_character_move_rq,
	packet_character_move_rs,
	packet_start_skill_rq,
	packet_start_skill_rs,

	packet_load_server_config_rq,
	packet_load_server_config_rs,
	packet_register_minion_server_rq,
	packet_register_minion_server_rs,
	packet_authorize_server_rq,
	packet_authorize_server_rs,
	packet_update_intra_server_info_ps,
	packet_sector_allocation_ps,
	packet_recovery_minion_server_rq,
	packet_recovery_minion_server_rs,

	packet_spawn_character_rq,
	packet_spawn_character_rs,
	packet_remove_character_rq,
	packet_remove_character_rs,
	packet_create_observing_object_rq,
	packet_remove_observing_object_rq,
	packet_promote_object_rq,
};


template <typename T>
concept NetworkPacketConcept = requires {
	std::same_as<decltype(T::PACKET_TYPE), PacketType> && !(std::is_empty_v<T>);
};

using PacketLengthType = uint32_t;

struct PacketHeader
{
	PacketType packet_type;
	ErrorCode error_code;
	PacketLengthType body_size;
};

// struct packing 방법에 따라 헤더 사이즈가 맞지 않을수도 있지만 보류
constexpr PacketLengthType PACKET_LENGTH_SIZE = sizeof(PacketLengthType);
constexpr PacketLengthType PACKET_HEADER_SIZE = sizeof(PacketHeader);

////////////////////////////////////////////////
// general protocol
////////////////////////////////////////////////
struct packet_heartbeat_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_heartbeat_rq;
	uint32_t ping;
};

struct packet_heartbeat_rs 
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_heartbeat_rs;
	uint8_t _1;
};

struct packet_session_create_rq 
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_session_create_rq;
	uint8_t _1;
};

struct packet_session_create_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_session_create_rs;
	uint64_t session_id;
	std::array<uint8_t, SessionKeySize>  session_key;
};

struct packet_session_auth_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_session_auth_rq;
	uint64_t session_id;
	SessionKey::BufferType session_key;
};

struct packet_session_auth_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_session_auth_rs;
	uint8_t _1;
};

struct packet_session_close_rq 
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_session_close_rq;
	uint8_t _1;
};

struct packet_bandwith_overflow_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_bandwith_overflow_ps;
	uint8_t _1;
};

struct packet_server_is_busy_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_server_is_busy_ps;
	uint8_t _1;
};

////////////////////////////////////////////////
// client to server
////////////////////////////////////////////////
struct packet_game_enter_request_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_game_enter_request_rq;
	std::string nickname;
};

struct packet_game_enter_request_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_game_enter_request_rs;
	std::string nickname;
	PacketObjectInfo object_info;
};

struct packet_object_list_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_object_list_ps;
	uint64_t sector_id;
	std::vector<PacketObjectInfo> object_list;
};

struct packet_create_object_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_create_object_ps;
	PacketObjectInfo object_info;
};

struct packet_remove_object_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_remove_object_ps;
	uint64_t sector_id;
	uint64_t object_id;
	uint8_t reason;
};

struct packet_enter_new_character_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_enter_new_character_ps;
	std::string nickname;
	PacketObjectInfo object_info;
};

struct packet_object_move_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_object_move_ps;
	uint64_t sector_id;
	uint64_t object_id;
	Vector2 starting_position;

	Direction direction;
};

struct packet_object_start_skill_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_object_start_skill_ps;
	uint64_t object_id;
	Vector2 start;

	Direction direction;
	uint64_t skill_id;
};

struct packet_character_move_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_character_move_rq;
	uint64_t sector_id;
	uint64_t object_id;
	Vector2 starting_position;
	Direction direction;

	bool to_owner;
};

struct packet_character_move_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_character_move_rs;
	uint64_t sector_id;
	uint64_t object_id;
	Vector2 starting_position;
	Direction direction;
};

struct packet_start_skill_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_start_skill_rq;

	uint8_t _1;
};

struct packet_start_skill_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_start_skill_rs;

	uint8_t _1;
};

////////////////////////////////////////////////
// server to supervisor
////////////////////////////////////////////////

struct packet_load_server_config_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_load_server_config_rq;
	ServerType server_type;
};

struct packet_load_server_config_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_load_server_config_rs;
	ServerConfig server_config;
};

struct packet_register_minion_server_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_register_minion_server_rq;
	ServerType server_type;
	SocketAddress address;
};

struct packet_register_minion_server_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_register_minion_server_rs;
	ServerInfo server_info;

	std::array<uint8_t, SessionKeySize> auth_key;
};

struct packet_recovery_minion_server_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_recovery_minion_server_rq;
	std::array<uint8_t, SessionKeySize> auth_key;
	ServerInfo server_info;
	std::vector<uint64_t> sectors;
	std::vector<ServerInfo> intra_servers;
};

struct packet_recovery_minion_server_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_recovery_minion_server_rs;
	uint8_t _1;
};

struct packet_update_intra_server_info_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_update_intra_server_info_ps;
	std::vector<ServerInfo> joined_server_info;
	std::vector<ServerInfo> leaved_server_info;
};

struct packet_sector_allocation_ps
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_sector_allocation_ps;
	std::vector<SectorAllocationInfo> allocation_info;
	std::vector<SectorAllocationInfo> deallocation_info;
};

struct packet_authorize_server_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_authorize_server_rq;
	ServerType server_type;
	uint64_t server_id;
};

struct packet_authorize_server_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_authorize_server_rs;

	// 인증요청을 받은 서버의 id
	uint64_t server_id;
};

////////////////////////////////////////////////
// server to server
////////////////////////////////////////////////

struct packet_create_observing_object_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_create_observing_object_rq;
	uint64_t origin_sector_id;
	PacketObjectInfo object_info;
};

struct packet_remove_observing_object_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_remove_observing_object_rq;
	uint64_t sector_id;
	uint64_t object_id;
	uint8_t reason;
};

struct packet_spawn_character_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_spawn_character_rq;
	uint64_t user_id;
	std::string nickname;
	PacketObjectInfo object_info;
};

struct packet_spawn_character_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_spawn_character_rs;
	uint64_t user_id;
	std::string nickname;
	PacketObjectInfo object_info;
};

struct packet_remove_character_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_remove_character_rq;
	uint64_t user_id;
	uint64_t sector_id;
	uint64_t object_id;
	bool to_owner;
};

struct packet_remove_character_rs
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_remove_character_rs;
	uint64_t user_id;
	uint64_t sector_id;
	uint64_t object_id;
};

struct packet_promote_object_rq
{
	static constexpr PacketType PACKET_TYPE = PacketType::packet_promote_object_rq;
	uint64_t origin_sector_id;
	PacketObjectInfo object_info;
};