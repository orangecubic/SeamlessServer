#pragma once

#include "coordinate/SectorGrid.h"
#include "../engine/PacketThrottler.h"
#include "../engine/SocketSession.h"
#include <unordered_map>
#include <unordered_set>

struct NetworkSessionInfo;

struct NetworkSectorInfo
{
	uint64_t owner_id = 0;
	std::unordered_map<uint64_t, NetworkSessionInfo*> listening_sessions;
};

struct NetworkSessionInfo
{
	SocketSessionPtr session = SocketSessionPtr(nullptr);
	std::unordered_map<uint64_t, NetworkSectorInfo*> related_sectors;
};

class SectorPostingManager
{
private:
	std::unordered_map<uint64_t, NetworkSectorInfo> _sectors;

	std::unordered_map<uint64_t, NetworkSessionInfo> _sessions;

	PacketThrottler* _packet_throttler;

public:
	SectorPostingManager(PacketThrottler* const packet_throttler) : _packet_throttler(packet_throttler) {}

	template <NetworkPacketConcept Packet>
	void SendToNearSector(const Packet& packet, const uint64_t sector_id, const Direction direction, bool with_owner = false)
	{
		std::array<uint64_t, 3> sectors = GetNearSectors(sector_id, direction);

		for (const uint64_t near_sector_id : sectors)
		{
			if (near_sector_id == INVALID_SECTOR)
				break;

			SendToSector(packet, near_sector_id, with_owner);
		}
	}

	template <NetworkPacketConcept Packet>
	void SendToSector(const Packet& packet, const uint64_t sector_id, bool with_owner = false)
	{
		auto sector_iterator = _sectors.find(sector_id);
		if (sector_iterator == _sectors.end())
			return;

		for (auto& session_pair : sector_iterator->second.listening_sessions)
		{
			if (session_pair.second->session.Valid())
				_packet_throttler->PostPacket(session_pair.second->session, packet, PacketPostingPolicy::Throttle);
		}

		if (with_owner)
			SendToSectorOwner(packet, sector_id);
	}

	template <NetworkPacketConcept Packet>
	void SendToSectorWithout(const uint64_t ignored_target, const Packet& packet, const uint64_t sector_id, bool with_owner = false)
	{
		auto sector_iterator = _sectors.find(sector_id);
		if (sector_iterator == _sectors.end())
			return;

		for (auto& session_pair : sector_iterator->second.listening_sessions)
		{
			if (session_pair.first == ignored_target)
				continue;

			if (session_pair.second->session.Valid())
				_packet_throttler->PostPacket(session_pair.second->session, packet, PacketPostingPolicy::Throttle);
		}

		if (with_owner)
			SendToSectorOwner(packet, sector_id);
	}

	template <NetworkPacketConcept Packet>
	void SendToSectorOwner(const Packet& packet, const uint64_t sector_id)
	{
		auto sector_iterator = _sectors.find(sector_id);
		if (sector_iterator == _sectors.end())
			return;

		auto session_iterator = _sessions.find(sector_iterator->second.owner_id);
		if (session_iterator != _sessions.end() && session_iterator->second.session.Valid())
			_packet_throttler->PostPacket(session_iterator->second.session, packet, PacketPostingPolicy::Throttle);
	}

	template <NetworkPacketConcept Packet>
	void SendTo(const uint64_t target_id, const Packet& packet)
	{
		auto session_iterator = _sessions.find(target_id);
		if (session_iterator != _sessions.end() && session_iterator->second.session.Valid())
			_packet_throttler->PostPacket(session_iterator->second.session, packet, PacketPostingPolicy::Throttle);
	}


	// play의 관리 sector 지정
	void AllocateSector(const uint64_t sector_id, const uint64_t target_id);

	void LinkSession(const uint64_t target_id, const SocketSessionPtr& session);
	void UnlinkSession(const uint64_t target_id);

	// session의 정보를 받아갈 대상 지정
	void AddSectorListener(const uint64_t sector_id, const uint64_t target_id);
	void RemoveSectorListener(const uint64_t sector_id, const uint64_t target_id);
	void UnsetAll(const uint64_t target_id);

	const SocketSessionPtr& GetSectorOwnerSession(const uint64_t sector_id);

	const std::unordered_map<uint64_t, NetworkSectorInfo*>& GetOwnershipSectors(const uint64_t target_id);

};