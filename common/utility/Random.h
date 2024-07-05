#pragma once

#include <random>
#include <concepts>
#include <array>
#include <string>
#include <xhash>

namespace utility
{
	template <uint8_t T>
	concept DevidedBy8Byte = (T == 8 || T == 16 || T == 24 || T == 32 || T == 40 || T == 48 || T == 56 || T == 64);

	inline uint64_t RandomUint64()
	{
		static thread_local std::mt19937_64 rd;
		return rd();
	}

	template <uint8_t Size> requires DevidedBy8Byte<Size>
	inline void FillRandom(void* buffer)
	{
		for (uint32_t index = 0; index < Size; index += 8)
		{
			*reinterpret_cast<uint64_t*>(static_cast<char*>(buffer) + index) = RandomUint64();
		}
	}

	template <uint8_t KEY_SIZE>
	class RandomBuffer final
	{
	public:
		using ThisType = RandomBuffer<KEY_SIZE>;
		using BufferType = std::array<uint8_t, KEY_SIZE>;

		BufferType _buffer;

		RandomBuffer()
		{
			FillRandom<KEY_SIZE>(_buffer.data());
		}

		constexpr RandomBuffer(const decltype(_buffer)& buffer) : _buffer(buffer) { }

		const uint8_t* Raw() const
		{
			return _buffer.data();
		}

		bool operator==(const BufferType& buffer) const
		{
			return std::memcmp(_buffer.data(), buffer.data(), KEY_SIZE) == 0;
		}

		bool operator==(const ThisType& another) const
		{
			return std::memcmp(_buffer.data(), another._buffer.data(), KEY_SIZE) == 0;
		}

		bool operator>(const ThisType& another) const
		{
			return std::memcmp(_buffer.data(), another._buffer.data(), KEY_SIZE) > 0;
		}

		static constexpr const RandomBuffer<KEY_SIZE>& Empty()
		{
			static constexpr BufferType zero_buffer = {};
			static constexpr ThisType zero(zero_buffer);
			return zero;
		}
	};

	
}