#pragma once

#include <unordered_map>
#include <shared_mutex>
#include <functional>

template <typename Key, typename Value>
class ConcurrentMap
{
private:
	std::unordered_map<Key, Value> _map;
	std::shared_mutex _mutex;

public:

	// return true if overwrite previous value
	bool Insert(const Key& key, const Value& value)
	{
		std::lock_guard<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
		{
			_map[key] = value;
			return false;
		}
		
		iterator->second = value;

		return true;
	}

	bool TryGet(const Key& key, Value& result)
	{
		std::shared_lock<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
			return false;

		result = iterator->second;

		return true;
	}

	template <typename F>
	bool Process(const Key& key, F processor)
	{
		std::shared_lock<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
			return false;

		processor(iterator->second);

		return true;
	}

	template <typename F>
	bool RemoveIf(const Key& key, Value& value, F processor)
	{
		std::lock_guard<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
			return false;

		if (processor(iterator->second))
		{
			value = iterator->second;
			_map.erase(iterator);
			return true;
		}

		return false;
	}

	bool Remove(const Key& key, Value& value)
	{
		std::lock_guard<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
			return false;

		value = iterator->second;
		_map.erase(iterator);

		return true;
	}

	void Remove(const Key& key)
	{
		std::lock_guard<std::shared_mutex> guard(_mutex);

		auto iterator = _map.find(key);
		if (iterator == _map.end())
			return;

		_map.erase(iterator);
	}
};