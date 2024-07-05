#pragma once

#include "../utility/Time.h"
#include "LinearWorkQueue.h"
#include <thread>
#include <chrono>

using WorkerTimeUnit = utility::Milliseconds;

template <typename T>
class LinearWorker
{
private:
	std::thread _worker;
	LinearWorkQueue<T, 65536> _context_queue;

	void WorkerMain()
	{
		bool run = true;
		bool no_sleep = false;


		std::vector<T> contexts;
		T context;

		WorkerTimeUnit last_update_time(0), last_update_context_time(0);

		while (run)
		{
			_context_queue.LockSubmissionQueue();

			while (_context_queue.TryPop(context))
				contexts.push_back(context);

			_current_time = utility::CurrentTick<utility::Milliseconds>();

			if (!contexts.empty())
			{
				UpdateContext(contexts, _current_time, _current_time - last_update_context_time);
				last_update_context_time = utility::CurrentTick<utility::Milliseconds>();
				contexts.clear();

				no_sleep = true;
			}

			if ((_current_time - last_update_time) >= _update_tick)
			{
				run = Update(_current_time, _current_time - last_update_time);
				last_update_time = _current_time;

				no_sleep = true;
			}
			
			UpdateEveryTick(_current_time);

			if (no_sleep)
			{
				no_sleep = false;
				continue;
			}

			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

protected:
	WorkerTimeUnit _update_tick = WorkerTimeUnit(0);
	WorkerTimeUnit _current_time = WorkerTimeUnit(0);

	virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) = 0;
	virtual void UpdateContext(const std::vector<T>& contexts, const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) {}

	virtual void UpdateEveryTick(const WorkerTimeUnit current_time) {}

public:

	void Start(const WorkerTimeUnit update_tick)
	{
		if (_update_tick.count() != 0)
			return;

		_update_tick = update_tick;
		_worker = std::thread(&LinearWorker::WorkerMain, this);
	}

	bool Submit(const T& context)
	{
		return _context_queue.TryPush(context);
	}
};