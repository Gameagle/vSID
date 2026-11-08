/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim of vSID is to ease the work of any controller that edits and assigns
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

#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>

#include "constants.h"
#include "utils.h"
#include "logger.h"
#include "messageHandler.h"

namespace vsid
{
	namespace time
	{
		// Wine only: keeping legacy gmtime_s/strftime formatting until a Wine tester confirms that
		// std::format of a sys_time works (comparing "Time format check" debug line at start-up).
		// after confirmation we can change this to false / remove the checks
		inline constexpr bool useLegacyFormatOnWine = true;

		//************************************
		// Description: Returns the local time of the given IANA zone at the given instant.
		//				The only place that selects the time zone provider (std::chrono on Windows,
		//				shipped TZif data on Wine) and runs the fallback chain:
		//				provider -> error (reported once per cause) -> UTC (offset 0, no tz function involved).
		// Method:    localTime
		// FullName:  vsid::time::localTime
		// Access:    public
		// Returns:   std::chrono::local_seconds
		// Qualifier: noexcept
		// Parameter: const std::string & zoneName
		// Parameter: std::chrono::sys_seconds t
		//************************************
		std::chrono::local_seconds localTime(const std::string& zoneName, std::chrono::sys_seconds t) noexcept;

		//************************************
		// Description: Sets the time provider override from the main config ("auto", "wine", "windows").
		//				Unknown values log a warning (once) and mean "auto". A change drops all caches.
		// Method:    setTimeProvider
		// FullName:  vsid::time::setTimeProvider
		// Access:    public
		// Returns:   void
		// Qualifier:
		// Parameter: std::string_view value
		//************************************
		void setTimeProvider(std::string_view value);

		//************************************
		// Description: Sets the zoneinfo folder (relative to the DLL directory) used on Wine.
		//				Always drops all caches (every thread) and resets the time zone error flags,
		//				so a config reload re-checks the folder.
		// Method:    setZoneinfoPath
		// FullName:  vsid::time::setZoneinfoPath
		// Access:    public
		// Returns:   void
		// Qualifier:
		// Parameter: std::string relativePath
		//************************************
		void setZoneinfoPath(std::string relativePath);

		//************************************
		// Description: Checks if a given SID time restriction is between start and end time
		// Method:    isActive
		// FullName:  vsid::time::isActive
		// Access:    public
		// Returns:   bool
		// Qualifier:
		// Parameter: const std::string & timezone
		// Parameter: const int start
		// Parameter: const int end
		//************************************
		bool isActive(const std::string& timezone, const int start, const int end);


		//************************************
		// Description: Logs the time provider in use and the current tzdb version (Windows) or the version
		//				of the shipped zoneinfo data (Wine), or an error if it is not available
		// Method:    logTzdbVersion
		// FullName:  vsid::time::logTzdbVersion
		// Access:    public
		// Returns:   void
		// Qualifier:
		//************************************
		void logTzdbVersion();

		namespace detail
		{
			//************************************
			// Description: true if the Wine time path (TZif provider, legacy formatting) is to be used:
			//				the timeProvider override if set, otherwise vsid::utils::usingWine().
			//				Every Wine-vs-Windows decision for time goes through this function.
			//************************************
			bool useWinePath() noexcept;

			//************************************
			// Description: Legacy formatting (gmtime_s / strftime, UTC)
			//************************************
			inline std::string legacyFormat(std::chrono::sys_seconds tp, std::string_view fmtStr)
			{
				const std::string fmt(fmtStr); // strftime needs a null terminated string
				const std::time_t time = std::chrono::system_clock::to_time_t(tp);
				std::tm tm{};
				if (gmtime_s(&tm, &time) != 0) return "TIME_ERROR";

				std::array<char, 64> timeBuffer{};

				if (std::strftime(timeBuffer.data(), timeBuffer.size(), fmt.c_str(), &tm) > 0) return std::string(timeBuffer.data());

				return "TIME_ERROR";
			}

			//************************************
			// Description: std::format formatting of a sys_time with a chrono pattern (e.g. "%H:%M:%S")
			//************************************
			template<typename D>
			std::string modernFormat(std::chrono::sys_time<D> tp, std::string_view fmtStr)
			{
				return std::vformat("{:" + std::string(fmtStr) + "}", std::make_format_args(tp));
			}
		}

		//************************************
		// Description: The single time formatter. Formats in UTC.
		//				Uses std::format at the precision of the given time point (%S shows fractional seconds
		//				for a sub-second time point); on Wine (see useLegacyFormatOnWine) the legacy strftime path,
		//				which is always truncated to full seconds. Only system_clock time points are accepted
		//				(clock_cast from other clocks, e.g. utc_clock, reads tzdb leap second data).
		// Method:    getFormattedTime
		// FullName:  vsid::time::getFormattedTime
		// Access:    public
		// Returns:   std::string - "TIME_ERROR" if formatting fails
		// Qualifier:
		// Parameter: const std::chrono::time_point<T
		// Parameter: U> & tp
		// Parameter: std::string_view fmtStr
		//************************************
		template<typename T, typename U>
		std::string getFormattedTime(const std::chrono::time_point<T, U>& tp, std::string_view fmtStr = "%Y-%m-%d %H:%M:%S")
		{
			static_assert(std::is_same_v<T, std::chrono::system_clock>, "only system_clock time points are supported");

			try
			{
				if (useLegacyFormatOnWine && detail::useWinePath())
					return detail::legacyFormat(std::chrono::floor<std::chrono::seconds>(tp), fmtStr);

				return detail::modernFormat(tp, fmtStr);
			}
			catch (...)
			{
				return "TIME_ERROR";
			}
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
			return getFormattedTime(std::chrono::system_clock::now(), "%Y-%m-%d");
		}

		/**
		 * @brief Transform a timepoint into a string
		 *
		 * @tparam T - the clock (system_clock only)
		 * @tparam U - time resolutiong e.g. ::seconds
		 * @param timePoint
		 *
		 */
		template<typename T, typename U>
		std::string toFullString(const std::chrono::time_point<T, U>& timePoint)
		{
			static_assert(std::is_same_v<T, std::chrono::system_clock>, "only system_clock time points are supported");
			return getFormattedTime(timePoint, "%Y.%m.%d %H:%M:%S");
		}

		//************************************
		// Description: Transform a timepoint into a time string
		// Method:    toTimeString
		// FullName:  vsid::time::toTimeString
		// Access:    public
		// Returns:   std::string
		// Qualifier:
		// Parameter: const std::chrono::time_point<T
		// Parameter: U> & timePoint
		//************************************
		template<typename T, typename U>
		std::string toTimeString(const std::chrono::time_point<T, U>& timePoint)
		{
			static_assert(std::is_same_v<T, std::chrono::system_clock>, "only system_clock time points are supported");
			return getFormattedTime(timePoint, "%H:%M:%S");
		}
	}
}

