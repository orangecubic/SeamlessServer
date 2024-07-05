#pragma once

#include "../AcceptorStream.h"

class IocpAcceptorStream : public AcceptorStream
{
public:

	virtual bool TransmitAccept(TransferStream* const transfer_stream) override;

};