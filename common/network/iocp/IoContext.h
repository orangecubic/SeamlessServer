#pragma once

#include "../../memory/RefCounter.h"
#include "../Meta.h"
#include "../TransferStream.h"
#include "../../memory/IntrusivePtr.h"

struct _IoContextStruct
{
	uint64_t attachment = 0;
	IntrusivePtr<TransferStream> operation_stream;

	IoOperation operation = IoOperation::None;
};

struct IoContext : public OVERLAPPED, public _IoContextStruct, public SocketBuffer { };