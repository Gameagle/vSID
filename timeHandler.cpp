#include "pch.h"
#include "timeHandler.h"

bool vsid::time::isActive(const std::string& timezone, const int start, const int end)
{
	try
	{
		const std::chrono::time_zone* tz = vsid::time::getCachedTimeZone(timezone);

		auto localNow = tz->to_local(std::chrono::system_clock::now());
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
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Time calculation failed [{}] - {}", timezone, e.what()));
	}
	return false;
}
