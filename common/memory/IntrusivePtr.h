#pragma once

#include "Allocator.h"
#include <atomic>
#include <concepts>

template <typename T>
concept IntrusivePtrElement =
	requires(T type) {
		{ type.Retain() } -> std::same_as<int32_t>;
	}&&
	requires(T type) {
		{ type.Release() } -> std::same_as<int32_t>;
	}&&
	requires(T type) {
		{ type.Ref() } -> std::same_as<int32_t>;
	};

template <typename T> requires IntrusivePtrElement<T>
class IntrusivePtr
{
public:
	using Deleter = void(*)(T* ptr);

private:
	mutable T* _ptr = nullptr;
	Deleter _deleter = nullptr;

public:

	explicit IntrusivePtr() : IntrusivePtr(nullptr) {}

	explicit IntrusivePtr(nullptr_t) : _ptr(nullptr), _deleter(nullptr) {}

	explicit IntrusivePtr(T* const ptr, const Deleter deleter) : _ptr(ptr), _deleter(deleter)
	{
		_ptr->Retain();
	}

	IntrusivePtr(const IntrusivePtr<T>& copy_ptr)
	{
		*this = copy_ptr;
	}

	IntrusivePtr(IntrusivePtr<T>&& move_ptr)
	{
		*this = std::move(move_ptr);
	}

	IntrusivePtr<T>& operator=(const IntrusivePtr<T>& copy_ptr)
	{
		if (copy_ptr._ptr == nullptr)
		{
			Release();
			return *this;
		}

		copy_ptr->Retain();
		Release();

		_ptr = copy_ptr._ptr;
		_deleter = copy_ptr._deleter;

		return *this;
	}

	IntrusivePtr<T>& operator=(IntrusivePtr<T>&& move_ptr)
	{
		if (move_ptr._ptr == nullptr)
		{
			Release();
			return *this;
		}

		Release();

		_ptr = move_ptr._ptr;
		_deleter = move_ptr._deleter;

		move_ptr._ptr = nullptr;

		return *this;
	}

	/*
	operator bool() const
	{
		return _ptr != nullptr;
	}
	*/

	bool Valid() const
	{
		return _ptr != nullptr;
	}

	T* operator->() const
	{
		return _ptr;
	}

	int32_t Ref() const
	{
		return _ptr->Ref();
	}

	void Release()
	{
		if (_ptr != nullptr)
		{
			int32_t result = _ptr->Release();

			if (result == 0)
			{
				_deleter(_ptr);
			}

			_ptr = nullptr;
			_deleter = nullptr;
		}
	}

	template <typename NewType> requires std::derived_from<T, NewType> || std::derived_from<NewType, T>
	IntrusivePtr<NewType> Cast(void(*deleter)(NewType* ptr))
	{
		return IntrusivePtr<NewType>(static_cast<NewType>(_ptr), deleter);
	}

	void _UnsafeRetain()
	{
		_ptr->Retain();
	}

	void _UnsafeRelease()
	{
		int32_t result = _ptr->Release();

		if (result == 0)
		{
			_deleter(_ptr);
			_ptr = nullptr;
		}
	}

	T* _UnsafePtr()
	{
		return _ptr;
	}

	~IntrusivePtr()
	{
		Release();
	}
};