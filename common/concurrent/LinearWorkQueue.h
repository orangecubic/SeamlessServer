#pragma once

#include <atomic>
#include <queue>
#include <atomic_queue/atomic_queue.h>
#include "../memory/Allocator.h"

// Multi Provier / Single Consumer

template <typename T, std::size_t BUFFER_SIZE>
class LinearWorkQueue final
{
private:
	using Queue = atomic_queue::AtomicQueue<T, BUFFER_SIZE>;
	CACHE_ALIGN Queue* volatile _submission_queue;
	CACHE_ALIGN Queue* volatile _fetch_queue;

public:
	LinearWorkQueue()
	{
		_submission_queue = new (std::nothrow)Queue();
		if (_submission_queue == nullptr)
			throw std::bad_alloc();
		
		_fetch_queue = new (std::nothrow)Queue();
		if (_fetch_queue == nullptr)
		{
			delete _submission_queue;
			throw std::bad_alloc();
		}
	}

	LinearWorkQueue(const LinearWorkQueue<T, BUFFER_SIZE>&) = delete;
	LinearWorkQueue<T, BUFFER_SIZE>& operator=(const LinearWorkQueue<T, BUFFER_SIZE>&) = delete;

	// "Thread Unsafe"
	void LockSubmissionQueue()
	{
		std::swap(_submission_queue, _fetch_queue);

		std::atomic_thread_fence(std::memory_order_release);
	}

	bool TryPush(const T& value)
	{
		std::atomic_thread_fence(std::memory_order_acquire);

		return _submission_queue->try_push(value);
	}

	bool TryPop(T& result)
	{
		std::atomic_thread_fence(std::memory_order_acquire);

		return _fetch_queue->try_pop(result);
	}

	~LinearWorkQueue()
	{
		delete _submission_queue;
		delete _fetch_queue;
	}
};