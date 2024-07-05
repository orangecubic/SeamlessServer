#pragma once

#include "../common.h"
#include <atomic>

class SpinLock final
{
private:
	std::atomic_flag _spinlock_flag = ATOMIC_FLAG_INIT;

public:
	inline void Lock()
	{
		while (_spinlock_flag.test_and_set(std::memory_order_acquire))
		{
#if defined(__cpp_lib_atomic_flag_test)
			while (_spinlock_flag.test(std::memory_order_relaxed));
#endif
		}
	}

	inline void Unlock()
	{
		_spinlock_flag.clear(std::memory_order_release);
	}
};