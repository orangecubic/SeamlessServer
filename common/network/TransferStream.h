#pragma once

#include "../memory/IntrusivePtr.h"
#include "SocketStreamBase.h"

class TransferStream : public SocketStreamBase, public RefCounter<true>
{
public:

	// Transmit 이후에 buffer를 사용하지 말 것

	// Thread Safe
	virtual void TransmitRead(SocketBuffer* const buffer, const uint64_t attachment) = 0;

	// Thread Safe
	virtual void TransmitWrite(SocketBuffer* const buffer, const uint32_t offset, const uint64_t attachment) = 0;

	// Thread Safe
	virtual void TransmitDisconnect(const uint64_t attachment) = 0;

	virtual SocketBuffer* AllocateReadBuffer(bool must_allocation = false) = 0;

	virtual void ReleaseReadBuffer(SocketBuffer* const buffer) = 0;

	virtual SocketBuffer* AllocateWriteBuffer(bool must_allocation = false) = 0;

	virtual void ReleaseWriteBuffer(SocketBuffer* const buffer) = 0;
};

using TransferStreamPtr = IntrusivePtr<TransferStream>;