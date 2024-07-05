#pragma once

#include <stdlib.h>
#include <atomic>
#include "../memory/Allocator.h"
#include "Time.h"

enum class UidType
{
	User = 1,
	Fixture = 2,
	Object = 3,
	Max = 4,
};

struct UidBlock
{
	CACHE_ALIGN std::atomic<uint64_t> current_number;
};

class UidGenerator
{
private:
	UidBlock* _block;
	size_t _block_size;

public:

	UidGenerator(const uint64_t server_id, const uint64_t seed_value)
	{
		utility::Seconds current_epoch_time = utility::CurrentTimeEpoch<utility::Seconds>();

		_block_size = static_cast<size_t>(UidType::Max) - 1;

#ifdef _WIN32
		_block = static_cast<UidBlock*>(_aligned_malloc(_block_size, _block_size * std::hardware_constructive_interference_size));
#else
		// _block = static_cast<UidBlock*>(posix_memalign(_block_size, _block_size * std::hardware_constructive_interference_size));
#endif
		// 184 467440 73709551615
		// 000 0000 
		for (uint32_t uid_type = static_cast<uint32_t>(UidType::User); uid_type < static_cast<uint32_t>(UidType::Max); uid_type++)
		{
			// _block[uid_type].current_number = 
		}
	}

	uint64_t Genarate(const UidType	uid_type)
	{

	}
};