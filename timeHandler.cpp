#include "pch.h"
#include "timeHandler.h"
#include "messageHandler.h"

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
		messageHandler->writeMessage("ERROR", "Timezone failed - " + std::string(e.what()) + ": " + timezone);
	}
	return false;
}

std::chrono::time_point<std::chrono::utc_clock, std::chrono::seconds> vsid::time::getUtcNow()
{
	return std::chrono::ceil<std::chrono::seconds>(std::chrono::utc_clock::now());
}