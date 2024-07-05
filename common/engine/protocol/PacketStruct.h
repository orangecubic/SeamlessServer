#pragma once

#include "PacketEnum.h"
#include "../../network/SocketServer.h"


struct NetworkEngineOption
{
	uint32_t socket_idle_timeout_ms = 5000;
	uint32_t session_idle_timeout_ms = 5000;
	uint32_t session_heartbeat_interval_ms = 2000;

	uint32_t max_session_count = 64;
	bool use_session_reconnect = false;
	uint32_t session_reconnect_timeout_ms = 30000;

	uint32_t worker_update_tick_ms = 500;
};

struct GameConfig
{
	uint16_t game_update_tick_ms;
	uint16_t packet_batch_process_time_ms;
	uint16_t fixture_transform_cooltime_ms;
};

struct ServerConfig
{
	// server to server network layer //
	uint16_t intra_server_listen_port;
	SocketServerConfig intra_socket_server_config;
	NetworkEngineOption intra_network_engine_option;

	// user network layer
	uint16_t user_listen_port;
	SocketServerConfig user_socket_server_config;
	NetworkEngineOption user_network_engine_option;

	// game layer //
	GameConfig game_config;
	
};

struct ServerInfo
{
	ServerType server_type;
	uint64_t server_id;
	SocketAddress address;
};

struct SectorAllocationInfo
{
	ServerInfo server_info;
	uint64_t sector_id;
};

struct PacketObjectInfo
{
	uint64_t object_id;
	uint64_t sector_id;
	ObjectType type;
	Fixture fixture;
	Vital vital;
};