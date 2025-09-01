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
#include "include/es/EuroScopePlugIn.h"
#include "sid.h"

#include <string>
#include <vector>
#include <set>
#include <chrono>

namespace vsid
{
	struct Fpln
	{
		bool atcRWY = false;
		bool noFplnUpdate = false;
		bool remarkChecked = false;
		vsid::Sid sid = {};
		vsid::Sid customSid = {};
		std::string sidWpt = "";
		std::string transition = "";
		std::pair<std::string, bool> intsec = {};
		std::chrono::time_point<std::chrono::utc_clock, std::chrono::seconds> lastUpdate;
		int updateCounter = 0;
		/*bool request = false;*/
		std::string request = "";
		long long reqTime = -1;
		bool validEquip = true;
		std::string gndState = "";
		bool ctl = false;
		bool mapp = false;
		// altitude tracking during acft landing phase
		int ldgAlt = 0;
	};

	namespace fplnhelper
	{
		//************************************
		// Description: Strip the filed route from SID/RWY and/or SID to have a bare route to populate with set SID.
		// Any SIDs or RWYs will be deleted and have to be reset.
		// Method:    clean
		// FullName:  vsid::fplnhelper::clean
		// Access:    public 
		// Returns:   std::vector<std::string>
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: std::string filedSidWpt
		//************************************
		std::vector<std::string> clean(const EuroScopePlugIn::CFlightPlan &FlightPlan, std::string filedSidWpt = "");

		//************************************
		// Description: Compares available transitions with the route and returns a matching transition
		// Method:    getTransition
		// FullName:  vsid::fpln::getTransition
		// Access:    public 
		// Returns:   std::string - found transition or empty string if not
		// Qualifier:
		// Parameter: const std::vector<std::string> & route - filed route split in single elements
		// Parameter: const std::map<std::string, vsid::Transition> & - map of all transitions for a given SID
		//************************************		
		std::string getTransition(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::map<std::string, vsid::Transition>& transition,
			const std::string &filedSidWpt);

		//************************************
		// Description: Splits the SID at 'X' for use with SIDxTRANS
		// Method:    splitTransition
		// FullName:  vsid::fpln::splitTransition
		// Access:    public 
		// Returns:   std::string - the SID stripped from possible transitions
		//	 if the SID itself contains a X it is rebuilt
		// Qualifier:
		// Parameter: std::string atcSid
		//************************************
		std::string splitTransition(std::string atcSid);

		//************************************
		// Description: Retrieves the "atc block" from a route (SID/RWY or ICAO/RWY)
		// Method:    getAtcBlock
		// FullName:  vsid::fplnhelper::getAtcBlock
		// Access:    public 
		// Returns:   std::pair<std::string, std::string> - first: SID or ICAO, second: RWY
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		std::pair<std::string, std::string> getAtcBlock(const EuroScopePlugIn::CFlightPlan &FlightPlan);

		//************************************
		// Description: Checks flight plan remarks for a given string
		// Method:    findRemarks
		// FullName:  vsid::fplnhelper::findRemarks
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& searchStr
		//************************************
		bool findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& searchStr));

		//************************************
		// Description: Removes a given remark from the flight plan
		// Method:    removeRemark
		// FullName:  vsid::fplnhelper::removeRemark
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& toRemove
		//************************************
		bool removeRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toRemove));

		//************************************
		// Description: Adds the given remark to the flight plan
		// Method:    addRemark
		// FullName:  vsid::fplnhelper::addRemark
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& toAdd
		//************************************
		bool addRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& toAdd));

		//************************************
		// Description: Search for a given entry in the flight plan scratchpad
		// Method:    findScratchPad
		// FullName:  vsid::fplnhelper::findScratchPad
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string & toSearch
		//************************************
		bool findScratchPad(const EuroScopePlugIn::CFlightPlan &FlightPlan, const std::string& toSearch);

		//************************************
		// Description: Set or update the scratchpad. If an entry is present the new entry will be added.
		// Method:    setScratchPad
		// FullName:  vsid::fplnhelper::setScratchPad
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string & toAdd
		//************************************
		std::string addScratchPad(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toAdd);

		//************************************
		// Description: Remove an entry from the scratchpad. Non matching entries are preserved.
		// Method:    removeScratchPad
		// FullName:  vsid::fplnhelper::removeScratchPad
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string & toRemove
		//************************************
		std::string removeScratchPad(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toRemove);

		//************************************
		// Description: Get the filed equipment code for the flight plan. If the acft type is found in the
		// RNAV list a RNAV capable code is returned. If equipment is missing but capabilities are present
		// they are converted from FAA to ICAO, defaulting to RNAV capable code if the entry isn't found in the
		// internal list.
		// Method:    getEquip
		// FullName:  vsid::fplnhelper::getEquip
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::set<std::string> & rnav
		//************************************
		std::string getEquip(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::set<std::string> &rnav = {});

		//************************************
		// Description: Returns flight plan values for the PBN/ field if present
		// Method:    getPbn
		// FullName:  vsid::fplnhelper::getPbn
		// Access:    public 
		// Returns:   std::string - PBN/ field entry or empty string
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		std::string getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan);

		//************************************
		// Description: Stores flight plan info for possible reconnect
		// Method:    saveFplnInfo
		// FullName:  vsid::fplnhelper::saveFplnInfo
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: vsid::Fpln fplnInfo
		// Parameter: std::map<std::string
		// Parameter: vsid::Fpln> & savedFplnInfo
		//************************************
		void saveFplnInfo(const std::string& callsign, vsid::Fpln fplnInfo,
			std::map<std::string, vsid::Fpln>& savedFplnInfo);

		//************************************
		// Description: Restores flight plan info if callsign has stored info
		// Method:    restoreFplnInfo
		// FullName:  vsid::fplnhelper::restoreFplnInfo
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: std::map<std::string
		// Parameter: vsid::Fpln> & processed
		// Parameter: std::map<std::string
		// Parameter: vsid::Fpln> & savedFplnInfo
		//************************************
		bool restoreFplnInfo(const std::string& callsign, std::map<std::string, vsid::Fpln>& processed,
			std::map<std::string, vsid::Fpln>& savedFplnInfo);


		//************************************
		// Description: Restores initial climb if this info was lost (e.g. during a reconnect of a pilot)
		// Method:    restoreIC
		// FullName:  vsid::fplnhelper::restoreIC
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const vsid::Fpln & fplnInfo
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		// Parameter: EuroScopePlugIn::CController atcMyself
		//************************************
		bool restoreIC(const vsid::Fpln& fplnInfo, EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CController atcMyself);
	}
}