#pragma once

#include "../../game/define.h"
#include <xmemory>

enum ErrorCode : uint16_t
{
	None,
	UnknownServerId,
	SectorNotFound,
	ObjectNotFound,
	RecoveryFailed,
	SessionAuthFailed,
	SessionIsNotReady,
	SessionIsFull,
};

enum class ServerType : uint8_t
{
	Supervisor = 1,
	Agency = 2,
	Play = 3,
};
