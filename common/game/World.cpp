#include "World.h"
#include "../utility/Time.h"
#include "../utility/Logger.h"

void World::AddSector(const uint64_t server_id, const uint64_t sector_id)
{
	SectorSystem*& sector = _sectors[sector_id];
	if (sector != nullptr)
		throw std::exception("duplicate sector");

	sector = new SectorSystem(server_id, sector_id, GetSectorFixtureRectangle(sector_id).LeftDown(), SectorSize, ChunkSize, SectorGrayZoneChunk, this);

}

void World::RemoveSector(const uint64_t sector_id)
{
	SectorSystem*& sector = _sectors[sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "have not this sector %llu", sector_id);
		return;
	}

	delete sector;
	_sectors.erase(sector_id);
}

void World::ClearSector()
{
	for (auto& pair : _sectors)
	{
		delete pair.second;
		pair.second = nullptr;
	}
}

void World::SpawnCharacter(const SocketSessionPtr& request_session, const uint64_t request_server_id, const packet_spawn_character_rq& packet)
{
	packet_spawn_character_rs response;
	response.user_id = packet.user_id;

	SectorSystem* & sector = _sectors[packet.object_info.sector_id];

	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.object_info.sector_id);
		request_session->Send(response, ErrorCode::SectorNotFound);
		return;
	}
	Fixture* fixture = sector->CreateFixture(packet.object_info.fixture.position, packet.object_info.fixture.size);

	// Object* object = new Object(fixture->id, sector->SectorId(), ObjectType::Character, ObjectState::Stop, {}, *fixture);

	// fixture->user_data = reinterpret_cast<uint64_t>(object);
	fixture->last_transform_time = utility::CurrentTick<utility::Milliseconds>().count();
		
	fixture->CreateTracingHistory(FixtureTracingEvent::Create, utility::CurrentTimeEpoch<utility::Milliseconds>().count());

	response.object_info.fixture = *fixture;
	response.object_info.object_id = fixture->id;
	response.object_info.sector_id = fixture->sector_id;

	packet_create_object_ps broadcast;
	broadcast.object_info = response.object_info;

	_posting_manager->SendTo(request_server_id, response);
	_posting_manager->SendToSectorWithout(request_server_id, broadcast, packet.object_info.sector_id);
}

void World::RemoveCharacter(const SocketSessionPtr& request_session, const uint64_t request_server_id, packet_remove_character_rq& packet)
{
	packet_remove_character_rs response;
	response.object_id = packet.object_id;
	response.sector_id = packet.sector_id;
	response.user_id = packet.user_id;

	SectorSystem*& sector = _sectors[packet.sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.sector_id);
		request_session->Send(response, ErrorCode::SectorNotFound);
		return;
	}

	Fixture* fixture = sector->GetFixture(packet.object_id);

	if (fixture == nullptr || fixture->is_observing_fixture)
	{
		if (packet.to_owner)
		{
			LOG(LogLevel::Warn, "unmatched packet sequence, reassemble packet object: %llu", packet.object_id);

			auto promoted_object = _promoted_object_list.find(packet.object_id);

			if (promoted_object != _promoted_object_list.end())
			{
				packet.sector_id = promoted_object->second.promoted_sector_id;
				_posting_manager->SendToSectorOwner(packet, packet.sector_id);
			}
			else
			{
				throw std::exception("error");
			}

			return;
		}
		else if (fixture != nullptr)
		{
			sector->RemoveFixture(fixture->id);
			return;
		}

		LOG(LogLevel::Warn, "object not found %llu", packet.object_id);
		request_session->Send(response, ErrorCode::ObjectNotFound);
		return;
	}
	else
	{
		_remove_count++;

		packet_remove_object_ps broadcast;
		broadcast.object_id = packet.object_id;
		broadcast.sector_id = packet.sector_id;

		_posting_manager->SendTo(request_server_id, response);
		_posting_manager->SendToSectorWithout(request_server_id, broadcast, packet.sector_id);

		if (fixture->location == FixtureLocation::Gray)
		{
			packet.to_owner = false;

			const auto& near_sectors = GetNearSectors(packet.sector_id, fixture->phase);

			for (const uint64_t sector_id : near_sectors)
			{
				if (sector_id == INVALID_SECTOR)
					continue;

				packet.sector_id = sector_id;
				broadcast.sector_id = sector_id;

				_posting_manager->SendToSector(broadcast, sector_id);
				_posting_manager->SendToSectorOwner(packet, sector_id);
			}
		}
	}
	sector->RemoveFixture(fixture->id);
}

