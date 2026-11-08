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

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vsid
{
	namespace tzif
	{
		//************************************
		// Description: Checks if a zone name is safe to be used as relative path below the zoneinfo folder.
		//				Accepts 1 to 3 segments separated by '/', each of [A-Za-z0-9_+-]. Case is not folded.
		// Parameter: std::string_view name
		//************************************
		bool isValidZoneName(std::string_view name) noexcept;

		//************************************
		// Description: A POSIX TZ rule (footer of a TZif file), e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
		//************************************
		class PosixTz
		{
		public:
			//************************************
			// Description: Parses a POSIX TZ string (incl. <+01> quoted names and negative / >24h rule times)
			// Parameter: std::string_view footer
			// Returns: std::nullopt if the string is invalid
			//************************************
			static std::optional<PosixTz> parse(std::string_view footer);

			//************************************
			// Description: Returns the offset to UTC (positive east of Greenwich) at the given instant
			// Parameter: std::chrono::sys_seconds t
			//************************************
			std::chrono::seconds utcOffset(std::chrono::sys_seconds t) const;

		private:
			enum class RuleType { Month, Julian1, Julian0 };

			struct Rule
			{
				RuleType type = RuleType::Month;
				int month = 0; // 1-12 (Month)
				int week = 0; // 1-5, 5 = last (Month)
				int weekday = 0; // 0 = Sunday (Month)
				int day = 0; // 1-365 (Julian1) or 0-365 (Julian0)
				long long time = 7200; // seconds from local midnight, may be negative or > 24h
			};

			static long long transition(const Rule& rule, int year, long long localOffset);

			long long stdOffset = 0; // seconds east of UTC
			long long dstOffset = 0; // seconds east of UTC
			bool hasDst = false;
			Rule start;
			Rule end;
		};

		//************************************
		// Description: A time zone loaded from a TZif file (RFC 8536, version 2+, 64 bit data and footer)
		//************************************
		class Zone
		{
		public:
			//************************************
			// Description: Parses the content of a TZif file. Every length is validated against the buffer.
			// Parameter: std::span<const unsigned char> bytes
			// Returns: std::nullopt if the data is truncated, inconsistent or not TZif v2+
			//************************************
			static std::optional<Zone> parse(std::span<const unsigned char> bytes);

			//************************************
			// Description: Returns the offset to UTC (positive east of Greenwich) at the given instant
			// Parameter: std::chrono::sys_seconds t
			//************************************
			std::chrono::seconds utcOffset(std::chrono::sys_seconds t) const;

		private:
			std::vector<long long> times; // transition instants, strictly ascending
			std::vector<unsigned char> typeIndex; // per transition, index into offsets
			std::vector<int> offsets; // utc offset in seconds per local time type
			std::optional<PosixTz> footer; // rule for instants after the last transition
		};

		enum class LoadStatus { Ok, InvalidName, FolderMissing, NotFound, Corrupt };

		struct LoadResult
		{
			LoadStatus status;
			std::optional<Zone> zone;
		};

		//************************************
		// Description: Loads the zone <folder>/<name>. The name is validated and the resolved path
		//				must stay inside the folder.
		// Parameter: const std::filesystem::path& folder
		// Parameter: std::string_view name
		//************************************
		LoadResult loadZone(const std::filesystem::path& folder, std::string_view name);

		//************************************
		// Description: Returns the first line of <folder>/+VERSION (trimmed)
		// Parameter: const std::filesystem::path& folder
		// Returns: std::nullopt if the file is missing, unreadable or empty
		//************************************
		std::optional<std::string> readVersion(const std::filesystem::path& folder);
	}
}
