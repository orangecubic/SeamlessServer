#pragma once

#include "Packet.h"
#include "../../common.h"
#include <ylt/struct_pack.hpp>
#include <xmemory>
#include <cassert>

inline PacketHeader DeserializePacketHeader(const uint8_t* const packet_buffer, const std::size_t buffer_size)
{
	assert(buffer_size >= PACKET_HEADER_SIZE);
	return *reinterpret_cast<const PacketHeader*>(packet_buffer);
}

template <NetworkPacketConcept T>
inline bool DeserializePacketBody(const uint8_t* const packet_buffer, const std::size_t buffer_size, T& packet)
{
	struct_pack::err_code error = struct_pack::deserialize_to(packet, reinterpret_cast<const char*>(packet_buffer), buffer_size);

	return error == struct_pack::errc::ok;
}

template <NetworkPacketConcept T>
inline bool SerializePacket(const T& packet, const ErrorCode error_code, uint8_t* const dest_buffer, const uint32_t buffer_size, uint32_t& packet_size)
{
	auto needed_size = struct_pack::get_needed_size(packet);

	packet_size = PACKET_LENGTH_SIZE + PACKET_HEADER_SIZE + static_cast<uint32_t>(needed_size);

	if (packet_size > buffer_size)
		return false;

	PacketLengthType packet_length = packet_size - PACKET_LENGTH_SIZE;

	PacketHeader header{ T::PACKET_TYPE, error_code, static_cast<uint32_t>(needed_size)};

	std::memcpy(dest_buffer, &packet_length, PACKET_LENGTH_SIZE);
	std::memcpy(dest_buffer + PACKET_LENGTH_SIZE, &header, PACKET_HEADER_SIZE);

	struct_pack::serialize_to(reinterpret_cast<char*>(dest_buffer + PACKET_LENGTH_SIZE + PACKET_HEADER_SIZE), needed_size, packet);

	return true;
}