void World::CreateObservingObject(const packet_create_observing_object_rq& packet)
{
	SectorSystem*& sector = _sectors[packet.object_info.sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.object_info.sector_id);
		return;
	}

	Fixture* fixture = sector->GetFixture(packet.object_info.object_id);
	if (fixture == nullptr)
		fixture = sector->CreateFixture(packet.object_info.fixture);
	else
	{
		throw std::exception("error");
		*fixture = packet.object_info.fixture;
	}
	
	fixture->is_observing_fixture = true;
	fixture->phase = sector->CalculateCurrentPhase(fixture);
	fixture->CreateTracingHistory(FixtureTracingEvent::CreateObservingObject, utility::CurrentTimeEpoch<utility::Milliseconds>().count());
}

void World::RemoveObservingObject(const packet_remove_observing_object_rq& packet)
{
	SectorSystem*& sector = _sectors[packet.sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.sector_id);
		return;
	}
	Fixture* fixture = sector->GetFixture(packet.object_id);
	
	assert(fixture != nullptr);
		
	assert(fixture->is_observing_fixture);

	sector->RemoveFixture(packet.object_id);

	// _posting_manager->SendToSector(packet, packet.sector_id);

	/*
	if (fixture->location == FixtureLocation::Gray && !fixture->is_observing_fixture)
	{
		packet_remove_observing_object_rq packet;
		packet.object_id = object_id;

		_posting_manager->SendToNearSector(packet, fixture->sector_id, fixture->phase);
	}
	*/
}

const std::unordered_map<uint64_t, Fixture*>& World::GetAllObjects(const uint64_t sector_id)
{
	SectorSystem*& sector = _sectors[sector_id];
	assert(sector != nullptr);

	return sector->GetAllFixtures();
}

const std::unordered_map<uint64_t, SectorSystem*> World::GetAllSectors()
{
	return _sectors;
}

void World::MoveObject(const SocketSessionPtr& request_session, const uint64_t request_server_id, packet_character_move_rq& packet)
{
	packet_character_move_rs response;

	response.sector_id = packet.sector_id;
	response.object_id = packet.object_id;
	response.starting_position = packet.starting_position;
	response.direction = packet.direction;

	SectorSystem*& sector = _sectors[packet.sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.sector_id);
		return;
	}

	Fixture* fixture = sector->GetFixture(packet.object_id);

	if (fixture != nullptr)
	{
		fixture->position = packet.starting_position;
		fixture->direction = packet.direction;
	}

	if (fixture == nullptr || fixture->is_observing_fixture)
	{
		if (packet.to_owner)
		{
			auto promoted_object = _promoted_object_list.find(packet.object_id);

			if (promoted_object != _promoted_object_list.end())
			{
				packet.sector_id = promoted_object->second.promoted_sector_id;
				_posting_manager->SendToSectorOwner(packet, packet.sector_id);
			}
			else
			{
				throw std::exception("error");
			}

			return;
		}
		else if (fixture != nullptr)
			return;

		throw std::exception("error");
		LOG(LogLevel::Warn, "object not found %llu", packet.object_id);
		// request_session->Send(response, ErrorCode::ObjectNotFound);
		return;

	}
	else
	{
		packet_object_move_ps broadcast;
		broadcast.direction = packet.direction;
		broadcast.object_id = packet.object_id;
		broadcast.sector_id = packet.sector_id;
		broadcast.starting_position = packet.starting_position;


		_posting_manager->SendToSectorWithout(request_server_id, broadcast, response.sector_id);
		_posting_manager->SendTo(request_server_id, response);

		if (fixture->location == FixtureLocation::Gray)
		{
			packet.to_owner = false;

			const auto& near_sectors = GetNearSectors(packet.sector_id, fixture->phase);

			for (const uint64_t sector_id : near_sectors)
			{
				if (sector_id == INVALID_SECTOR)
					continue;

				packet.sector_id = sector_id;
				broadcast.sector_id = sector_id;

				_posting_manager->SendToSector(broadcast, sector_id);
				_posting_manager->SendToSectorOwner(packet, sector_id);
			}
		}
	}
}

