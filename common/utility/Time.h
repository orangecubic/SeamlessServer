#pragma once

#include <chrono>
#include <unordered_map>
#include <sstream>

namespace utility
{
	template <class Dur>
	concept TimeUnit = std::same_as<
		Dur, std::chrono::duration<typename Dur::rep, typename Dur::period>
	>;

	using Nanoseconds = std::chrono::nanoseconds;
	using Microseconds = std::chrono::microseconds;
	using Milliseconds = std::chrono::milliseconds;
	using Seconds = std::chrono::seconds;

	using Timezone = std::chrono::time_zone;

	using Minutes = std::chrono::minutes;
	using Hours = std::chrono::hours;
	using Days = std::chrono::days;
	using Months = std::chrono::months;
	using Years = std::chrono::years;

	using TimePoint = Nanoseconds;

	inline const std::unordered_map<int64_t, const std::chrono::time_zone*>* _tz_list()
	{
		static thread_local std::unordered_map<int64_t, const std::chrono::time_zone*>* timezones = nullptr;
		if (timezones != nullptr)
			return timezones;

		timezones = new std::unordered_map<int64_t, const std::chrono::time_zone*>();

		for (const std::chrono::time_zone& tz : std::chrono::get_tzdb().zones)
		{
			std::chrono::sys_info info = tz.get_info(std::chrono::system_clock::now());
			timezones->insert(std::pair(info.offset.count(), &tz));
		}

		return timezones;
	}

	template <TimeUnit T>
	inline std::chrono::local_time<T> ToLocalTimePoint(const std::chrono::year_month_day& ymd, const std::chrono::hh_mm_ss<T>& hms)
	{
		return static_cast<std::chrono::local_days>(ymd) + static_cast<T>(hms);
	}

	template <TimeUnit T>
	inline const Timezone* GetAnyTimezoneByOffset(const T offset)
	{
		const std::unordered_map<int64_t, const std::chrono::time_zone*>* tz_list = _tz_list();

		auto iterator = tz_list->find(std::chrono::duration_cast<Seconds>(offset).count());
		if (iterator == tz_list->end())
			return nullptr;

		return iterator->second;
	}

	inline const Timezone* GetTimezoneByName(const std::string_view& name)
	{
		return std::chrono::locate_zone(name);
	}

	template <TimeUnit CastUnit, TimeUnit OriginUnit>
	inline CastUnit TimeCast(const OriginUnit& org)
	{
		return std::chrono::duration_cast<CastUnit>(org);
	}

	inline Nanoseconds CurrentTick()
	{
		return std::chrono::high_resolution_clock::now().time_since_epoch();
	}

	template <TimeUnit T>
	inline T CurrentTick()
	{
		return TimeCast<T>(std::chrono::high_resolution_clock::now().time_since_epoch());
	}

	inline Nanoseconds CurrentTimeEpoch()
	{
		return std::chrono::system_clock::now().time_since_epoch();
	}

	template <TimeUnit T>
	inline T CurrentTimeEpoch()
	{
		return TimeCast<T>(std::chrono::system_clock::now().time_since_epoch());
	}



	class DateTime
	{
	private:
		std::chrono::year_month_day _ymd;
		std::chrono::hh_mm_ss<Nanoseconds> _hms;
		std::chrono::zoned_time<Nanoseconds> _epoch_time;

		template <typename TimePoint>
		void SetTimeData(const TimePoint epoch_time_point, const Timezone* timezone)
		{
			_epoch_time = std::chrono::zoned_time<Nanoseconds>(timezone, epoch_time_point);

			std::chrono::local_time<Nanoseconds> local_time = _epoch_time.get_local_time();
			std::chrono::local_days local_days = std::chrono::floor<Days>(local_time);

			_ymd = std::chrono::year_month_day(local_days);
			_hms = std::chrono::hh_mm_ss<Nanoseconds>(local_time.time_since_epoch() - local_days.time_since_epoch());
		}

		template <typename TimePoint>
		DateTime(const TimePoint epoch_time_point, const Timezone* const timezone)
		{
			SetTimeData(epoch_time_point, timezone);
		}
	public:

		template <TimeUnit T>
		DateTime(const T epoch_time, const Timezone* const timezone)
		{
			SetTimeData(std::chrono::sys_time(epoch_time), timezone);
		}

		template <TimeUnit T>
		DateTime(const T epoch_time) : DateTime(epoch_time, std::chrono::current_zone()) {}

		DateTime(const uint32_t year, const uint32_t month, const uint32_t day, const uint32_t hour, const uint32_t minute, const uint32_t second, const Timezone* timezone)
			: _ymd(std::chrono::year(year), std::chrono::month(month), std::chrono::day(day)), _hms(Seconds(hour * 3600 + minute * 60 + second))
		{
			SetTimeData(ToLocalTimePoint(_ymd, _hms), timezone);
		}

