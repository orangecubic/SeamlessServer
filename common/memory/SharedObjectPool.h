#pragma once

#include "Allocator.h"
#include "Buffer.h"
#include "ConcurrentStack.h"
#include "IntrusivePtr.h"
#include <atomic_queue/atomic_queue.h>

template <typename T, ObjectInitializerConcept<T> Initializer = EmptyInitializer<T>>
	requires std::constructible_from<T> && IntrusivePtrElement<T>
class SharedObjectPool final
{
private:
#ifdef _DEBUG

	struct alignas(CACHELINE_SIZE) ObjectBlock : public LowDebugBit, public T, public HighDebugBit
	{
		void* pool_address = nullptr;
		bool is_heap_allocation = false;
	};

	ConcurrentStack<ObjectBlock*>* _pool;
#else

	struct alignas(CACHELINE_SIZE) ObjectBlock : public T
	{
		void* pool_address = nullptr;
		bool is_heap_allocation = false;
	};

	atomic_queue::AtomicQueueB<ObjectBlock*>* _pool;
		
#endif

	void AllocateChunk(uint64_t& sequence, const uint64_t chunk_size, const uint64_t initializer_param)
	{
		ObjectBlock* chunk_ptr = reinterpret_cast<ObjectBlock*>(CacheAlignedMemoryAllocator::allocate(sizeof(ObjectBlock) * chunk_size));

		for (uint32_t chunk_index = 0; chunk_index < chunk_size; chunk_index++)
		{
			ObjectBlock* block = chunk_ptr + chunk_index;

			// 명시적 생성자 호출
			if constexpr (std::is_constructible_v<T>)
				new(block) ObjectBlock();

			Initializer::Initialize(block, sequence++, initializer_param);

			block->pool_address = this;

			PushObject(block);
		}
	}

private:
	void PushObject(ObjectBlock* const object)
	{
		_pool->push(object);
	}

	bool PopObject(ObjectBlock*& result)
	{
		return _pool->try_pop(result);
	}

	void Push(T* const object)
	{
		ObjectBlock* block = static_cast<ObjectBlock*>(object);

		assert(block->pool_address == this);

#ifdef _DEBUG
		assert(block->low_debug_bit == DEBUG_BIT);
		assert(block->high_debug_bit == DEBUG_BIT);
#endif

		PushObject(block);
	}

	static void IntrusivePtrDeleter(T* object)
	{
		ObjectBlock* block = static_cast<ObjectBlock*>(object);
		auto pool = reinterpret_cast<SharedObjectPool<T, Initializer>*>(block->pool_address);
		pool->Push(object);
	}

public:
	SharedObjectPool(const uint32_t pool_size, const uint32_t chunk_size, const uint64_t initializer_param = 0, const uint32_t start_sequence = 1)
	{
		assert(chunk_size > 0);

#ifndef _DEBUG
		_pool = new std::remove_pointer_t<decltype(_pool)>(pool_size);
#else
		_pool = new std::remove_pointer_t<decltype(_pool)>;
#endif

		uint64_t sequence = start_sequence;
		for (uint32_t size = 0; size < pool_size; size += chunk_size)
		{
			uint32_t allocation_size = chunk_size;
			if (size + chunk_size > pool_size)
				allocation_size = pool_size - size;

			AllocateChunk(sequence, allocation_size, initializer_param);
		}
	}

	SharedObjectPool(const SharedObjectPool<T, Initializer>&) = delete;
	SharedObjectPool<T, Initializer>& operator=(const SharedObjectPool<T, Initializer>&) = delete;

	IntrusivePtr<T> Pop()
	{
		ObjectBlock* ret = nullptr;
		if (PopObject(ret))
			return IntrusivePtr<T>(static_cast<T*>(ret), IntrusivePtrDeleter);

		return IntrusivePtr<T>(nullptr);
	}
};