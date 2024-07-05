#pragma once

#include "Allocator.h"
#include <atomic>
#include <new>

class AlignedRef
{
public:
	CACHE_ALIGN std::atomic<int32_t> _count = 0;
};

class Ref
{
public:
	std::atomic<int32_t> _count = 0;
};

template <bool CACHE_ALIGNED = true>
class RefCounter
{
private:
	std::conditional_t<CACHE_ALIGNED, AlignedRef, Ref> _ref;

public:

	bool CAS(int32_t expected, const int32_t desired)
	{
		return _ref._count.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
	}

	int32_t Retain()
	{
		return _ref._count.fetch_add(1, std::memory_order_acq_rel) + 1;
	}

	int32_t RetainN(const int32_t count)
	{
		return _ref._count.fetch_add(count, std::memory_order_acq_rel) + count;
	}

	int32_t Release()
	{
		return _ref._count.fetch_add(-1, std::memory_order_acq_rel) - 1;
	}

	int32_t ReleaseN(const int32_t count)
	{
		return _ref._count.fetch_add(-count, std::memory_order_acq_rel) - count;
	}

	int32_t Ref()
	{
		return _ref._count.load(std::memory_order_acquire);
	}
};

class SingleThreadRefCounter
{
private:
	int32_t _count = 0;

public:

	bool CAS(int32_t expected, const int32_t desired)
	{
		if (_count == expected)
		{
			_count = desired;
			return true;
		}

		return false;
	}

	int32_t Retain()
	{
		return ++_count;
	}

	int32_t RetainN(const int32_t count)
	{
		_count += count;
		return _count;
	}

	int32_t Release()
	{
		return --_count;
	}

	int32_t ReleaseN(const int32_t count)
	{
		_count -= count;
		return _count;
	}

	int32_t Ref() const
	{
		return _count;
	}
};