#pragma once

#include "../common.h"
#include <atomic>
#include <concepts>
#include <new>

#define CACHE_ALIGN alignas(std::hardware_constructive_interference_size) 

constexpr static inline unsigned long long CACHELINE_SIZE = std::hardware_constructive_interference_size;
constexpr static inline unsigned long long DEBUG_BIT = 0xDDDDDDDDDDDDDDDD;

template <typename T>
concept RawMemoryAllocatorConcept =
    requires(T type, const size_t size) {
        { T::allocate(size) } -> std::same_as<void*>;
    } &&
        requires(T type, void* const ptr) {
            { T::deallocate(ptr) };
    };

template <typename T, typename Data>
concept ObjectInitializerConcept =
    requires(T type, Data* const data, const uint64_t id, const uint64_t param) {
        { T::Initialize(data, id, param) };
    };

template <typename T, typename Data>
concept ObjectDestructorConcept =
    requires(T type, Data* const data) {
        { T::Destruct(data) };
};

class CommonRawMemoryAllocator
{
public:
    static void* allocate(const size_t size)
    {
        return malloc(size);
    }

    static void deallocate(void* const ptr)
    {
        free(ptr);
    }
};

class CacheAlignedMemoryAllocator
{
public:
    static void* allocate(const size_t size)
    {
        return _aligned_malloc(size, CACHELINE_SIZE);
    }

    static void deallocate(void* const ptr)
    {
        _aligned_free(ptr);
    }
};

template <typename T>
class EmptyInitializer
{
public:
    static void Initialize(T* const object, const uint64_t id, const uint64_t param) {}
};

template <typename T>
class EmptyDestructor
{
public:
    static void Destruct(T* const object) {}
};

struct LowDebugBit
{
    uint64_t low_debug_bit = DEBUG_BIT;
};

struct HighDebugBit
{
    uint64_t high_debug_bit = DEBUG_BIT;
};
