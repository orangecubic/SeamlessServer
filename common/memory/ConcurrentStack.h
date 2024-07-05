#pragma once

#include <stack>
#include "SpinLock.h"

// Concurrent Stack using spinlock
template <typename T>
class ConcurrentStack final
{
private:
	std::stack<T> _stack;
	SpinLock _spin_lock;

public:
	ConcurrentStack() {}
	NONCOPYABLE_T(ConcurrentStack, ConcurrentStack<T>)

	void push(const T& value)
	{
		_spin_lock.Lock();

		_stack.push(value);

		_spin_lock.Unlock();
	}

	bool try_pop(T& result)
	{
		_spin_lock.Lock();

		if (_stack.empty())
		{
			_spin_lock.Unlock();
			return false;
		}

		result = _stack.top();
		_stack.pop();

		_spin_lock.Unlock();

		return true;
	}
};
