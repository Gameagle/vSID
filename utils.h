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

#include "airport.h"

#include <vector>
#include <string>
#include <algorithm>
#include <ranges>

namespace vsid
{
	namespace utils
	{
		/**
		 * @brief trim spaces on the left side of a string
		 *
		 * @param string
		 * @return std::string
		 */
		std::string ltrim(const std::string& string);
		/**
		 * @brief trim spaces on the right side of a string
		 *
		 * @param string
		 * @return std::string
		 */
		std::string rtrim(const std::string& string);
		/**
		 * @brief trims spaces on both sides of a string
		 *
		 * @param string
		 * @return std::string
		 */
		std::string trim(const std::string& string);
		/**
		 * @brief Splits a string based on a given delimiter
		 *
		 * @param string
		 * @param del - delimiter
		 * @return std::vector<std::string>
		 */
		std::vector<std::string> split(const std::string& string, const char& del, const bool keepWhitespace = false);
		/**
		 * @brief Joins a vector of strings into one string
		 * 
		 * @param toJoin - Vector to join 
		 * @param del - delimiter
		 * @return std::string 
		 */
		std::string join(const std::vector<std::string>& toJoin, const char del = ' ');
		/**
		 * @brief Joins a set of strings into one string
		 *
		 * @param toJoin - Vector to join
		 * @param del - delimiter
		 * @return std::string
		 */
		std::string join(const std::set<std::string>& toJoin, const char del = ' ');
		/**
		 * @brief Splits a string based on spaces - splits again for '/' delimiter. Used to vectorize the filed route - WILL BE DELETED
		 *
		 * @param string
		 * @return std::vector<std::string>
		 */
		std::vector<std::string> splitRoute(std::string& string);
		/**
		 * @brief Fixed for airport icao checks - possible change to template
		 *
		 * @param airportVector - vector containing all active airports
		 * @param toSearch - string to search for, should be icao of an airport to check
		 * @return true - if the icao was found in active airports
		 * @return false - if the icao was NOT found in active airports
		 */
		bool isIcaoInVector(const std::vector<vsid::Airport>& airportVector, const std::string& toSearch);
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
		// Returns:   bool
		// Qualifier:
		// Parameter: const R & range - object container, e.g. std::vector
		// Parameter: const T & value - value to search for
		//************************************
		template<std::ranges::range R, typename T>
		bool contains(const R& range, const T& value)
		{
			return std::ranges::find(range, value) != std::ranges::end(range);
		}

		inline bool lastIsDigit(std::string_view s)
		{
			return std::isdigit(s.back());
		}
	}
}