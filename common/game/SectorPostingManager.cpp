#include "SectorPostingManager.h"

void SectorPostingManager::AllocateSector(const uint64_t sector_id, const uint64_t target_id)
{
	NetworkSectorInfo& sector_info = _sectors[sector_id];

	if (sector_info.owner_id != 0)
		_sessions[sector_info.owner_id].related_sectors.erase(sector_id);

	NetworkSessionInfo& session_info = _sessions[target_id];

	session_info.related_sectors[sector_id] = &sector_info;
	sector_info.owner_id = target_id;
}

void SectorPostingManager::LinkSession(const uint64_t target_id, const SocketSessionPtr& session)
{
	_sessions[target_id].session = session;
}

void SectorPostingManager::UnlinkSession(const uint64_t target_id)
{
	NetworkSessionInfo& session_info = _sessions[target_id];

	if (!session_info.session.Valid())
		return;

	_packet_throttler->CleanUp(session_info.session);
	session_info.session.Release();
}

void SectorPostingManager::AddSectorListener(const uint64_t sector_id, const uint64_t target_id)
{
	NetworkSectorInfo& sector_info = _sectors[sector_id];

	NetworkSessionInfo& session_info = _sessions[target_id];

	session_info.related_sectors[sector_id] = &sector_info;

	sector_info.listening_sessions[target_id] = &session_info;
}

void SectorPostingManager::RemoveSectorListener(const uint64_t sector_id, const uint64_t target_id)
{
	NetworkSessionInfo& session_info = _sessions[target_id];

	for (auto& sector_pair : session_info.related_sectors)
		sector_pair.second->listening_sessions.erase(target_id);
}

void SectorPostingManager::UnsetAll(const uint64_t target_id)
{
	NetworkSessionInfo& session_info = _sessions[target_id];

	for (auto& sector_pair : session_info.related_sectors)
	{
		sector_pair.second->listening_sessions.erase(target_id);
		if (sector_pair.second->owner_id == target_id)
			sector_pair.second->owner_id = 0;
	}

	session_info.related_sectors.clear();

	UnlinkSession(target_id);
}

const SocketSessionPtr& SectorPostingManager::GetSectorOwnerSession(const uint64_t sector_id)
{
	NetworkSectorInfo& sector_info = _sectors[sector_id];
	NetworkSessionInfo& session_info = _sessions[sector_info.owner_id];

	return session_info.session;
}

const std::unordered_map<uint64_t, NetworkSectorInfo*>& SectorPostingManager::GetOwnershipSectors(const uint64_t target_id)
{
	NetworkSessionInfo& session_info = _sessions[target_id];
	return session_info.related_sectors;
}