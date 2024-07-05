#pragma once

#include <cassert>
#include <xmemory>

#define NONCOPYABLE(type) \
	type(const type&) = delete; \
	type& operator=(const type&) = delete;

#define NONCOPYABLE_T(class_name, template_type) \
	class_name(const template_type&) = delete; \
	template_type& operator=(const template_type&) = delete;

