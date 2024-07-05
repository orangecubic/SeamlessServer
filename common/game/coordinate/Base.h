#pragma once

#include <cmath>
#include <array>

using CoordinateUnit = float;

struct Vector2
{
	CoordinateUnit x;
	CoordinateUnit y;

	constexpr Vector2 operator*(const CoordinateUnit value)
	{
		return Vector2{ x * value, y * value };
	}

	constexpr Vector2 operator/(const CoordinateUnit value)
	{
		return Vector2{ x / value, y / value };
	}

	constexpr Vector2 operator+(const Vector2& another)
	{
		return Vector2{ x + another.x, y + another.y };
	}

};

using Size = Vector2;
using Vector = Vector2;
using AnchorPoint = Vector2;

struct FixtureRectangle
{
	CoordinateUnit up = 0;
	CoordinateUnit down = 0;
	CoordinateUnit left = 0;
	CoordinateUnit right = 0;

	constexpr FixtureRectangle(const float up, const float down, const float left, const float right) : up(up), down(down), left(left), right(right) {}
	constexpr FixtureRectangle(const Vector2 position, const Size size, CoordinateUnit anchor_point = 0 /*0 ~ 1*/)
	{
		up = position.y + size.y * (1 - anchor_point);
		down = position.y - size.y * anchor_point;
		left = position.x - size.x * anchor_point;
		right = position.x + size.x * (1 - anchor_point);
	}

	constexpr Vector2 LeftDown() const
	{
		return Vector2{ left, down };
	}

	constexpr Vector2 LeftUp() const
	{
		return Vector2{ left, up };
	}

	constexpr Vector2 RightDown() const
	{
		return Vector2{ right, down };
	}

	constexpr Vector2 RightUp() const
	{
		return Vector2{ right, up };
	}

	constexpr Vector2 Center() const
	{
		return Vector2{ left + (right - left) / 2, down + (up - down) / 2 };
	}
};

struct Indexer2
{
	int32_t row;
	int32_t column;
};

enum class Direction : uint8_t
{
	Up,
	UpRight,
	Right,
	DownRight,
	Down,
	DownLeft,
	Left,
	UpLeft,
	Max,
};

enum class FixtureLocation : uint8_t
{
	In,
	Gray,
	Leave,
	Max,
};

constexpr uint64_t INVALID_SECTOR = UINT64_MAX;

struct SectorInfo
{
	uint64_t sector_id = INVALID_SECTOR;
	Indexer2 grid_index{};
};

constexpr CoordinateUnit ChunkSize = 100;

// Chunk Count
constexpr Size SectorSize{ 50 * ChunkSize, 50 * ChunkSize };

// decide object delegation
constexpr CoordinateUnit SectorGrayZoneChunk = 10;

constexpr Size ChunkGrid{ SectorSize.x / ChunkSize, SectorSize.y / ChunkSize };

constexpr CoordinateUnit MaxAOI = 50;

constexpr CoordinateUnit ForceConstant = 800;

constexpr int32_t SectorGridWidth = 5;
constexpr int32_t SectorGridHeight = 5;

using SectorGridRow = std::array<SectorInfo, SectorGridHeight>;
using SectorGridType = std::array<SectorGridRow, SectorGridWidth>;

constexpr SectorGridType SectorGrid = {
	std::array<SectorInfo, SectorGridWidth> {
		SectorInfo{0, {0, 0}},
		SectorInfo{1, {0, 1}},
		SectorInfo{2, {0, 2}},
		SectorInfo{3, {0, 3}},
		SectorInfo{4, {0, 4}},
	},
	std::array<SectorInfo, SectorGridWidth> {
		SectorInfo{5, {1, 0}},
		SectorInfo{6, {1, 1}},
		SectorInfo{7, {1, 2}},
		SectorInfo{8, {1, 3}},
		SectorInfo{9, {1, 4}},
	},
	std::array<SectorInfo, SectorGridWidth> {
		SectorInfo{10, {2, 0}},
		SectorInfo{11, {2, 1}},
		SectorInfo{12, {2, 2}},
		SectorInfo{13, {2, 3}},
		SectorInfo{14, {2, 4}},
	},
	std::array<SectorInfo, SectorGridWidth> {
		SectorInfo{15, {3, 0}},
		SectorInfo{16, {3, 1}},
		SectorInfo{17, {3, 2}},
		SectorInfo{18, {3, 3}},
		SectorInfo{19, {3, 4}},
	},
	std::array<SectorInfo, SectorGridWidth> {
		SectorInfo{20, {4, 0}},
		SectorInfo{21, {4, 1}},
		SectorInfo{22, {4, 2}},
		SectorInfo{23, {4, 3}},
		SectorInfo{24, {4, 4}},
	},
};

