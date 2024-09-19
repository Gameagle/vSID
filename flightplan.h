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
#include "pch.h"
#include "es/EuroScopePlugIn.h"
#include "sid.h"

#include <string>
#include <vector>
#include <set>
#include <chrono>

namespace vsid
{
	namespace fpln
	{
		struct Info
		{
			bool atcRWY = false;
			bool noFplnUpdate = false;
			bool remarkChecked = false;
			vsid::Sid sid = {};
			vsid::Sid customSid = {};
			std::string sidWpt = "";
			std::chrono::time_point<std::chrono::utc_clock, std::chrono::seconds> lastUpdate;
			int updateCounter = 0;
			bool request = false;
			bool validEquip = true;
			std::string gndState = "";
		};

		/**
		 * @brief Strip the filed route from SID/RWY and/or SID to have a bare route to populate with set SID.
		 * Any SIDs or RWYs will be deleted and have to be reset.
		 * 
		 * @param CFlightPlan - FlightPlan as stored by Euroscope
		 * @param filedSidWpt - waypoint to check again (base of a SID or the waypoint a SID with different name leads to)
		 * @return vectored route
		 */
		std::vector<std::string> clean(const EuroScopePlugIn::CFlightPlan &FlightPlan, std::string filedSidWpt = "");

		/**
		 * @brief Get only the assigned rwy extracted from the flightplan
		 * 
		 * @param CFlightPlan - FlightPlan as stored by Euroscope
		 * @return pair if ICAO/RWY or SID/RWY was found (split by '/')
		 */
		std::pair<std::string, std::string> getAtcBlock(const EuroScopePlugIn::CFlightPlan &FlightPlan);

		/** 
		 * @brief Searches the flightplan remarks for the given string
		 * 
		 * @param fplnData - flightplan data to get the remarks from
		 * @param searchStr - which string to search for
		 * @return true - if the string was found
		 * @return false - if the string was not found
		 */
		bool findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& searchStr));

		/**
		 * @brief Removes the given string from the flightplan remarks if present
		 * 
		 * @param fplnData - flightplan data to remove the remarks from
		 * @param searchStr - which string to remove
		 */
		bool removeRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toRemove));

		/**
		 * @brief Adds the given string to the flightplan remarks
		 * 
		 * @param fplnData - flightplan data to edit the remarks for
		 * @param searchStr - which string to add
		 */
		bool addRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& toAdd));

		bool findScratchPad(const EuroScopePlugIn::CFlightPlan &FlightPlan, const std::string& toSearch);

		bool setScratchPad(EuroScopePlugIn::CFlightPlan &FlightPlan, const std::string& toAdd);

		bool removeScratchPad(EuroScopePlugIn::CFlightPlan &FlightPlan, const std::string& toRemove);

		std::string getEquip(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::set<std::string> &rnav = {});

		std::string getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan);
	}
}