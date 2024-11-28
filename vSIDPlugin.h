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

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <sstream>

#include "es/EuroScopePlugIn.h"
#include "airport.h"
#include "constants.h"

#include "flightplan.h"
#include "configparser.h"
#include "utils.h"


namespace vsid
{
	const std::string pluginName = "vSID";
	const std::string pluginVersion = "0.11.0";
	const std::string pluginAuthor = "Gameagle";
	const std::string pluginCopyright = "GPL v3";
	const std::string pluginViewAviso = "";

	class Display; // forward declaration

	/**
	 * @brief Main class communicating with ES
	 *
	 */
	class VSIDPlugin : public EuroScopePlugIn::CPlugIn
	{
	public:
		VSIDPlugin();
		virtual ~VSIDPlugin();

		inline std::map<std::string, vsid::fpln::Info>& getProcessed() { return this->processed; };
		inline std::set<std::string> getDepRwy(std::string icao)
		{
			if (this->activeAirports.contains(icao))
			{
				return this->activeAirports[icao].depRwys;
			}
			else return {};
		}
		
		inline std::map<std::string, vsid::Airport> getActiveApts() { return this->activeAirports; };

		/**
		 * @brief Extract a sid waypoint. If ES doesn't find a SID the route is compared to available SID waypoints
		 * 
		 * @param FlightPlanData 
		 * @return
		 */
		std::string findSidWpt(EuroScopePlugIn::CFlightPlanData FlightPlanData);
		/**
		 * @brief Iterate over loaded .dll and check for topsky and ccams
		 * 
		 */
		void detectPlugins();

		/**
		 * @brief Search for a matching SID depending on current RWYs in use, SID wpt
		 * and configured SIDs in config
		 * 
		 * @param FlightPlan - Flightplan data from ES
		 * @param atcRwy - The rwy assigned by ATC which shall be considered
		 */
		vsid::Sid processSid(EuroScopePlugIn::CFlightPlan FlightPlan, std::string atcRwy = "");
		/**
		 * @brief Tries to set a clean route without SID. SID will then be placed in front
		 * and color codes for the TagItem set. Processed flightplans are stored.
		 * 
		 * @param FlightPlan - Flightplan data from ES
		 * @param checkOnly - If the flightplan should only be validated
		 * @param atcRwy - The rwy assigned by ATC which shall be considered
		 * @param manualSid - manual Sid that has been selected and should be processed
		 */
		void processFlightplan(EuroScopePlugIn::CFlightPlan FlightPlan, bool checkOnly, std::string atcRwy = "", vsid::Sid manualSid = {});
		/**
		 * @brief Called with every function call (list interaction) inside ES
		 *
		 * @param FunctionId - registered TagItemFunction Id
		 * @param sItemString
		 * @param Pt
		 * @param Area
		 */
		void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area);
		/**
		 * @brief Handles events on plane position updates if the flightplan is present in a tagItem
		 * 
		 * @param FlightPlan 
		 * @param RadarTarget 
		 * @param ItemCode 
		 * @param TagData 
		 * @param sItemString 
		 * @param pColorCode 
		 * @param pRGB 
		 * @param pFontSize 
		 */
		void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize);
		/**
		 * @brief Called when a dot commmand is used and ES couldn't resolve it.
		 * ES then checks this functions to evaluate the command
		 *
		 * @param sCommandLine
		 * @return
		 */
		bool OnCompileCommand(const char* sCommandLine);

		/*EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);*/

		/**
		 * @brief Called when something is changed in the flightplan (used for route updates)
		 * 
		 * @param FlightPlan 
		 */
		void OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan);
		/**
		 * @brief Called when something is changed in the controller assigned data
		 *
		 * @param FlightPlan - the flight plan reference whose controller assigned data is updated
		 * @param DataType - the type of the data updated (CTR_DATA_TYPE ...)
		 */
		void OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType);
		/**
		 * @brief Called when a flightplan disconnects from the network
		 * 
		 * @param FlightPlan 
		 */
		void OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan);
		/**
		 * @brief Called when a position for radar target is updated
		 * 
		 * @param RadarTarget
		 */
		void OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget RadarTarget);
		/**
		 * @brief Called whenever a controller position is updated. ~ every 5 seconds
		 * 
		 * @param Controller 
		 */
		void OnControllerPositionUpdate(EuroScopePlugIn::CController Controller);
		/**
		 * @brief Called if a controller disconnects
		 * 
		 * @param Controller 
		 */
		void OnControllerDisconnect(EuroScopePlugIn::CController Controller);
		/**
		 * @brief Called when the user clicks on the ok button of the runway selection dialog
		 *
		 */
		void OnAirportRunwayActivityChanged();
		/**
		 * @brief Called once a second
		 * 
		 * @param Counter 
		 * @return * void 
		 */
		void OnTimer(int Counter);
		/**
		 * @brief Sync states and clearance flag to new controller
		 * 
		 * @param FlightPlan - ES flightplan object
		 */
		void syncStates(EuroScopePlugIn::CFlightPlan FlightPlan);
		/**
		 * @brief Radar Screen.
		 */
		EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);
		/**
		 * @brief Resets and deletes stored pointers to radar screens
		 * 
		 * @param id of the pointer
		 */
		void deleteScreen(int id);

		/**
		 * @brief missing explanation
		 * 
		 */
		void exit();

		/**
		 * @brief Calling ES StartTagFunction with a reference to any of the stored (valid) screens
		 * 
		 * @param sCallsign - acft which TAG is clicked
		 * @param sItemPlugInName - item provider plugin (for base ES use NULL)
		 * @param ItemCode - the item code
		 * @param sItemString - string of the selected item
		 * @param sFunctionPlugInName - item provider plugin (for base ES use NULL)
		 * @param FunctionId - id of the function
		 * @param Pt - mouse position
		 * @param Area - area covered by tag item
		 */
		void callExtFunc(const char* sCallsign, const char* sItemPlugInName, int ItemCode, const char* sItemString, const char* sFunctionPlugInName,
			int FunctionId);
		
	private:
		std::map<std::string, vsid::Airport> activeAirports;
		std::map<std::string, vsid::fpln::Info> processed;
		/**
		 * @param std::map<std::string,> callsign
		 * @param std::pair<,bool> fpln is disconnected
		 */
		std::map<std::string, std::pair< std::chrono::utc_clock::time_point, bool>> removeProcessed;
		vsid::ConfigParser configParser;
		std::string configPath;
		std::map<std::string, std::map<std::string, bool>> savedSettings;
		std::map<std::string, std::map<std::string, bool>> savedRules;
		std::map<std::string, std::map<std::string, vsid::Area>> savedAreas;
		std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>> savedRequests = {};
		// list of ground states set by controllers
		std::string gsList;
		std::map<std::string, std::string> actAtc;
		std::set<std::string> ignoreAtc;
		bool preferTopsky = false;
		bool topskyLoaded = false;
		bool ccamsLoaded = false;
		/**
		 * @param key - id of the saved screen pointer (always increased during runtime)
		 * @param value - derived class of CRadarScreens
		 */

		std::map<int, std::shared_ptr<vsid::Display>> radarScreens = {};
		int screenId = 0;
		/**
		 * @brief pointer to plugin itself for data exchange and control of loading/unloading
		 */
		std::shared_ptr<vsid::VSIDPlugin> shared;

		/**
		 * @brief Loads and updates the active airports with available configs
		 *
		 */
		void UpdateActiveAirports();
	};
}
