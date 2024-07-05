#pragma once

#include "../concurrent/LinearWorkQueue.h"
#include "../memory/ObjectPool.h"
#include "../memory/Buffer.h"
#include "Time.h"
#include <source_location>
#include <thread>
#include <sstream>
#include <iostream>

namespace utility
{
	enum class LogLevel : uint8_t
	{
		Info = 0,
		Warn,
		Error,
		Fatal,
		Max,
	};
	
	struct Log
	{
		Milliseconds time = Milliseconds(0);
		std::source_location location;
		DynamicBuffer message;
		std::thread::id thread_id;
		LogLevel log_level;
	};

	class LogBufferInitializer
	{
	public:
		static void Initialize(Log* const log, const uint64_t id, const uint64_t param)
		{
			log->message.ptr = new unsigned char[param];
			log->message.length = 0;

			const uint32_t& capacity_ref = log->message.capacity;
			const_cast<uint32_t&>(capacity_ref) = static_cast<uint32_t>(param);
		}
	};

	using LogBufferPool = ObjectPool<Log, true, LogBufferInitializer>;
	using LogQueue = atomic_queue::AtomicQueueB<Log*>;


	class Logger
	{
	private:
		std::thread* _logger_routine;

		std::vector<LogBufferPool*> _buffer_pool;
		std::vector<LogQueue*> _log_queue;

		uint32_t _max_log_size;
		uint32_t _parallelism;

		void LoggerMain()
		{
			constexpr std::array<const char*, static_cast<size_t>(LogLevel::Max) + 1> level_str_table{ "Info", "Warn", "Error", "Fatal", "Max"};
			std::stringstream sstream;

			while(true)
			{
				for (LogQueue* queue : _log_queue)
				{
					Log* log;
					while(queue->try_pop(log))
					{
						sstream << '[' << level_str_table[static_cast<uint8_t>(log->log_level)] << "][" << DateTime(log->time).ToLocalDateString() << "][" << log->location.file_name() <<" line:"<< log->location.line() << "][" << log->thread_id << "][ ";
						sstream << std::string_view(reinterpret_cast<char*>(log->message.ptr), log->message.length);
						sstream << " ]\n";
						
						LogBufferPool::PushDirect(log);
					}
				}

				const std::string& log = sstream.str();
				if (log.size() > 0)
				{
					std::cout << log;
					sstream.str("");
				}

				std::this_thread::sleep_for(Milliseconds(1));
			}
		}

	public:
		static void InitializeLogger(const uint32_t max_log_size, const uint32_t parallelism)
		{
			Logger* logger = GetInstance();

			logger->_max_log_size = max_log_size;
			logger->_parallelism = parallelism;

			for (uint32_t index = 0; index < parallelism; index++)
			{
				LogBufferPool* pool = new LogBufferPool(1024, 8, max_log_size, 1);
				LogQueue* queue = new LogQueue(1024 * parallelism);

				logger->_buffer_pool.push_back(pool);
				logger->_log_queue.push_back(queue);
			}

			logger->_logger_routine = new std::thread(&Logger::LoggerMain, logger);
		}

		static Logger* GetInstance()
		{
			static Logger logger;
			return &logger;
		}

		template <typename... Args>
		void PushLog(const LogLevel log_level, const std::source_location& location, const Milliseconds time, const char* const format, Args... args)
		{
			Log* log = _buffer_pool[std::hash<std::thread::id>()(std::this_thread::get_id()) % _parallelism]->Pop(true);

			log->time = time;
			log->location = location;
			log->thread_id = std::this_thread::get_id();
			log->log_level = log_level;

			log->message.length = sprintf_s(reinterpret_cast<char*>(log->message.ptr), log->message.capacity, format, args...);

			assert(log->message.length != _max_log_size);

			_log_queue[std::hash<std::thread::id>()(log->thread_id) % _parallelism]->push(log);
		}
	};
}

using LogLevel = utility::LogLevel;

#define LOG(log_level, ...) \
	utility::Logger::GetInstance()->PushLog(log_level, std::source_location::current(), utility::CurrentTimeEpoch<utility::Milliseconds>(), __VA_ARGS__);

#define LOG_AT(log_level, time, format, ...) \
	utility::Logger::GetInstance()->PushLog(log_level, std::source_location::current(), time, format, __VA_ARGS__)