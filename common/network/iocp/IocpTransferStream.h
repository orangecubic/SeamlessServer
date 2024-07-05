#pragma once

#include "../TransferStream.h"
#include "../../memory/ObjectPool.h"
#include "IoContext.h"

class IoContextInitializer
{
public:
	static void Initialize(IoContext* const context, const uint64_t id, const uint64_t param);
};

class IoContextDestructor
{
public:
	static void Destruct(IoContext* const context);
};

using IoContextObjectPool = ObjectPool<IoContext, true, IoContextInitializer, IoContextDestructor>;

class IocpTransferStream : public TransferStream
{
protected:
	HANDLE _iocp_handle;
	IoContextObjectPool* _read_buffer_pool;
	IoContextObjectPool* _write_buffer_pool;
public:

	virtual void TransmitRead(SocketBuffer* const buffer, const uint64_t attachment) override;

	virtual void TransmitWrite(SocketBuffer* const buffer, const uint32_t offset, const uint64_t attachment) override;

	virtual void TransmitDisconnect(const uint64_t attachment) override;

	virtual SocketBuffer* AllocateReadBuffer(bool must_allocation = false) override;

	virtual void ReleaseReadBuffer(SocketBuffer* const buffer) override;

	virtual SocketBuffer* AllocateWriteBuffer(bool must_allocation = false) override;

	virtual void ReleaseWriteBuffer(SocketBuffer* const buffer) override;

	void InitializeBufferPool(const uint32_t read_buffer_pool_size, const uint32_t read_buffer_size, const uint32_t write_buffer_pool_size, const uint32_t write_buffer_size);

	void SetIocpHandle(const HANDLE handle);

	HANDLE GetIocpHandle() const;

	virtual ~IocpTransferStream();
};