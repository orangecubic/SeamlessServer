#pragma once

#include "Base.h"
#include "SectorGrid.h"
#include "../../utility/Time.h"
#include <xmmintrin.h>

inline bool FixtureRectangleOverlap(const FixtureRectangle& r1, const FixtureRectangle& r2)
{
	if (r1.up == r1.down || r1.left == r1.right || r2.up == r2.down || r2.left == r2.right)
		return false;

	if (r1.left > r2.right || r2.left > r1.right)
		return false;

	if (r1.down > r2.up || r2.down > r1.up)
		return false;

	return true;
}

enum class FixtureTracingEvent
{
	Create = 1,
	ChangeLocation,
	ChangePhase,
	CreateObservingObject,
	PromoteObject,
};

#ifdef _DEBUG


struct FixtureTracingHistory
{
	uint64_t sector_id;
	int64_t transform_time;
	Vector2 position;
	FixtureTracingEvent tracing_event;
	Direction phase;
	FixtureLocation location;
	bool is_observing_fixture;
};
#endif

struct Fixture
{
	uint64_t id = 0;

	Vector2 position;

	Vector2 size;

	CoordinateUnit force = ForceConstant;
	Direction direction = Direction::Up;
	FixtureLocation location = FixtureLocation::In;

	bool is_observing_fixture = false;
	Direction phase = Direction::Max;

	uint64_t sector_id;
	int64_t last_transform_time = 0;
	uint64_t personal_delta_time = 0;

	uint64_t user_data = 0;

#ifdef _DEBUG
	//std::vector<FixtureTracingHistory> tracing_history;
	//std::array<uint64_t, 3> observing_sectors;

	void CreateTracingHistory(const FixtureTracingEvent tracing_event, const int64_t transform_time)
	{
		// tracing_history.push_back({ sector_id, transform_time, position, tracing_event, phase, location, is_observing_fixture });
	}
#else
	inline void CreateTracingHistory(const FixtureTracingEvent tracing_event, const int64_t transform_time) {}
#endif

	//Fixture* parent;

	Vector2 Move(const utility::Milliseconds delta_time)
	{
		if (direction == Direction::Max)
			return {};

		CoordinateUnit time_ratio = static_cast<CoordinateUnit>(delta_time.count() + personal_delta_time) / 1000.0f;

		Indexer2 direction_force = PhaseForce[static_cast<uint64_t>(direction)];

		Vector2 movement{ force * time_ratio * direction_force.column, force * time_ratio * direction_force.row };

		Vector2 backup_position = position;

		position.x += movement.x;
		position.y += movement.y;

		FixtureRectangle after_rect = ToFixtureRectangle();

		if (WorldRect.left > after_rect.left)
			movement.x += WorldRect.left - after_rect.left;
		if (WorldRect.right < after_rect.right)
			movement.x -= after_rect.right - WorldRect.right;
		if (WorldRect.down > after_rect.down)
			movement.y += WorldRect.down - after_rect.down;
		if (WorldRect.up < after_rect.up)
			movement.y -= after_rect.up - WorldRect.up;

		position = backup_position + movement;

		personal_delta_time = 0;

		return movement;
	}

	FixtureRectangle ToFixtureRectangle()
	{
		return FixtureRectangle(position, size, 0.5);
	}
};