void World::SendRemoveObservingObject(Fixture* const fixture, const std::array<uint64_t, 4>& sectors, const uint64_t object_id)
{
	packet_remove_observing_object_rq remove_packet;
	remove_packet.object_id = object_id;
	remove_packet.reason = 0;

	packet_remove_object_ps remove_broadcast_packet;
	remove_broadcast_packet.object_id = object_id;
	remove_broadcast_packet.reason = 0;

	for (const uint64_t sector_id : sectors)
	{

		if (sector_id == INVALID_SECTOR)
			continue;

		remove_packet.sector_id = sector_id;
		remove_broadcast_packet.sector_id = sector_id;

		_posting_manager->SendToSector(remove_broadcast_packet, sector_id);
		_posting_manager->SendToSectorOwner(remove_packet, sector_id);
	}
}

void World::SendCreateObservingObject(Fixture* const fixture, const std::array<uint64_t, 3>& sectors, const uint64_t origin_sector_id, const PacketObjectInfo& object_info)
{
	packet_create_observing_object_rq create_packet;
	create_packet.object_info = object_info;
	create_packet.origin_sector_id = origin_sector_id;

	packet_create_object_ps create_broadcast_packet;
	create_broadcast_packet.object_info = object_info;

	uint64_t index = 0;
	for (const uint64_t sector_id : sectors)
	{
		if (sector_id == INVALID_SECTOR)
			continue;

		create_packet.object_info.sector_id = sector_id;
		create_packet.object_info.fixture.sector_id = sector_id;

		create_broadcast_packet.object_info.sector_id = sector_id;

		_posting_manager->SendToSector(create_broadcast_packet, sector_id);
		_posting_manager->SendToSectorOwner(create_packet, sector_id);
	}
}

