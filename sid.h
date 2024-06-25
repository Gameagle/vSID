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
#include <map>

namespace vsid
{
	class Sid
	{
	public:

		Sid(std::string waypoint = "", std::string id = "", char number = ' ', char designator = ' ',
			std::string rwy = "", int initialClimb = 0, bool climbvia = false, int prio = 99,
			bool pilotfiled = false, std::map<std::string, std::string> actArrRwy = {}, std::map<std::string, std::string> actDepRwy = {}, std::string wtc = "", std::string engineType = "",
			std::map<std::string, bool>acftType = {}, int engineCount = 0, int mtow = 0,
			std::string customRule = "", std::string area = "", std::string equip = "", int lvp = -1,
			int timeFrom = -1, int timeTo = -1) : waypoint(waypoint), id(id), number(number), designator(designator),
			rwy(rwy), initialClimb(initialClimb), climbvia(climbvia), prio(prio),
			pilotfiled(pilotfiled), actArrRwy(actArrRwy), actDepRwy(actDepRwy), wtc(wtc), engineType(engineType),
			acftType(acftType), engineCount(engineCount), mtow(mtow),
			customRule(customRule), area(area), equip(equip), lvp(lvp), timeFrom(timeFrom), timeTo(timeTo) {};

		std::string waypoint;
		std::string id;
		char number;
		char designator;
		std::string rwy;
		int initialClimb;
		bool climbvia;
		int prio;
		bool pilotfiled;
		std::map<std::string, std::string> actArrRwy;
		std::map<std::string, std::string> actDepRwy;
		std::string wtc;
		std::string engineType;
		std::map<std::string, bool> acftType;
		int engineCount;
		int mtow;
		std::string customRule;
		std::string area;
		std::string equip;
		int lvp;
		int timeFrom;
		int timeTo;
		/**
		 * @brief Gets the SID name (wpt + number + designator)
		 *
		 * @param sid - the sid object to check
		 */
		std::string name() const;
		/**
		 * @brief Return the full name of a sid (including id)
		 * 
		 */
		std::string idName() const;
		/**
		 * @brief Gets the runway associated with a sid
		 *
		 * @return runway if single or first runway if multiple
		 */
		std::string getRwy() const;
		/**
		 * @brief Checks if a SID object is empty (waypoint is checked)
		 *
		 * @param sid - the sid object
		 */
		bool empty() const;
		/**
		 * @brief Compares if two SIDs are the same
		 *
		 * @param sid1 - first sid to compare
		 * @param sid2 - second sid to compare
		 * @return true - if waypoint, number and designator match
		 */
		bool operator==(const Sid& sid);
		/**
		 * @brief Compares if two SIDs are the different
		 *
		 * @param sid1 - first sid to compare
		 * @param sid2 - second sid to compare
		 * @return true - if at least one of waypoint, number or designator don't match
		 */
		bool operator!=(const Sid& sid);
	};
}
