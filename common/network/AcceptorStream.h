#pragma once

#include "TransferStream.h"

class AcceptorStream : public SocketStreamBase
{
public:
	virtual bool TransmitAccept(TransferStream* const transfer_stream) = 0;
};