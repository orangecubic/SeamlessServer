#pragma once

#include <array>
#include <cassert>
#include <concepts>

struct DynamicBuffer
{
	unsigned char* ptr;
	const unsigned int capacity = 0;
	unsigned int length;
};

template <typename T> requires std::derived_from<T, DynamicBuffer>
class DynamicBufferCursor
{
private:
	mutable T* _buffer;
	mutable int32_t _cursor;

public:
	DynamicBufferCursor(T* const buffer, const int32_t cursor) : _buffer(buffer), _cursor(cursor) {}
	DynamicBufferCursor(T* const buffer) : DynamicBufferCursor(buffer, 0) {}
	DynamicBufferCursor(nullptr_t) : DynamicBufferCursor(nullptr, 0) {}

	unsigned char* Data() const
	{
		return _buffer->ptr + _cursor;
	}

	void Seek(const int32_t byte_movement) const
	{
		_cursor += byte_movement;

		assert(_cursor >= 0 && _buffer->length >= _cursor);
	}

	uint32_t RemainBytes() const
	{
		return _buffer->length - _cursor;
	}

	uint32_t Cursor() const
	{
		return _cursor;
	}

	operator T* () const
	{
		return _buffer;
	}

	T* operator-> () const
	{
		return _buffer;
	}
};