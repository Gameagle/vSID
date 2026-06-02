#include "pch.h"
#include "timeHandler.h"
#include "messageHandler.h"

#include "logger.h"
#include <format>

bool vsid::time::isActive(const std::string& timezone, const int start, const int end)
{
	try
	{
		std::chrono::zoned_time zt{ timezone, std::chrono::system_clock::now() };
		std::chrono::zoned_time ztStart = zt;
		std::chrono::zoned_time ztEnd = zt;

		auto day = std::chrono::floor<std::chrono::days>(zt.get_local_time());
		ztStart = day + std::chrono::hours{ start };
		ztEnd = day + std::chrono::hours{ end };

		if ((ztEnd.get_local_time() < ztStart.get_local_time() &&
			(zt.get_local_time() > ztStart.get_local_time() || zt.get_local_time() < ztEnd.get_local_time())) ||
			(ztEnd.get_local_time() > ztStart.get_local_time() &&
			zt.get_local_time() > ztStart.get_local_time() &&
			zt.get_local_time() < ztEnd.get_local_time())
			)
		{
			return true;
		}
	}
	catch (std::runtime_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Timezone failed - {}: {}", e.what(), timezone));
	}
	return false;
}