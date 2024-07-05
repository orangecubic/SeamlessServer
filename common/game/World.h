#pragma once

#include "coordinate/SectorGrid.h"
#include "SectorPostingManager.h"
#include "coordinate/SectorSystem.h"
#include "../engine/protocol/Packet.h"
#include "../utility/MultiDimensionMap.h"
#include "object/Object.h"

struct PromotedObjectInfo
{
	uint64_t object_id;
	uint64_t promoted_sector_id;

	utility::Countdown<utility::Milliseconds> tracking_timer;
};

class World : public SectorSystem::Callback
{
private:
	uint64_t _world_id = 0;

	SectorPostingManager* _posting_manager;

	std::unordered_map<uint64_t, SectorSystem*> _sectors;

	std::unordered_map<uint64_t, PromotedObjectInfo> _promoted_object_list;

	std::vector<uint64_t> _removing_fixtures;

	utility::Milliseconds _fixture_transform_cooltime;

	uint32_t _remove_count = 0;

	void SendRemoveObservingObject(Fixture* const fixture, const std::array<uint64_t, 4>& sectors, const uint64_t object_id);
	void SendCreateObservingObject(Fixture* const fixture, const std::array<uint64_t, 3>& sectors, const uint64_t origin_sector_id, const PacketObjectInfo& object_info);
	void ExchangeObservingObject(Fixture* const fixture, const PacketObjectInfo& object_info, const FixtureLocation location, const Direction phase, const uint64_t sector_id, const FixtureLocation new_location, const Direction new_phase, const uint64_t dest_sector_id);
public:

	World(SectorPostingManager* const posting_manager, const utility::Milliseconds fixture_transform_cooltime) : _posting_manager(posting_manager), _fixture_transform_cooltime(fixture_transform_cooltime) {}

	void AddSector(const uint64_t server_id, const uint64_t sector_id);
	void RemoveSector(const uint64_t sector_id);

	void ClearSector();
	///////////////////////////////////////////////////////////////////////
	
	virtual void OnChangeFixturePhase(const uint64_t sector_id, Fixture* const fixture, const Direction phase) override;
	virtual void OnChangeFixtureLocation(const uint64_t sector_id, Fixture* const fixture, const FixtureLocation location, const Direction phase) override;
	virtual void OnCollisionFixture(const uint64_t sector_id, Fixture* const fixture1, Fixture* const fixture2) override;

	///////////////////////////////////////////////////////////////////////

	void SpawnCharacter(const SocketSessionPtr& request_session, const uint64_t request_server_id, const packet_spawn_character_rq& packet);
	void RemoveCharacter(const SocketSessionPtr& request_session, const uint64_t request_server_id, packet_remove_character_rq& packet);

	void MoveObject(const SocketSessionPtr& request_session, const uint64_t request_server_id, packet_character_move_rq& packet);

	void CreateObservingObject(const packet_create_observing_object_rq& packet);
	void RemoveObservingObject(const packet_remove_observing_object_rq& packet);

	void PromoteObservingObject(const SocketSessionPtr& request_session, const uint64_t request_server_id, const packet_promote_object_rq& packet);

	const std::unordered_map<uint64_t, Fixture*>& GetAllObjects(const uint64_t sector_id);
	const std::unordered_map<uint64_t, SectorSystem*> GetAllSectors();
	void Update(const utility::Milliseconds current_time, const utility::Milliseconds delta_time);

	
}; 