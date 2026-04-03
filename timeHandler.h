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
		// Description: Get the current time in utc (ceiled in seconds)
		// Method:    getUtcNow
		// FullName:  vsid::time::getUtcNow
		// Access:    public 
		// Returns:   std::chrono::time_point<std::chrono::utc_clock, std::chrono::seconds>
		// Qualifier:
		//************************************
		inline std::chrono::time_point<std::chrono::utc_clock, std::chrono::seconds> getUtcNow()
		{
			return std::chrono::ceil<std::chrono::seconds>(std::chrono::utc_clock::now());
		}

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
			std::chrono::sys_time sysTime = std::chrono::clock_cast<std::chrono::system_clock>(vsid::time::getUtcNow());
			return std::format("{:%Y-%m-%d}", sysTime);
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


