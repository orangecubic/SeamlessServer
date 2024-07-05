#pragma once

#include <cstdint>

namespace utility
{
	union Union16
	{
		uint16_t fit;
		struct _8_8
		{
			uint8_t _1;
			uint8_t _2;
		};
	};

	union Union32
	{
		uint32_t fit;
		struct _16_16
		{
			Union16 _1;
			Union16 _2;
		};
	};

	union Union64
	{
		uint64_t fit;
		struct _32_32
		{
			Union32 _1;
			Union32 _2;
		} _32_32;
	};

}