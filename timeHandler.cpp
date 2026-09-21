#include "pch.h"
#include "timeHandler.h"

bool vsid::time::isActive(const std::string& timezone, const int start, const int end)
{
	try
	{
		const std::chrono::time_zone* tz = vsid::time::getCachedTimeZone(timezone);

		using LocalTime = std::chrono::local_time<std::common_type_t<std::chrono::system_clock::duration, std::chrono::seconds>>;

		const auto now = std::chrono::system_clock::now();

		// no tz database available -> use system clock (UTC) as local time
		LocalTime localNow = tz ? tz->to_local(now) : LocalTime{ now.time_since_epoch() };
		auto day = std::chrono::floor<std::chrono::days>(localNow);		

		auto ztStart = day + std::chrono::hours{ start };
		auto ztEnd = day + std::chrono::hours{ end };

		if (ztStart <= ztEnd)
		{
			return localNow >= ztStart && localNow < ztEnd;
		}

		return localNow >= ztStart || localNow < ztEnd;
	}
	catch (const std::runtime_error& e)
	{
		vsid::Logger::log(
			vsid::LogLevel::Error,
			std::format("Time calculation failed [{}] - {}", timezone, e.what())
		);
	}
	return false;
}

void vsid::time::logTzdbVersion()
{
	try
	{
		vsid::Logger::log(
			vsid::LogLevel::Debug,
			std::format("Timezone database version [{}]", std::chrono::get_tzdb().version)
		);
	}
	catch (const std::exception& e)
	{
		vsid::Logger::log(
			vsid::LogLevel::Debug,
			std::format("Timezone database not available - {}", e.what())
		);
	}
}
