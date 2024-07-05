#pragma once

#include <xmemory>

enum class ObjectType : uint8_t
{
	Monster,
	Character,
	Prop,
	Wall,
};

enum class ObjectState : uint8_t
{
	Appeared,
	Stop,
	Move,
	Motion,
	Dead,
};