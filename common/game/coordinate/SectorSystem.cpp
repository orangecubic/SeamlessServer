#include "SectorSystem.h"
#include "SectorGrid.h"

SectorSystem::SectorSystem(const uint64_t server_id, const uint64_t sector_id, const Vector2 base_position, const Size sector_size, const CoordinateUnit chunk_size, const CoordinateUnit gray_zone_chunk, Callback* const callback)
	: _sector_id(sector_id), _size(sector_size), _chunk_size(chunk_size), _gray_zone_chunk(gray_zone_chunk), _base_position(base_position), _callback(callback)
{
	_current_fixture_id = server_id * 1000000000000000 + 200000000000000 + 100000000000 * sector_id + 1;
	_direction_ratio = Size{ _size.x / 3, _size.y / 3 };
	_inner_box = Size{ _size.x - _chunk_size * _gray_zone_chunk * 2, _size.y - _chunk_size * _gray_zone_chunk * 2 };
	_inner_box_position = Vector2{_base_position.x + _chunk_size * _gray_zone_chunk, _base_position.y + _chunk_size * _gray_zone_chunk };
}

Fixture* SectorSystem::CreateFixture(const Fixture& origin)
{
	if (_fixtures.find(origin.id) != _fixtures.end())
		throw std::exception("duplicate fixture id");

	Fixture* fixture = new Fixture();
	*fixture = origin;

	_fixtures[fixture->id] = fixture;

	return fixture;
}

Fixture* SectorSystem::CreateFixture(const Vector2 position, const Size size)
{
	return CreateFixture(position, size, _current_fixture_id++);
}

Fixture* SectorSystem::CreateFixture(const Vector2 position, const Size size, uint64_t fixture_id)
{
	if (_fixtures.find(fixture_id) != _fixtures.end())
		throw std::exception("duplicate fixture id");

	Fixture* fixture = new Fixture();
	fixture->direction = Direction::Max;
	fixture->position = position;
	fixture->size = size;
	fixture->id = fixture_id;
	fixture->sector_id = _sector_id;

	_fixtures[fixture->id] = fixture;

	return fixture;
}

void SectorSystem::RemoveFixture(const uint64_t fixture_id)
{
	auto iterator = _fixtures.find(fixture_id);

	if (iterator == _fixtures.end())
		return;
	// delete iterator->second->user_data
	delete iterator->second;
	_fixtures.erase(iterator);
}
Fixture* SectorSystem::GetFixture(const uint64_t fixture_id)
{
	auto iterator = _fixtures.find(fixture_id);

	if (iterator == _fixtures.end())
		return nullptr;

	return iterator->second;
}

FixtureLocation SectorSystem::CalculateCurrentLocation(Fixture* fixture)
{
	bool is_in = FixtureRectangleOverlap(FixtureRectangle(_inner_box_position, _inner_box), fixture->ToFixtureRectangle());
	if (is_in)
		return FixtureLocation::In;

	bool is_gray = FixtureRectangleOverlap(FixtureRectangle(_base_position, _size), fixture->ToFixtureRectangle());
	if (is_gray)
		return FixtureLocation::Gray;

	return FixtureLocation::Leave;
}

Direction SectorSystem::CalculateCurrentPhase(Fixture* fixture)
{
	Size fixture_sector_ratio = Size{ (fixture->position.x - _base_position.x) / _direction_ratio.x, (fixture->position.y - _base_position.y) / _direction_ratio.y };

	if (fixture_sector_ratio.x < 0)
		fixture_sector_ratio.x = 0;

	if (fixture_sector_ratio.x > 2)
		fixture_sector_ratio.x = 2;

	if (fixture_sector_ratio.y < 0)
		fixture_sector_ratio.y = 0;

	if (fixture_sector_ratio.y > 2)
		fixture_sector_ratio.y = 2;

	return PhaseGrid[2 - int32_t(std::abs(fixture_sector_ratio.y))][int32_t(std::abs(fixture_sector_ratio.x))];
}

void SectorSystem::Update(const utility::Milliseconds current_time, const utility::Milliseconds delta_time)
{
	for (std::pair<const uint64_t, Fixture*>& pair : _fixtures)
	{
		Fixture* fixture = pair.second;

		fixture->Move(delta_time);

		if (fixture->is_observing_fixture)
			continue;

		if (pair.second->last_transform_time + TransformCooltime > current_time.count())
			continue;
		
		switch (fixture->location)
		{
		case FixtureLocation::In:
		{
			FixtureLocation current_location = CalculateCurrentLocation(fixture);

			if (current_location != fixture->location)
			{
				Direction current_phase = CalculateCurrentPhase(fixture);
				
				_callback->OnChangeFixtureLocation(_sector_id, fixture, current_location, current_phase);

				fixture->phase = current_phase;
				fixture->location = current_location;
			}

			break;
		}
		case FixtureLocation::Gray:
		{
			FixtureLocation current_location = CalculateCurrentLocation(fixture);
			Direction current_phase = CalculateCurrentPhase(fixture);

			if (current_location != fixture->location)
			{
				if (current_location == FixtureLocation::In)
					current_phase = Direction::Max;

				_callback->OnChangeFixtureLocation(_sector_id, fixture, current_location, current_phase);
				
				fixture->location = current_location;
				fixture->phase = current_phase;

				break;
			}
			
			if (current_phase != fixture->phase)
			{
				if (current_phase != Direction::Max)
					_callback->OnChangeFixturePhase(_sector_id, fixture, current_phase);

				fixture->phase = current_phase;
				break;
			}
		}
		}
	}
}

const std::unordered_map<uint64_t, Fixture*>& SectorSystem::GetAllFixtures()
{
	return _fixtures;
}

uint64_t SectorSystem::SectorId()
{
	return _sector_id;
}