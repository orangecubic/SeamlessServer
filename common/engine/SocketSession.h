#pragma once

#include "Session.h"
#include "protocol/Packet.h"
#include "protocol/Protocol.h"
#include "../network/SocketServer.h"
#include "../memory/ObjectPool.h"
#include <concepts>

constexpr uint64_t GRACEFUL_SHUTDOWN_CODE = 0x01;

class NetworkEngine;

class SocketSession : public Session
{
private:
	TransferStreamPtr _socket_stream;
	NetworkEngine* _engine;

	TransferStream* GetSocketStream()
	{
		SessionState state = GetSessionState();
		assert(state == SessionState::Opened || state == SessionState::Abandoned);

		return _socket_stream._UnsafePtr();
	}

	friend NetworkEngine;
public:

	void ResetSocketStream(const TransferStreamPtr& stream)
	{
		_socket_stream = stream;
	}

	void SetNetworkEngine(NetworkEngine* engine)
	{
		_engine = engine;
	}

	template <NetworkPacketConcept Packet>
	void Send(const Packet& packet, const ErrorCode error = ErrorCode::None, bool must_send = false)
	{
		SocketBuffer* buffer = _socket_stream->AllocateWriteBuffer();

		if (buffer == nullptr)
		{
			if (must_send)
				buffer = _socket_stream->AllocateWriteBuffer(true);
			else
			{
				CloseSession();
				return;
			}
		}
#pragma warning ( disable : 6011 )
		if (!SerializePacket(packet, error, buffer->ptr, buffer->capacity, buffer->length))
			assert(false);
#pragma warning ( default : 6011 )

		GetSocketStream()->TransmitWrite(buffer, 0, 0);
	}

	virtual void Send(SocketBuffer* const buffer)
	{
#ifdef _DEBUG
		DynamicBufferCursor<SocketBuffer> cursor(buffer);
		
		PacketLengthType packet_length;

		std::memcpy(&packet_length, cursor.Data(), PACKET_LENGTH_SIZE);

		cursor.Seek(PACKET_LENGTH_SIZE);

		PacketHeader header = DeserializePacketHeader(cursor.Data(), cursor.RemainBytes());
		
		assert(header.body_size + sizeof(PacketHeader) == packet_length);
#endif
		GetSocketStream()->TransmitWrite(buffer, 0, 0);
	}

	virtual SocketBuffer* TryAllocateReadBuffer()
	{
		TransferStream* stream = GetSocketStream();

		SocketBuffer* read_buffer = stream->AllocateReadBuffer();

		if (read_buffer != nullptr)
			return read_buffer;

		Send(packet_bandwith_overflow_ps{}, ErrorCode::None, true);
		CloseSession();

		return nullptr;
	}

	virtual void ReleaseReadBuffer(SocketBuffer* const buffer)
	{
		GetSocketStream()->ReleaseReadBuffer(buffer);
	}

	virtual SocketBuffer* TryAllocateWriteBuffer()
	{
		TransferStream* stream = GetSocketStream();

		SocketBuffer* write_buffer = stream->AllocateWriteBuffer();

		if (write_buffer != nullptr)
			return write_buffer;

		Send(packet_bandwith_overflow_ps{}, ErrorCode::None, true);
		CloseSession();

		return nullptr;
	}

	virtual void ReleaseWriteBuffer(SocketBuffer* const buffer)
	{
		GetSocketStream()->ReleaseWriteBuffer(buffer);
	}

	virtual void Disconnect()
	{
		GetSocketStream()->TransmitDisconnect(0);
	}

	virtual void CloseSession()
	{
		Send(packet_session_close_rq{}, ErrorCode::None, true);
		GetSocketStream()->TransmitDisconnect(GRACEFUL_SHUTDOWN_CODE);
	}

	NetworkEngine* GetNetworkEngine()
	{
		return _engine;
	}

	void ReleaseStream()
	{
		_socket_stream.Release();
	}

};

using SocketSessionPtr = IntrusivePtr<SocketSession>;

class LoopbackSessionBufferInitializer
{
public:
	static void Initialize(SocketBuffer* const buffer, const uint64_t id, const uint64_t param)
	{
		*const_cast<uint32_t*>(&buffer->capacity) = static_cast<uint32_t>(param);
		buffer->length = 0;
		buffer->ptr = new uint8_t[param];
	}
};

using LoopbackSessionBufferPool = ObjectPool<SocketBuffer, false, LoopbackSessionBufferInitializer>;

template <typename T> 
	requires requires(T t, SocketBuffer* const buffer) {
		{ t.ProcessLoopbackData(buffer) } -> std::convertible_to<void>;
	}
class LoopbackSocketSession : public SocketSession
{
private:
	std::vector<SocketBuffer*> _packet_list;
	LoopbackSessionBufferPool* _buffer_pool;

	T* _executor;
public:

	LoopbackSocketSession(T* const executor, const uint32_t initial_pool_size, const uint32_t buffer_size)
	{
		_executor = executor;
		_buffer_pool = new LoopbackSessionBufferPool(initial_pool_size, initial_pool_size, buffer_size);
		_packet_list.reserve(buffer_size);
	}

	virtual void Send(SocketBuffer* const buffer) override
	{
		_packet_list.push_back(buffer);
	}

	virtual SocketBuffer* TryAllocateReadBuffer() override
	{
		SocketBuffer* buffer = _buffer_pool->Pop(true);
		buffer->length = 0;
		
		return buffer;
	}

	virtual void ReleaseReadBuffer(SocketBuffer* const buffer) override
	{
		_buffer_pool->Push(buffer);
	}

	virtual SocketBuffer* TryAllocateWriteBuffer() override
	{
		SocketBuffer* buffer = _buffer_pool->Pop(true);
		buffer->length = 0;

		return buffer;
	}

	virtual void ReleaseWriteBuffer(SocketBuffer* const buffer) override
	{
		_buffer_pool->Push(buffer);
	}

	virtual void Disconnect() override {}

	virtual void CloseSession() override {}

	void ProcessLoopbackPacket()
	{
		for (SocketBuffer* const buffer : _packet_list)
			_executor->ProcessLoopbackData(buffer);

		_packet_list.clear();
	}
};