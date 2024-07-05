#pragma once

#include "../define.h"
#include "Vital.h"
#include "../coordinate/Fixture.h"
#include "../../utility/Time.h"
#include <chrono>
#include <ylt/struct_pack.hpp>

class Object
{
private:
	uint64_t _object_id;
	uint64_t _sector_id;

	ObjectType _object_type;
	ObjectState _object_state;

	Vital _vital;
	Fixture _fixture;


public:

	Object(const uint64_t _object_id, const uint64_t _sector_id, const ObjectType _object_type, const ObjectState _object_state, const Vital& _vital, const Fixture& _fixture)
		: _object_id(_object_id), _sector_id(_sector_id), _object_type(_object_type), _object_state(_object_state), _vital(_vital), _fixture(_fixture) {}

	Vital* GetVital()
	{
		return &_vital;
	}

	Fixture* GetFixture()
	{
		return &_fixture;
	}

	uint64_t GetId()
	{
		return _object_id;
	}

	uint64_t GetSectorId()
	{
		return _sector_id;
	}

	ObjectType GetType()
	{
		return _object_type;
	}

	virtual void Update(const utility::Milliseconds current_time, const utility::Milliseconds delta) {}
};

// STRUCT_PACK_REFL(Object, _object_id, _sector_id, _object_type, _object_state, _vital, _fixture);

class ObservedObject : public Object
{
};