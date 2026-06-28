/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flightplans.

Copyright (C) 2024 Gameagle (Philip Maier)
Repo @ https://github.com/Gameagle/vSID

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>
#include <chrono>
#include <format>
#include <unordered_map>

#include "constants.h"
#include "utils.h"
#include "logger.h"
#include "messageHandler.h"

namespace vsid
{
	namespace time
	{
		/**
		 * @brief Checks if a given SID time restriction is between start and end time
		 * 
		 * @param timezone - which timezone should be checked
		 * @param start - start time
		 * @param end - end time
		 * @return if the restricted SID is active
		 */
		bool isActive(const std::string& timezone, const int start, const int end);

		//************************************
		// Description: Get the current date as string in format YYYY-MM-DD
		// Method:    getDate
		// FullName:  vsid::time::getDate
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		//************************************
		inline std::string getDate()
		{
			return std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
		}

		//************************************
		// Description: Special workaround function to use legacy code for Wine or where modern C++ isn't available
		// Method:    getFormattedTime
		// FullName:  vsid::time::getFormattedTime
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: std::chrono::system_clock::time_point tp
		// Parameter: std::string_view fmtStr
		//************************************
		std::string getFormattedTime(std::chrono::system_clock::time_point tp = std::chrono::system_clock::now(), std::string_view fmtStr = "%Y-%m-%d %H:%M:%S");

		//************************************
		// Description: Caches timezone and provides fallback if timezone unavailable
		// Method:    getCachedTimeZone
		// FullName:  vsid::time::getCachedTimeZone
		// Access:    public 
		// Returns:   const std::chrono::time_zone* - using "UTC" if using Wine or on timezone error
		// Qualifier:
		// Parameter: const std::string & tzName
		//************************************
		inline const std::chrono::time_zone* getCachedTimeZone(const std::string& tzName)
		{
			thread_local std::unordered_map<std::string, const std::chrono::time_zone*> tzCache;

			if (auto it = tzCache.find(tzName); it != tzCache.end()) return it->second;

			const std::chrono::time_zone* tz = nullptr;

			try
			{
				tz = std::chrono::locate_zone(vsid::utils::usingWine() ? "UTC" : tzName);

				messageHandler->removeGenError(ERROR_TIME_ZONE);
			}
			catch (const std::runtime_error&e )
			{
				tz = std::chrono::locate_zone("UTC");

				if (!messageHandler->genErrorsContains(ERROR_TIME_ZONE))
				{
					vsid::Logger::log(vsid::LogLevel::Warning, std::format("Invalid timezone [{}]. Fallback to UTC - [{}]", tzName, e.what()));

					messageHandler->addGenError(ERROR_TIME_ZONE);
				}
			}
			catch (const std::exception& e)
			{
				tz = std::chrono::locate_zone("UTC");

				if (!messageHandler->genErrorsContains(ERROR_TIME_ZONE))
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("Unexpected exception on timezone [{}]. Fallback to UTC - [{}]", tzName, e.what()));

					messageHandler->addGenError(ERROR_TIME_ZONE);
				}
			}

			tzCache[tzName] = tz;

			return tz;
		}

		/**
		 * @brief Transform a timepoint into a string
		 * 
		 * @tparam T - the clock e.g. utc_clock
		 * @tparam U - time resolutiong e.g. ::seconds
		 * @param timePoint 
		 * 
		 */
		template<typename T, typename U>
		std::string toFullString(const std::chrono::time_point<T, U>& timePoint)
		{
			return std::string(std::format("{:%Y.%m.%d %H:%M:%S}", timePoint));
		}

		template<typename T, typename U>
		std::string toTimeString(const std::chrono::time_point<T, U>& timePoint)
		{
			return std::string(std::format("{:%H:%M:%S}", timePoint));
		}
	}
}


