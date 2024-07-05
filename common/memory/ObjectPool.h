#pragma once

#include "Allocator.h"
#include "Buffer.h"
#include "ConcurrentStack.h"
#include <atomic_queue/atomic_queue.h>
#include <vector>

template <typename T, bool CONCURRENT = false,
	ObjectInitializerConcept<T> Initializer = EmptyInitializer<T>,
	ObjectDestructorConcept<T> Destructor = EmptyDestructor<T>
>
	requires std::constructible_from<T>
class ObjectPool final
{
private:

#ifdef _DEBUG
	struct ObjectBlock : public LowDebugBit, public T, public HighDebugBit
	{
		void* pool_address = nullptr;
		std::atomic<uint8_t> in_used = 0;
		bool is_heap_allocation = false;
	};

	std::conditional_t<CONCURRENT, ConcurrentStack<ObjectBlock*>*, std::deque<ObjectBlock*>*> _pool;
#else

	struct ObjectBlock_None : public T
	{
		void* pool_address = nullptr;
		bool is_heap_allocation = false;
	};

	struct alignas(CACHELINE_SIZE) ObjectBlock_Aligned : public T
	{
		void* pool_address = nullptr;
		bool is_heap_allocation = false;
	};

	using ObjectBlock = std::conditional_t<CONCURRENT, ObjectBlock_Aligned, ObjectBlock_None>;

	// std::conditional_t<CONCURRENT, atomic_queue::AtomicQueueB<ObjectBlock*>*, std::deque<ObjectBlock*>*> _pool;
	std::conditional_t<CONCURRENT, ConcurrentStack<ObjectBlock*>*, std::deque<ObjectBlock*>*> _pool;
#endif

	using Allocator = std::conditional_t<CONCURRENT, CacheAlignedMemoryAllocator, CommonRawMemoryAllocator>;

	std::vector<ObjectBlock*> _object_list;

	void AllocateChunk(uint64_t& sequence, const uint64_t chunk_size, const uint64_t initializer_param)
	{
		ObjectBlock* chunk_ptr = reinterpret_cast<ObjectBlock*>(Allocator::allocate(sizeof(ObjectBlock) * chunk_size));

		for (uint32_t chunk_index = 0; chunk_index < chunk_size; chunk_index++)
		{
			ObjectBlock* block = chunk_ptr + chunk_index;
			
			// 명시적 생성자 호출
			if constexpr (std::is_constructible_v<T>)
				new(block) ObjectBlock();
			
			Initializer::Initialize(block, sequence++, initializer_param);

			block->pool_address = this;
			// _object_list.push_back(block);
			PushObject(block);
		}
	}

	std::atomic<uint64_t> _object_sequence;
	uint64_t _initializer_parameter;

private:
	void PushObject(ObjectBlock* const object)
	{
		if constexpr (CONCURRENT)
		{
// #ifdef _DEBUG
			_pool->push(object);
/*
#else
			bool result = _pool->try_push(object);
			if (result)
				return;

			throw std::exception("fatal error");
#endif
*/
		}
		else
		{
			_pool->push_back(object);
		}
	}

	bool PopObject(ObjectBlock*& result)
	{
		if constexpr (CONCURRENT)
		{
			return _pool->try_pop(result);
		}
		else
		{
			if (!_pool->empty())
			{
				result = _pool->back();

				_pool->pop_back();

				return true;
			}

			return false;
		}
	}

public:
	ObjectPool(const uint32_t pool_size, const uint32_t chunk_size, const uint64_t initializer_param = 0, const uint64_t start_sequence = 1)
		: _initializer_parameter(initializer_param)
	{
		assert(chunk_size > 0);

// #ifdef _DEBUG
		_pool = new std::remove_pointer_t<decltype(_pool)>;
/*
#else
		if constexpr (CONCURRENT)
			_pool = new std::remove_pointer_t<decltype(_pool)>(pool_size);
		else
			_pool = new std::remove_pointer_t<decltype(_pool)>;
#endif
*/
		uint64_t object_sequence = start_sequence;
		for (uint32_t size = 0; size < pool_size; size += chunk_size)
		{
			uint32_t allocation_size = chunk_size;
			if (size + chunk_size > pool_size)
				allocation_size = pool_size - size;

			AllocateChunk(object_sequence, allocation_size, _initializer_parameter);
		}

		_object_sequence = object_sequence;
	}

	ObjectPool(const ObjectPool<T, CONCURRENT, Initializer, Destructor>&) = delete;
	ObjectPool<T, CONCURRENT, Initializer, Destructor>& operator=(const ObjectPool<T, CONCURRENT, Initializer, Destructor>&) = delete;

	static void PushDirect(T* const object)
	{
		ObjectBlock* block = static_cast<ObjectBlock*>(object);
		auto pool = reinterpret_cast<ObjectPool<T, CONCURRENT, Initializer, Destructor>*>(block->pool_address);
		pool->Push(object);
	}

	void Push(T* const object)
	{
		ObjectBlock* block = static_cast<ObjectBlock*>(object);

#ifdef _DEBUG
		assert(block->pool_address == this);
		
		uint8_t expected = 1;
		assert(block->low_debug_bit == DEBUG_BIT);
		assert(block->high_debug_bit == DEBUG_BIT);
		assert(block->in_used.compare_exchange_strong(expected, 0, std::memory_order_acq_rel));
#endif

		if (block->is_heap_allocation)
		{
			Destructor::Destruct(block);
			delete block;
			return;
		}

		PushObject(block);
	}

	T* Pop(bool must_allocation = false)
	{
		ObjectBlock* ret = nullptr;
		if (PopObject(ret))
		{

#ifdef _DEBUG
			uint8_t expected = ret->in_used.load(std::memory_order_acquire);
			assert(ret->in_used.compare_exchange_strong(expected, 1, std::memory_order_acq_rel));
#endif

			return static_cast<T*>(ret);
		}

		if (must_allocation)
		{
			ret = new ObjectBlock();
			
			Initializer::Initialize(ret, _object_sequence++, _initializer_parameter);

			ret->pool_address = this;
			ret->is_heap_allocation = true;

#ifdef _DEBUG
			ret->in_used = 1;
#endif

			return ret;
		}

		return nullptr;
	}
};