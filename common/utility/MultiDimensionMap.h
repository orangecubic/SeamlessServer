#pragma once

#include <unordered_map>

namespace utility
{
	template <typename KeyElement, typename Val>
	class MultiDimensionMap
	{
	private:
		struct Node
		{
			Val value = {};
			bool value_enabled = false;

			std::unordered_map<KeyElement, Node> next_node_map;
		};

		Node root_node;

		void InsertNode(const KeyElement* const key_vector, const std::size_t size, const Val& value)
		{
			decltype(root_node)* current_node = &root_node;

			for (std::size_t index = 0; index < size; index++)
				current_node = &(current_node->next_node_map[key_vector[index]]);

			current_node->value = value;
			current_node->value_enabled = true;
		}

		bool FindValue(const KeyElement* const key_vector, const std::size_t size, Val& value)
		{
			decltype(root_node)* current_node = &root_node;

			for (std::size_t index = 0; index < size; index++)
			{
				auto node_iter = current_node->next_node_map.find(key_vector[index]);

				if (node_iter == current_node->next_node_map.end())
					return false;

				current_node = &node_iter->second;
			}

			if (current_node->value_enabled)
				value = current_node->value;

			return current_node->value_enabled;
		}

	public:
		void Insert(const KeyElement* const key_vector, const std::size_t size, const Val& value)
		{
			InsertNode(key_vector, size, value);
		}

		void Insert(const KeyElement& key, const Val& value)
		{
			Insert(&key, 1, value);
		}

		template <typename... Args>
		void InsertWithVariadicKey(const Val& value, Args... key_args)
		{
			KeyElement key_vector[] = { key_args... };

			Insert(key_vector, sizeof...(Args), value);
		}

		bool TryGet(const KeyElement* const key_vector, const std::size_t size, Val& value)
		{
			return FindValue(key_vector, size, value);
		}

		bool TryGet(const KeyElement& key, Val& value)
		{
			return TryGet(&key, 1, value);
		}

		template <typename... Args>
		bool TryGetWithVariadicKey(Val& value, Args... key_args)
		{
			KeyElement key_vector[] = { key_args... };

			return TryGet(key_vector, sizeof...(Args), value);
		}
	};
}