constexpr std::array<SectorInfo, SectorGridWidth* SectorGridHeight> SectorGridList{
	SectorGrid[0][0], SectorGrid[0][1], SectorGrid[0][2], SectorGrid[0][3], SectorGrid[0][4],
	SectorGrid[1][0], SectorGrid[1][1], SectorGrid[1][2], SectorGrid[1][3], SectorGrid[1][4],
	SectorGrid[2][0], SectorGrid[2][1], SectorGrid[2][2], SectorGrid[2][3], SectorGrid[2][4],
	SectorGrid[3][0], SectorGrid[3][1], SectorGrid[3][2], SectorGrid[3][3], SectorGrid[3][4],
	SectorGrid[4][0], SectorGrid[4][1], SectorGrid[4][2], SectorGrid[4][3], SectorGrid[4][4],
};

constexpr std::array<std::array<Direction, 3>, static_cast<uint32_t>(Direction::Max) + 1> SectorNearDirectionGrid{
	std::array<Direction, 3>{Direction::Max, Direction::Up, Direction::Max},
	{Direction::Up, Direction::UpRight, Direction::Right},
	{Direction::Max, Direction::Right, Direction::Max},
	{Direction::Right, Direction::DownRight, Direction::Down},
	{Direction::Max, Direction::Down, Direction::Max},
	{Direction::Down, Direction::DownLeft, Direction::Left},
	{Direction::Max, Direction::Left, Direction::Max},
	{Direction::Left, Direction::UpLeft, Direction::Up},
	std::array<Direction, 3>{Direction::Max, Direction::Max, Direction::Max},
};

constexpr std::array<std::array<Direction, 3>, static_cast<uint32_t>(Direction::Max) + 1> SectorOpositeDirectionGrid{
	std::array<Direction, 3>{Direction::Down, Direction::Max, Direction::Max},
	{Direction::DownLeft, Direction::DownRight, Direction::Left},
	{Direction::Left, Direction::Max, Direction::Max},
	{Direction::DownRight, Direction::UpLeft, Direction::UpRight} ,
	{Direction::Max, Direction::Down, Direction::Max},
	{Direction::Down, Direction::DownLeft, Direction::Left},
	{Direction::Max, Direction::Left, Direction::Max},
	{Direction::Left, Direction::UpLeft, Direction::Up},
	std::array<Direction, 3>{Direction::Max, Direction::Max, Direction::Max},
};

constexpr std::array<Indexer2, static_cast<size_t>(Direction::Max) + 1> PhaseForce = {
	Indexer2{1, 0}, // up
	{1, 1},
	{0, 1},
	{-1, 1},
	{-1, 0},
	{-1, -1},
	{0, -1},
	{1, -1},
	{0, 0}, // max
};

constexpr std::array<std::array<Direction, 3>, 3> PhaseGrid = {
	std::array<Direction, 3>{Direction::UpLeft, Direction::Up, Direction::UpRight},
	std::array<Direction, 3>{Direction::Left, Direction::Max, Direction::Right},
	std::array<Direction, 3>{Direction::DownLeft, Direction::Down, Direction::DownRight}
};

static_assert(SectorGridWidth == SectorGridHeight);
static_assert(SectorGridWidth % 2 == 1);

constexpr SectorInfo CenterSector = SectorGrid[SectorGridWidth / 2][SectorGridHeight / 2];