void World::ExchangeObservingObject(Fixture* const fixture, const PacketObjectInfo& object_info, const FixtureLocation location, const Direction phase, const uint64_t sector_id, const FixtureLocation new_location, const Direction new_phase, const uint64_t dest_sector_id)
{
	if (sector_id == dest_sector_id && phase == new_phase && new_location == FixtureLocation::Leave)
		return;

	std::array<uint64_t, 3> _prev_near_sectors = GetNearSectors(sector_id, phase);
	std::array<uint64_t, 4> prev_near_sectors;
	std::array<uint64_t, 3> new_near_sectors = GetNearSectors(dest_sector_id, new_phase);

	std::memcpy(prev_near_sectors.data(), _prev_near_sectors.data(), sizeof(_prev_near_sectors));
	prev_near_sectors[3] = sector_id;

	// 전후 위상 중 하나라도 Max라면 비교할 필요 없음
	if (phase != Direction::Max && new_phase != Direction::Max)
	{
		for (uint64_t& prev_sector_id : prev_near_sectors)
		{
			if (prev_sector_id == INVALID_SECTOR)
				break;

			for (uint64_t& new_sector_id : new_near_sectors)
			{
				if (new_sector_id == INVALID_SECTOR)
					break;

				// 겹치는 sector 제거
				if (prev_sector_id == new_sector_id)
				{
					prev_sector_id = INVALID_SECTOR;
					new_sector_id = INVALID_SECTOR;
					break;
				}
			}
		}
	}

	// 이전 위상의 주변 sector중에 목적 sector가 있다면 무효화
	for (uint64_t& prev_near_sector_id : prev_near_sectors)
	{
		// 특수 케이스, sector를 넘어갔는데 즉시 목적 sector의 In Zone에 진입했다면 모든 Observing Object 제거
		// if (new_location == FixtureLocation::In && sector_id != dest_sector_id)
		// 	break;

		if (prev_near_sector_id == dest_sector_id)
			prev_near_sector_id = INVALID_SECTOR;
	}

	// 신규 위상의 주변 sector중에 이전 sector가 있다면 무효화
	for (uint64_t& new_near_sector_id : new_near_sectors)
	{
		if (new_near_sector_id == sector_id)
			new_near_sector_id = INVALID_SECTOR;
	}

	if (phase != Direction::Max)
		SendRemoveObservingObject(fixture, prev_near_sectors, object_info.object_id);

	if (new_phase != Direction::Max)
		SendCreateObservingObject(fixture, new_near_sectors, sector_id, object_info);
}

void World::PromoteObservingObject(const SocketSessionPtr& request_session, const uint64_t request_server_id, const packet_promote_object_rq& packet)
{
	SectorSystem*& sector = _sectors[packet.object_info.sector_id];
	if (sector == nullptr)
	{
		LOG(LogLevel::Warn, "sector not found %llu", packet.object_info.sector_id);
		return;
	}

	Fixture* promoted_fixture = sector->GetFixture(packet.object_info.object_id);
	if (promoted_fixture == nullptr)
	{
		throw std::exception("error");
		LOG(LogLevel::Error, "fatal error, prototed object not found %llu", packet.object_info.object_id);
		request_session->CloseSession();
		return;
	}

	*promoted_fixture = packet.object_info.fixture;
	promoted_fixture->is_observing_fixture = false;
	promoted_fixture->last_transform_time = static_cast<uint32_t>(utility::CurrentTick<utility::Milliseconds>().count());

	FixtureLocation origin_location = promoted_fixture->location;
	FixtureLocation dest_location = sector->CalculateCurrentLocation(promoted_fixture);

	Direction origin_phase = promoted_fixture->phase;
	Direction dest_phase = sector->CalculateCurrentPhase(promoted_fixture);
	if (dest_location == FixtureLocation::In)
		dest_phase = Direction::Max;

	// 기록용
	promoted_fixture->phase = dest_phase;
	promoted_fixture->location = dest_location;
	promoted_fixture->CreateTracingHistory(FixtureTracingEvent::PromoteObject, utility::CurrentTimeEpoch<utility::Milliseconds>().count());

	// agency가 object를 찾지 못하는 경우가 있기 때문에 먼저 전송
	_posting_manager->SendToSector(packet, packet.origin_sector_id);

	if (dest_location == FixtureLocation::Leave)
	{
		throw std::exception("error");
	}
	// Zone 순서를 지키지 않고 텔레포트해서 건너간 경우
	else if (!IsNearPhase(packet.origin_sector_id, origin_phase, promoted_fixture->sector_id, dest_phase))
	{
		ExchangeObservingObject(promoted_fixture, packet.object_info, origin_location, origin_phase, packet.origin_sector_id, dest_location, dest_phase, promoted_fixture->sector_id);
	}
}

thread_local std::vector<uint64_t> untracked_objects;

