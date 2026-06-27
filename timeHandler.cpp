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

std::string vsid::time::getFormattedTime(std::chrono::system_clock::time_point tp, std::string_view fmtStr)
{
	std::time_t time = std::chrono::system_clock::to_time_t(tp);
	std::tm tm{};
	gmtime_s(&tm, &time);

	std::array<char, 64> timeBuffer;

	if (std::strftime(timeBuffer.data(), timeBuffer.size(), fmtStr.data(), &tm) > 0) return std::string(timeBuffer.data());

	return "TIME_ERROR";
}