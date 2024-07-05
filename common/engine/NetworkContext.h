#pragma once

#include "../network/SocketServer.h"
#include "protocol/Packet.h"

enum class NetworkSubject : uint8_t
{
	Socket,
	Mysql,
	Redis,
};

enum class ContextType : uint8_t
{
	SessionAccepted,
	SessionConnected,
	SessionAbandoned,
	SessionAlived,
	SessionClosed,
	SessionData,

	SessionError,
};

struct NetworkContext
{
	NetworkContext* next;
	NetworkSubject network_subject;
	ContextType context_type;
	uint16_t last_error_code;
};