void World::Update(const utility::Milliseconds current_time, const utility::Milliseconds delta_time)
{
	for (const auto& pair : _promoted_object_list)
	{
		if (pair.second.tracking_timer)
			untracked_objects.push_back(pair.first);
	}

	for (const uint64_t object_id : untracked_objects)
		_promoted_object_list.erase(object_id);

	untracked_objects.clear();

	for (std::pair<const uint64_t, SectorSystem*>& pair : _sectors)
	{
		assert(pair.second != nullptr);

		pair.second->Update(current_time, delta_time);

		for (const uint64_t fixture_id : _removing_fixtures)
			pair.second->RemoveFixture(fixture_id);
		
		_removing_fixtures.clear();
	}

}

void World::OnChangeFixturePhase(const uint64_t sector_id, Fixture* const fixture, const Direction phase)
{
	assert(fixture->location == FixtureLocation::Gray);

	PacketObjectInfo object_info;

	object_info.object_id = fixture->id;
	object_info.sector_id = sector_id;
	object_info.fixture = *fixture;
	
#ifdef _DEBUG
	Direction prev_phase = fixture->phase;
	fixture->phase = phase;

	fixture->CreateTracingHistory(FixtureTracingEvent::ChangePhase, utility::CurrentTimeEpoch<utility::Milliseconds>().count());

	fixture->phase = prev_phase;
#endif

	ExchangeObservingObject(fixture, object_info, fixture->location, fixture->phase, fixture->sector_id, fixture->location, phase, sector_id);
}

void World::OnChangeFixtureLocation(const uint64_t sector_id, Fixture* const fixture, const FixtureLocation location, const Direction phase)
{
	auto sector_iterator = _sectors.find(sector_id);
	if (sector_iterator == _sectors.end())
		return;

#ifdef _DEBUG
	FixtureLocation prev_location = fixture->location;
	Direction prev_phase = fixture->phase;

	fixture->location = location;
	fixture->phase = phase;
	fixture->CreateTracingHistory(FixtureTracingEvent::ChangeLocation, utility::CurrentTimeEpoch<utility::Milliseconds>().count());

	fixture->location = prev_location;
	fixture->phase = prev_phase;
#endif

	fixture->last_transform_time = utility::CurrentTick<utility::Milliseconds>().count();

	PacketObjectInfo object_info;
	object_info.object_id = fixture->id;
	object_info.sector_id = sector_id;
	object_info.fixture = *fixture;
	
	ExchangeObservingObject(fixture, object_info, fixture->location, fixture->phase, sector_id, location, phase, sector_id);

	if (location != FixtureLocation::Leave)
		return;

	auto near_sectors = GetNearSectors(sector_id, phase);

	uint64_t dest_sector_id = GetSectorByPosition(fixture->position);

	bool find_in_near_sector = false;
	for (const uint64_t near_sector_id : near_sectors)
	{
		if (near_sector_id == dest_sector_id)
		{
			find_in_near_sector = true;
			break;
		}
	}

	if (!find_in_near_sector)
	{
		SendCreateObservingObject(fixture, { dest_sector_id, INVALID_SECTOR, INVALID_SECTOR }, sector_id, object_info);
	}
	object_info.fixture.location = location;
	object_info.fixture.phase = phase;
	fixture->is_observing_fixture = true;

	packet_promote_object_rq packet;
	packet.origin_sector_id = sector_id;
	packet.object_info = object_info;

	assert(dest_sector_id != INVALID_SECTOR);

	packet.object_info.sector_id = dest_sector_id;
	packet.object_info.fixture.sector_id = dest_sector_id;

	PromotedObjectInfo promoted_object;
	promoted_object.object_id = fixture->id;
	promoted_object.promoted_sector_id = packet.object_info.sector_id;
	promoted_object.tracking_timer = utility::Countdown<utility::Milliseconds>(utility::Milliseconds(TransformCooltime * 4));
		
	_promoted_object_list[fixture->id] = promoted_object;

	_posting_manager->SendToSectorOwner(packet, packet.object_info.sector_id);
}

void World::OnCollisionFixture(const uint64_t sector_id, Fixture* const fixture1, Fixture* const fixture2)
{

}