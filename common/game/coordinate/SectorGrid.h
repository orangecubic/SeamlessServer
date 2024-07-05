#pragma once

#include "Base.h"
#include <vector>
#include <array>

inline constexpr FixtureRectangle GetSectorFixtureRectangle(const uint64_t sector_id)
{
	SectorInfo sector_info = SectorGridList[sector_id];

	return FixtureRectangle(Vector2{ (sector_info.grid_index.column - CenterSector.grid_index.column) * SectorSize.x, (CenterSector.grid_index.row - sector_info.grid_index.row) * SectorSize.y }, SectorSize);
}

inline constexpr FixtureRectangle GetSectorAbsFixtureRectangle(const uint64_t sector_id)
{
	FixtureRectangle rect = GetSectorFixtureRectangle(sector_id);
	Vector2 left_down = rect.LeftDown();

	left_down.x += CenterSector.grid_index.column * SectorSize.x;
	left_down.y += (SectorGridWidth - CenterSector.grid_index.row - 1) * SectorSize.y;

	return FixtureRectangle(left_down, SectorSize);
}

inline constexpr uint64_t GetSectorByPosition(const Vector2 position)
{
	constexpr FixtureRectangle center_rect = GetSectorFixtureRectangle(CenterSector.sector_id);

	// position 양수화
	Vector2 positive_position = { position.x + CenterSector.grid_index.column * SectorSize.x, position.y + CenterSector.grid_index.row * SectorSize.y };

	Indexer2 indexer{
		SectorGridHeight - static_cast<int>(positive_position.y / SectorSize.y) - 1, 
		static_cast<int>(positive_position.x / SectorSize.x)
	};

	if (indexer.row >= SectorGridHeight || indexer.row < 0 || indexer.column >= SectorGridWidth || indexer.column < 0)
		return INVALID_SECTOR;

	return SectorGrid[indexer.row][indexer.column].sector_id;
}

// TODO: 추후 인접 Sector를 컴파일 타임에 구하도록 최적화 예정
inline constexpr uint64_t GetNextSector(const uint64_t base_sector_id, const Direction direction)
{
	Indexer2 base_index = SectorGridList[base_sector_id].grid_index;

	Indexer2 direction_force = PhaseForce[static_cast<uint64_t>(direction)];
	Indexer2 array_force{ base_index.row - direction_force.row, base_index.column + direction_force.column };

	if (array_force.row < 0 || array_force.row >= SectorGridHeight || array_force.column < 0 || array_force.column >= SectorGridWidth)
		return INVALID_SECTOR;

	return SectorGrid[array_force.row][array_force.column].sector_id;
}

// TODO: 추후 인접 Sector를 컴파일 타임에 구하도록 최적화 예정
inline constexpr std::array<uint64_t, 3> GetNearSectors(const uint64_t base_sector_id, const Direction direction)
{
	std::array<uint64_t, 3> sectors{ INVALID_SECTOR, INVALID_SECTOR, INVALID_SECTOR };

	uint64_t index = 0;

	Indexer2 base_index = SectorGridList[base_sector_id].grid_index;

	for (const Direction near_direction : SectorNearDirectionGrid[static_cast<uint32_t>(direction)])
	{
		if (near_direction == Direction::Max)
			continue;

		uint64_t sector_id = GetNextSector(base_sector_id, near_direction);
		if (sector_id == INVALID_SECTOR)
			continue;

		sectors[index++] = sector_id;
	}

	return sectors;
}

inline Direction GetOpositeDirection(const Direction direction)
{
	int32_t oposite = static_cast<int32_t>(direction) + 4;
	if (oposite >= 8)
		oposite -= 8;

	return static_cast<Direction>(oposite);
}

inline bool IsNearPhase(const uint64_t origin_sector_id, const Direction origin_phase, const uint64_t dest_sector_id, const Direction dest_phase)
{
	auto origin_near_phase = SectorNearDirectionGrid[static_cast<uint64_t>(origin_phase)];
	auto dest_near_phase = SectorNearDirectionGrid[static_cast<uint64_t>(dest_phase)];

	bool is_origin_near_phase = false;
	for (const Direction phase : origin_near_phase)
	{
		if (GetNextSector(origin_sector_id, phase) == dest_sector_id)
		{
			is_origin_near_phase = true;
			break;
		}
	}

	bool is_dest_near_phase = false;
	for (const Direction phase : dest_near_phase)
	{
		if (GetNextSector(dest_sector_id, phase) == origin_sector_id)
		{
			is_dest_near_phase = true;
			break;
		}
	}

	return is_origin_near_phase && is_dest_near_phase;
}

constexpr FixtureRectangle WorldRect(GetSectorFixtureRectangle(CenterSector.sector_id).Center(), static_cast<Vector2>(SectorSize) * SectorGridWidth, 0.5f);