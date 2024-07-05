#pragma once

#include <cassert>
#include <concepts>
#include <type_traits>
#include <cstring>
#include <vector>

template<typename T>
concept BufferDataConcept = std::is_scalar<T>().value == true;

template <BufferDataConcept T>
class SequentialBuffer final
{
private:
    static constexpr uint32_t DATA_SIZE = sizeof(T);
    uint32_t _rewinding_size;
    uint32_t _starting_index;
    T* _buffer;

    uint32_t _capacity;
    uint32_t _count;

public:
    SequentialBuffer(unsigned int capacity)
    {
        _count = _starting_index = 0;
        _capacity = capacity;
        _rewinding_size = capacity / 2;
        
        _buffer = (T*)malloc(DATA_SIZE * _capacity);
    }
    SequentialBuffer(const SequentialBuffer&) = delete;

    void PushBack(const T& data)
    {
        if (_capacity == _starting_index + _count)
        {
            uint32_t new_capacity = _capacity * 2;
            T* new_buffer = (T*)malloc(DATA_SIZE * new_capacity);

            std::memcpy(new_buffer, _buffer + _starting_index, DATA_SIZE * _count);

            _starting_index = 0;

            free(_buffer);

            _buffer = new_buffer;
            _capacity = new_capacity;
            _rewinding_size = _capacity / 2;
        }
        _buffer[_starting_index + _count++] = data;
    }

    template <typename Stream> requires ReadableStream<Stream>
    int RecvFromStream(Stream* stream)
    {
        int ret = stream->recv(End(), _capacity - (_starting_index + _count), 0);
        if (ret > 0)
            _count += ret;

        return ret;
    }

    void EraseFront(uint32_t count)
    {
        assert(_count >= count);
        _count -= count;
        _starting_index += count;

        if (_starting_index >= _rewinding_size) {
            std::memcpy(_buffer, _buffer + _starting_index, DATA_SIZE * (_capacity - _starting_index));
            _starting_index = 0;
        }

        if (_count == 0) {
            _starting_index = 0;
        }
    }

    T* Begin()
    {
        return _buffer + _starting_index;
    }
    T* End()
    {
        return _buffer + _starting_index + _count;
    }

    // foreach compatible
    T* begin()
    {
        return Begin();
    }
    T* end()
    {
        return End();
    }
    // foreach compatible

    uint32_t GetIndex(T* iterator)
    {
        return (uint32_t)(uint64_t(iterator) - uint64_t(Begin())) / DATA_SIZE;
    }

    uint32_t Size()
    {
        return _count;
    }

    T& operator [](uint32_t index)
    {
        assert(index < _starting_index + _count);
        return _buffer[_starting_index + index];
    }

    ~SequentialBuffer()
    {
        free(_buffer);
    }
};