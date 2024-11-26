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
#include <vector>

namespace vsid
{
	class Sid
	{
	public:

		Sid(std::string base = "", std::string waypoint = "", std::string id = "", std::string number = "", std::string designator = "",
			std::vector<std::string> rwys = {}, std::map<std::string, bool> equip = {}, int initialClimb = 0, bool climbvia = false, int prio = 99,
			bool pilotfiled = false, std::map<std::string, std::string> actArrRwy = {}, std::map<std::string, std::string> actDepRwy = {}, std::string wtc = "", std::string engineType = "",
			std::map<std::string, bool>acftType = {}, int engineCount = 0, int mtow = 0, std::map<std::string, bool> dest = {},
			std::string customRule = "", std::string area = "", int lvp = -1,
			int timeFrom = -1, int timeTo = -1) : base(base), waypoint(waypoint), id(id), number(number), designator(designator),
			rwys(rwys), equip(equip), initialClimb(initialClimb), climbvia(climbvia), prio(prio),
			pilotfiled(pilotfiled), actArrRwy(actArrRwy), actDepRwy(actDepRwy), wtc(wtc), engineType(engineType),
			acftType(acftType), engineCount(engineCount), mtow(mtow), dest(dest),
			customRule(customRule), area(area), lvp(lvp), timeFrom(timeFrom), timeTo(timeTo) {};

		std::string base;
		std::string waypoint;
		std::string id;
		std::string number;
		std::string designator;
		std::vector<std::string> rwys;
		/**
		first - std::string - equipment code
		second - bool - mandatory (true) or forbidden (false)
		 */
		std::map<std::string, bool> equip;
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
		std::map<std::string, bool> dest;
		std::string customRule;
		std::string area;
		int lvp;
		int timeFrom;
		int timeTo;
		/**
		 * @brief Gets the SID name (base + number + designator)
		 *
		 * @param sid - the sid object to check
		 */
		std::string name() const;
		/**
		 * @brief Return the full name of a sid (including id)
		 * 
		 */
		std::string idName() const;

		//************************************
		// Method:    getRwys
		// FullName:  vsid::Sid::getRwys
		// Access:    public 
		// Returns:   std::vector<std::string> - might be an empty vector for no rwys or single element for one rwy
		// Qualifier: const
		//************************************
		std::vector<std::string> getRwys() const;
		/**
		 * @brief Checks if a SID object is empty (base is checked)
		 *
		 */
		bool empty() const;
		/**
		 * @brief Compares if two SIDs are the same
		 *
		 * @param sid - sid to compare to
		 * @return true - if waypoint, number and designator match
		 */
		bool operator==(const Sid& sid);
		/**
		 * @brief Compares if two SIDs are the different
		 *
		 * @param sid - sid to compare to
		 * @return true - if at least one of waypoint, number or designator don't match
		 */
		bool operator!=(const Sid& sid);
	};
}
