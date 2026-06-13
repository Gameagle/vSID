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

#include "include/es/EuroScopePlugIn.h"

#include <vector>
#include <string>
#include <algorithm>
#include <ranges>
#include <cctype>

#include <set> // #dev - added due to disabling airport.h

namespace vsid
{
	namespace utils
	{
		struct CICompare {
			using is_transparent = void; // Enable heterogeneous lookup

			bool operator()(std::string_view a, std::string_view b) const noexcept
			{
				return std::ranges::lexicographical_compare(a, b, [](unsigned char ac, unsigned char bc)
					{
						return std::tolower(ac) < std::tolower(bc);
					});
			}
		};

		//************************************
		// Description: Trims spaces on the left side of a string_view
		// Method:    ltrim
		// FullName:  vsid::utils::ltrim
		// Access:    public 
		// Returns:   constexpr std::string_view
		// Qualifier:
		// Parameter: std::string_view sv
		//************************************
		constexpr std::string_view ltrim(std::string_view sv)
		{
			size_t start = sv.find_first_not_of(' ');
			return (start == std::string_view::npos) ? "" : sv.substr(start);
		}
		
		//************************************
		// Description: Trims spaces on the right side of a string_view
		// Method:    rtrim
		// FullName:  vsid::utils::rtrim
		// Access:    public 
		// Returns:   constexpr std::string_view
		// Qualifier:
		// Parameter: std::string_view sv
		//************************************
		constexpr std::string_view rtrim(std::string_view sv)
		{
			size_t end = sv.find_last_not_of(' ');
			return (end == std::string_view::npos) ? "" : sv.substr(0, end + 1);
		}
		
		//************************************
		// Description: Trims spaces on both sides of a string_view and returns string
		// Method:    trim
		// FullName:  vsid::utils::trim
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: const std::string_view sv
		//************************************
		inline std::string trim(const std::string_view sv)
		{
			// return std::string(ltrim(rtrim(sv)));
			return std::string(ltrim(rtrim(sv)));
		}
		
		//************************************
		// Description: Splits a string_view into a vector of strings based on a given delimiter, optionally keeping empty elements
		// Method:    split
		// FullName:  vsid::utils::split
		// Access:    public 
		// Returns:   std::vector<std::string> #refactor - possible improvement to string_view later on
		// Qualifier:
		// Parameter: std::string_view sv
		// Parameter: const char del
		// Parameter: const bool keepEmpty
		//************************************
		std::vector<std::string> split(std::string_view sv, const char del, const bool keepEmpty = false);

		//************************************
		// Description: Joins a container of strings / string_views into one string with a given delimiter
		// Method:    join
		// FullName:  vsid::utils::join
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: const C & toJoin
		// Parameter: const char del
		//************************************
		template<typename C>
		inline std::string join(const C& toJoin, const char del = ' ')
		{
			if (toJoin.empty()) return "";

			std::string result;
			for (const auto& elem : toJoin)
			{
				result += elem;
				result += del;
			}

			result.pop_back(); // remove the last delimiter
			return result;
		}

		/**
		 * @brief Checks if a number is contained in another number, e.g. if 2 is in 123
		 * 
		 * @param number number to be checked for
		 * @param digit number to be checked in
		 * @return true 
		 * @return false 
		 */
		bool containsDigit(int number, int digit);

		/**
		 * @brief Get a minimum climb for an airport (rounded to the next thousand ft above field + 500 ft)
		 * 
		 * @param elevation 
		 * @return int 
		 */
		int getMinClimb(int elevation);

		/**
		 * @brief Transforms the input string to lowercase
		 * 
		 * @param input - the string to transform
		 * @return lowercase input
		 */
		std::string tolower(std::string input);

		/**
		 * @brief Transforms the input to uppercase
		 *  
		 * @param input - the input to transform
		 * @return uppercase input
		 */
		std::string toupper(std::string input);

		//************************************
		// Method:    contains
		// FullName:  vsid::utils::contains
		// Access:    public 
		// Return:   bool
		// Qualifier:
		// Parameter: const R & range - object container, e.g. std::vector
		// Parameter: const T & value - value to search for
		//************************************
		template<std::ranges::range R, typename T>
		bool contains(const R& range, const T& value)
		{
			return std::ranges::find(range, value) != std::ranges::end(range);
		}


		//************************************
		// Description: Checks if the last character of a string is a digit
		// Method:    lastIsDigit
		// FullName:  vsid::utils::lastIsDigit
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view s
		//************************************
		inline bool lastIsDigit(std::string_view s)
		{
			return std::isdigit(s.back());
		}

		//************************************
		// Description: Checks if a string contains a digit
		// Method:    containsDigit
		// FullName:  vsid::utils::containsDigit
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view s
		//************************************
		inline bool containsDigit(std::string_view s)
		{
			for (unsigned char c : s)
				if (std::isdigit(c)) return true;

			return false;
		}

		//************************************
		// Description: Counts digits present in a string
		// Method:    countDigits
		// FullName:  vsid::utils::countDigits
		// Access:    public 
		// Returns:   int
		// Qualifier:
		// Parameter: const std::string_view s
		//************************************
		inline int countDigits(const std::string_view s)
		{
			int counter = 0;

			for (unsigned char c : s)
				if (std::isdigit(c)) counter++;

			return counter;
		}

		//************************************
		// Description: Creates a decimal position pair
		// Method:    toPoint
		// FullName:  vsid::utils::toPoint
		// Access:    public 
		// Returns:   EuroScopePlugIn::CPosition
		// Qualifier:
		// Parameter: const std::pair<std::string
		// Parameter: std::string> & pos
		//************************************
		EuroScopePlugIn::CPosition toPoint(const std::pair<std::string, std::string>& pos);

		//************************************
		// Description: Transforms lat/long string value into decimal equivalent
		// Method:    toDeg
		// FullName:  vsid::utils::toDeg
		// Access:    public 
		// Returns:   double
		// Qualifier:
		// Parameter: const std::string & coord
		//************************************
		double toDeg(const std::string& coord);

		//************************************
		// Description: Checks if a string starts with another string, ignoring case
		// Method:    startsWithCi
		// FullName:  vsid::utils::startsWithCi
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view txt - string to check
		// Parameter: std::string_view start - string to check if txt starts with
		//************************************
		inline bool startsWithCi(std::string_view txt, std::string_view start)
		{
			if (txt.length() < start.length()) return false;

			auto txtStart = txt.substr(0, start.length());
			return std::ranges::equal(txtStart, start, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
		}

		//************************************
		// Description: Checks if two strings are equal, ignoring case
		// Method:    svEqualCi
		// FullName:  vsid::utils::svEqualCi
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view a
		// Parameter: std::string_view b
		//************************************
		inline bool svEqualCi(std::string_view a, std::string_view b)
		{
			if (a.length() != b.length()) return false;
			return std::ranges::equal(a, b, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });	
		}
	}
}