#pragma once

#include "../memory/SequentialBuffer.h"
#include "../network/SocketServer.h"
#include "SocketSession.h"
#include <unordered_map>
#include <unordered_set>

struct ConnectorInfo
{
	uint64_t connector_id = 0;

	TransferStreamPtr stream;
	SocketAddress remote_address;
	uint64_t attachment = 0;

	uint64_t session_id = 0;
	SessionKey session_key = SessionKey::Empty();
};

struct SocketWorkerInfo
{
	std::unordered_map<uint64_t, TransferStreamPtr> _activated_sockets;

	std::unordered_map<uint64_t, ConnectorInfo> _connector_list;

	std::unordered_map<uint64_t, SocketSessionPtr> _opened_sessions;
	std::unordered_map<uint64_t, SocketSessionPtr> _abandoned_sessions;
};