		DateTime(const Timezone* const timezone) : DateTime(std::chrono::duration_cast<Nanoseconds>(std::chrono::system_clock::now().time_since_epoch()), timezone) {}

		DateTime() : DateTime(std::chrono::duration_cast<Nanoseconds>(std::chrono::system_clock::now().time_since_epoch())) {}

		template <TimeUnit T = Nanoseconds>
		T ToEpochTimestamp()
		{
			return std::chrono::duration_cast<T>(_epoch_time.get_sys_time().time_since_epoch());
		}

		template <TimeUnit T>
		DateTime& Add(const T time)
		{
			auto expected_time = _epoch_time.get_sys_time().time_since_epoch() + time;
			SetTimeData(std::chrono::sys_time(expected_time), _epoch_time.get_time_zone());

			return *this;
		}

		template <>
		DateTime& Add(const Months time)
		{
			_ymd += time;
			return Floor<Months>();
		}


		template <>
		DateTime& Add(const Years time)
		{
			_ymd += time;
			return Floor<Years>();
		}

		template <TimeUnit T>
		DateTime Plus(const T time)
		{
			auto expected_time = _epoch_time.get_sys_time().time_since_epoch() + time;
			return DateTime(expected_time, _epoch_time.get_time_zone());
		}

		template <>
		DateTime Plus(const Months time)
		{
			std::chrono::year_month_day ymd(_ymd);
			ymd += time;

			return DateTime(ToLocalTimePoint(ymd, _hms), _epoch_time.get_time_zone()).Floor<Months>();
		}

		template <>
		DateTime Plus(const Years time)
		{
			std::chrono::year_month_day ymd(_ymd);
			ymd += time;

			return DateTime(ToLocalTimePoint(ymd, _hms), _epoch_time.get_time_zone()).Floor<Years>();
		}

		template <TimeUnit T>
		DateTime& Floor()
		{
			std::chrono::local_time<Nanoseconds> expected_time = std::chrono::floor<T>(_epoch_time.get_local_time());
			SetTimeData(expected_time, _epoch_time.get_time_zone());

			return *this;
		}

		template <>
		DateTime& Floor<Months>()
		{
			SetTimeData(
				ToLocalTimePoint(std::chrono::year_month_day(_ymd.year(), _ymd.month(), std::chrono::day(1)), std::chrono::hh_mm_ss(Nanoseconds(0))),
				_epoch_time.get_time_zone()
			);

			return *this;
		}

		template <>
		DateTime& Floor<Years>()
		{
			SetTimeData(
				ToLocalTimePoint(std::chrono::year_month_day(_ymd.year(), std::chrono::January, std::chrono::day(1)), std::chrono::hh_mm_ss(Nanoseconds(0))),
				_epoch_time.get_time_zone()
			);

			return *this;
		}

		template <TimeUnit T>
		DateTime& operator+=(const T time)
		{
			return Add(time);
		}

		template <TimeUnit T>
		DateTime& operator-=(const T time)
		{
			return Add(-time);
		}

		template <TimeUnit T>
		DateTime operator+(const T time)
		{
			return Plus(time);
		}

		template <TimeUnit T>
		DateTime operator-(const T time)
		{
			return Plus(-time);
		}

		int32_t Year()
		{
			return static_cast<int>(_ymd.year());
		}

		bool IsLeapYear()
		{
			return _ymd.year().is_leap();
		}

		bool IsLastDayOfMonth()
		{
			std::chrono::year_month_day_last ymdl(_ymd.year(), std::chrono::month_day_last(_ymd.month()));

			return ymdl.day() == _ymd.day();
		}

		uint32_t Month()
		{
			return static_cast<unsigned int>(_ymd.month());
		}

		uint32_t Day()
		{
			return static_cast<unsigned int>(_ymd.day());
		}

		int32_t Hour()
		{
			return _hms.hours().count();
		}

		int32_t Minute()
		{
			return _hms.minutes().count();
		}

		int64_t Second()
		{
			return _hms.seconds().count();
		}

		int64_t SubSecond()
		{
			return _hms.subseconds().count();
		}

		Hours GetTimezoneOffset()
		{
			return std::chrono::duration_cast<Hours>(_epoch_time.get_time_zone()->get_info(_epoch_time.get_local_time()).first.offset);
		}

		std::string ToLocalDateString()
		{
			std::stringstream sstream;
			sstream << Year() << '-' << std::setfill('0') << std::setw(2) << Month() << '-' << std::setfill('0') << std::setw(2) << Day() << ' ' <<
				std::setfill('0') << std::setw(2) << Hour() << ':' << std::setfill('0') << std::setw(2) << Minute() << ':' << std::setfill('0') << std::setw(2) << Second() << '.' << std::setfill('0') << std::setw(3) << SubSecond() / 1000000;

			return sstream.str();
		}
	};
}