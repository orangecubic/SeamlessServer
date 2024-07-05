#pragma once

#include "Fixture.h"
#include "../../utility/Time.h"
#include <unordered_map>

constexpr int64_t TransformCooltime = 1000;

class SectorSystem
{
public:
	class Callback
	{
	public:
		virtual void OnChangeFixturePhase(const uint64_t sector_id, Fixture* const fixture, const Direction phase) = 0;
		virtual void OnChangeFixtureLocation(const uint64_t sector_id, Fixture* const fixture, const FixtureLocation location, const Direction phase) = 0;
		virtual void OnCollisionFixture(const uint64_t sector_id, Fixture* const fixture1, Fixture* const fixture2) = 0;
	};

private:
	uint64_t _sector_id;
	std::unordered_map<uint64_t, Fixture*> _fixtures;

	uint64_t _current_fixture_id;

	const Vector2 _base_position;
	const Size _size;
	const CoordinateUnit _chunk_size;
	const CoordinateUnit _gray_zone_chunk;

	Size _direction_ratio;

	Size _inner_box;
	Vector2 _inner_box_position;

	Callback* _callback;
public:

	SectorSystem(const uint64_t server_id, const uint64_t sector_id, const Vector2 base_position, const Size sector_size, const CoordinateUnit chunk_size, const CoordinateUnit gray_zone_chunk, Callback* const callback);

	Fixture* CreateFixture(const Vector2 position, const Size size);
	Fixture* CreateFixture(const Vector2 position, const Size size, uint64_t fixture_id);
	Fixture* CreateFixture(const Fixture& origin);
	void RemoveFixture(const uint64_t fixture_id);
	Fixture* GetFixture(const uint64_t fixture_id);
	const std::unordered_map<uint64_t, Fixture*>& GetAllFixtures();
	uint64_t SectorId();

	FixtureLocation CalculateCurrentLocation(Fixture* fixture);
	Direction CalculateCurrentPhase(Fixture* fixture);

	void Update(const utility::Milliseconds current_time, const utility::Milliseconds delta_time);

};