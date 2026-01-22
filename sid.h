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
#include <optional>

namespace vsid
{
	struct Transition
	{
		std::string base = "";
		std::string number = "";
		std::string designator = "";
	};

	class Sid
	{
	public:

		Sid(std::string base = "", std::string waypoint = "", std::string id = "", std::string number = "", std::string designator = "", std::vector<std::string> rwys = {},
			std::map<std::string, vsid::Transition> transition = {}, bool allowDiffNumbers = false, std::map<std::string, bool> equip = {}, int initialClimb = 0,
			bool climbvia = false, int prio = 99, bool pilotfiled = false, std::map<std::string, std::map<std::string, std::string>> actArrRwy = {},
			std::map<std::string, std::map<std::string, std::string>> actDepRwy = {}, std::string wtc = "", std::string engineType = "", std::string wingType = "",
			std::map<std::string, bool>acftType = {}, int engineCount = 0, int mtow = 0, std::map<std::string, bool> dest = {},
			std::map<std::string, std::map<std::string, std::vector<std::string>>> route = {}, std::string customRule = "", std::string area = "", int lvp = -1,
			int timeFrom = -1, int timeTo = -1, bool sidHighlight = false, bool clmbHighlight = false) : base(base), waypoint(waypoint), id(id), number(number), designator(designator),
			rwys(rwys), transition(transition), allowDiffNumbers(allowDiffNumbers), equip(equip), initialClimb(initialClimb), climbvia(climbvia), prio(prio),
			pilotfiled(pilotfiled), actArrRwy(actArrRwy), actDepRwy(actDepRwy), wtc(wtc), engineType(engineType),
			wingType(wingType), acftType(acftType), engineCount(engineCount), mtow(mtow), dest(dest), route(route),
			customRule(customRule), area(area), lvp(lvp), timeFrom(timeFrom), timeTo(timeTo), sidHighlight(sidHighlight), clmbHighlight(clmbHighlight) {};

		std::string base;
		std::string waypoint;
		std::string id;
		std::string number;
		std::string designator;
		std::vector<std::string> rwys;
		std::map<std::string, vsid::Transition> transition;
		/**
		first - std::string - equipment code
		second - bool - mandatory (true) or forbidden (false)
		 */
		bool allowDiffNumbers;
		std::map<std::string, bool> equip;
		int initialClimb;
		bool climbvia;
		int prio;
		bool pilotfiled;
		//************************************
		// Parameter: 1. map <std::string, - type of "allow" or "deny"
		// Parameter: 2. map <std::string,  - type of "allow" or "deny"
		// Parameter: , std::string>  - comma separated list of runways
		//************************************
		std::map<std::string, std::map<std::string, std::string>> actArrRwy;
		//************************************
		// Parameter: 1. map <std::string, - type of "allow" or "deny"
		// Parameter: 2. map <std::string,  - type of "allow" or "deny"
		// Parameter: , std::string>  - comma separated list of runways
		//************************************
		std::map<std::string, std::map<std::string, std::string>> actDepRwy;
		std::string wtc;
		std::string engineType;
		std::string wingType;
		std::map<std::string, bool> acftType;
		int engineCount;
		int mtow;
		std::map<std::string, bool> dest;
		//************************************
		// First Map
		// Parameter: <std::string, - type of "allow" or "deny"
		// Second Map
		// Parameter: <std::string, - id
		// Parameter: , std::vector<std::string>> - vector of route segments ("..." allowed as 'skip' segment)
		//************************************
		std::map<std::string, std::map<std::string, std::vector<std::string>>> route;
		std::string customRule;
		std::string area;
		int lvp;
		int timeFrom;
		int timeTo;
		bool sidHighlight;
		bool clmbHighlight;
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
		
		//************************************
		// Description: Collapses sid / trans bases with consecutive letters for a match
		// Method:    collapsedBaseMatch
		// FullName:  vsid::Sid::collapsedBaseMatch
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view other - the other string to compare to (usually from .ese file)
		// Parameter: std::optional<std::string_view> trans - as multiple transitions can be held it has to be specified
		// for the check, if nullopt the sid base is checked
		//************************************
		bool collapsedBaseMatch(std::string_view other, std::optional<std::string_view> trans = std::nullopt);
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
