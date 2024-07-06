#pragma once

#include "SocketSession.h"
#include "../utility/Sampling.h"
#include "../network/Meta.h"
#include "protocol/Packet.h"
#include "protocol/Protocol.h"

enum class PacketPostingPolicy : uint8_t
{
	Immediate = 1,
	Throttle = 2,
};

class PacketThrottler
{
private:

	struct ThrottleData
	{
		SocketSessionPtr session = SocketSessionPtr(nullptr);
		SocketBuffer* buffer = nullptr;
	};

	std::unordered_map<uint64_t, ThrottleData> _throttle_data;

	utility::Throttler<utility::Milliseconds> _throttler;

	void SendPacket(ThrottleData& throttle_data)
	{
		throttle_data.session->Send(throttle_data.buffer);
		throttle_data.buffer = nullptr;
	}

	template <NetworkPacketConcept Packet>
	void StorePacket(ThrottleData& throttle_data, const Packet& packet, const ErrorCode error)
	{
		uint32_t packet_size = 0;
		bool success = SerializePacket(packet, error, throttle_data.buffer->ptr + throttle_data.buffer->length, throttle_data.buffer->capacity - throttle_data.buffer->length, packet_size);

		if (!success)
		{
			SendPacket(throttle_data);
			throttle_data.buffer = throttle_data.session->TryAllocateWriteBuffer();

			if (throttle_data.buffer == nullptr)
				return;

			success = SerializePacket(packet, error, throttle_data.buffer->ptr + throttle_data.buffer->length, throttle_data.buffer->capacity - throttle_data.buffer->length, packet_size);
			if (!success)
				throw std::exception("packet size overflow");
		}

		throttle_data.buffer->length += packet_size;
	}

public:

	PacketThrottler(const utility::Milliseconds tick) : _throttler(tick) {}

	template <NetworkPacketConcept Packet>
	void PostPacket(const SocketSessionPtr& session, const Packet& packet, const PacketPostingPolicy policy = PacketPostingPolicy::Immediate)
	{
		ThrottleData& throttle_data = _throttle_data[session->GetSessionId()];

		if (throttle_data.buffer == nullptr)
		{
			throttle_data.buffer = session->TryAllocateWriteBuffer();
			if (throttle_data.buffer == nullptr)
			{
				_throttle_data.erase(session->GetSessionId());
				return;
			}
		}
		
		throttle_data.session = session;

		StorePacket(throttle_data, packet, ErrorCode::None);

		switch (policy)
		{
		case PacketPostingPolicy::Immediate:
			SendPacket(throttle_data);
			_throttle_data.erase(session->GetSessionId());
			break;
		case PacketPostingPolicy::Throttle:
			break;
		}

	}

	void CleanUp(const SocketSessionPtr& session)
	{
		auto iter = _throttle_data.find(session->GetSessionId());
		if (iter == _throttle_data.end())
			return;

		if (iter->second.buffer != nullptr)
			iter->second.session->ReleaseWriteBuffer(iter->second.buffer);

		_throttle_data.erase(iter);
	}

	void TryFlushPacket()
	{
		if (!_throttler)
			return;

		ForceFlushPacket();
	}

	void ForceFlushPacket()
	{
		for (std::pair<const uint64_t, ThrottleData>& pair : _throttle_data)
			SendPacket(pair.second);

		_throttle_data.clear();
	}

};