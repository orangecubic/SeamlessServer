#pragma once

#include "Time.h"

namespace utility
{
	template<TimeUnit T>
	class Timer
	{
	private:
		const T _tick;
		mutable T _last_tick_time = T(0);

	public:
		Timer() : Timer(T(0)) {}
		Timer(const T tick) : _tick(tick), _last_tick_time(CurrentTick<T>()) {}

		operator bool() const
		{
			T current_tick = CurrentTick<T>();

			if (_last_tick_time + _tick < current_tick)
			{
				_last_tick_time = current_tick;
				return true;
			}

			return false;
		}
	};

	template<TimeUnit T>
	class Countdown
	{
	private:
		T _delay;
		T _start_time;

	public:
		Countdown() : Countdown(T(0)) {}
		Countdown(const T delay) : _delay(delay), _start_time(CurrentTick<T>()) {}
		Countdown(const Countdown<T>& countdown) : _delay(countdown._delay), _start_time(countdown._start_time) {}

		operator bool() const
		{
			T current_tick = CurrentTick<T>();

			if (_start_time + _delay < current_tick)
				return true;

			return false;
		}

		Countdown<T>& operator=(const Countdown<T>& countdown)
		{
			_delay = countdown._delay;
			_start_time = countdown._start_time;

			return *this;
		}
	};

	template<TimeUnit T>
	class Debouncer
	{
	private:
		const T _delay;
		mutable bool _is_on = false;
		T _last_enabled_time = T(0);
		
	public:
		Debouncer(const T delay) : _delay(delay) {}

		void Set()
		{
			_is_on = true;
			_last_enabled_time = CurrentTick<T>();
		}

		operator bool() const
		{
			if (_is_on && _last_enabled_time + _delay < CurrentTick<T>())
			{
				_is_on = false;
				return true;
			}

			return false;
		}
	};

	template<TimeUnit T>
	class Throttler
	{
	private:
		const T _tick;
		mutable T _last_execution_time;

	public:
		Throttler(const T tick) : _tick(tick), _last_execution_time(CurrentTick<T>()) {}

		operator bool() const
		{
			T current_tick = CurrentTick<T>();

			if (_last_execution_time + _tick <= current_tick)
			{
				_last_execution_time = current_tick;
				return true;
			}

			return false;
		}
	};
}