#pragma once

#include "IocpTransferStream.h"

class IocpConnectorStream : public IocpTransferStream
{
public:
	void TransmitConnect(const uint64_t attachment);
};