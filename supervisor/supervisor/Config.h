#pragma once

#include <engine/protocol/Packet.h>

/*
	uint16_t listen_port;
	SocketServerConfig socket_server_config;
	NetworkEngineOption network_engine_option;

	// game layer //
	uint16_t game_update_tick_ms;
*/

/*
uint32_t parallel_acceptor_count = 1;
uint32_t max_connectable_socket_count = 256;
uint32_t max_acceptable_socket_count = 1024;
uint32_t worker_count = 2;
uint32_t initial_buffer_pool_size = 4;
uint32_t buffer_pool_expanding_size = 2;
uint32_t buffer_capacity = 65536;

uint32_t update_tick = 0;
*/

/*
struct NetworkEngineOption
{
	uint32_t socket_idle_timeout_ms;
	uint32_t session_idle_timeout_ms;
	uint32_t session_heartbeat_interval_ms;

	uint32_t max_session_count;

	bool use_session_reconnect;
	uint32_t session_reconnect_timeout_ms;

	uint32_t worker_update_tick_ms;
};
*/

constexpr uint32_t MaxSectorPerPlayServer = 50;

constexpr ServerConfig AgencyServerConfig = {

	// intra server netowkork layer
	9998,
	{
		1,
		32,
		1,
		2,
		256,
		65536,
		256,
		131072,
		250,
	},
	{
		180000,
		180000,
		1000,
		256,
		false,
		0,
		33
	},

	// user network layer
	9997,
	{
		4,
		32,
		1024,
		2,
		32,
		8192,
		16,
		131072,
		250,
	},
	{
		180000,
		180000,
		1000,
		1024,
		false,
		0,
		33
	},

	{
		33,
		10,
		1000,
	}
};

constexpr ServerConfig PlayServerConfig = {
	9999,
	{
		2,
		32,
		32,
		2,
		512,
		131072,
		512,
		131072,
		250
	},
	{
		180000,
		180000,
		1000,
		256,
		false,
		0,
		33
	},

	0,
	{},
	{},

	{
		33,
		10,
		1000,
	}
};