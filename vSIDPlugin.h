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
#include <deque>

#include "include/es/EuroScopePlugIn.h"
#include "airport.h"
#include "constants.h"

#include "flightplan.h"
#include "configparser.h"
#include "utils.h"

namespace vsid
{
	const std::string pluginName = "vSID";
	const std::string pluginVersion = "0.13.0";
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

		inline std::map<std::string, vsid::Fpln>& getProcessed() { return this->processed; };
		inline std::set<std::string> getDepRwy(std::string icao)
		{
			if (this->activeAirports.contains(icao))
			{
				return this->activeAirports[icao].depRwys;
			}
			else return {};
		}
		
		inline const std::map<std::string, vsid::Airport>& getActiveApts() const { return this->activeAirports; };

		/**
		 * @brief Extract a sid waypoint. If ES doesn't find a SID the route is compared to available SID waypoints
		 * 
		 * @param FlightPlanData 
		 * @return
		 */
		std::string findSidWpt(EuroScopePlugIn::CFlightPlan FlightPlan);
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
		vsid::Sid processSid(EuroScopePlugIn::CFlightPlan& FlightPlan, std::string atcRwy = "");

		/**
		 * @brief Tries to set a clean route without SID. SID will then be placed in front
		 * and color codes for the TagItem set. Processed flight plans are stored.
		 * 
		 * @param FlightPlan - Flightplan data from ES
		 * @param checkOnly - If the flight plan should only be validated
		 * @param atcRwy - The rwy assigned by ATC which shall be considered
		 * @param manualSid - manual Sid that has been selected and should be processed
		 */
		void processFlightplan(EuroScopePlugIn::CFlightPlan& FlightPlan, bool checkOnly, std::string atcRwy = "", vsid::Sid manualSid = {});

		//************************************
		// Description: Removes given callsign from any requests for the specified airport
		// Method:    removeFromRequests
		// FullName:  vsid::VSIDPlugin::removeFromRequests
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: const std::string & icao
		//************************************
		void removeFromRequests(const std::string& callsign, const std::string& icao);

		//************************************
		// Description: Retrieves the config parser as read-only for access to configs
		// Method:    getConfigParser
		// FullName:  vsid::VSIDPlugin::getConfigParser
		// Access:    public 
		// Returns:   const vsid::ConfigParser
		// Qualifier: const
		//************************************
		inline vsid::ConfigParser& getConfigParser()
		{	
			return this->configParser;
		}

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
		 * @brief Called when a dot command is used and ES couldn't resolve it.
		 * ES then checks this functions to evaluate the command
		 *
		 * @param sCommandLine
		 * @return
		 */
		bool OnCompileCommand(const char* sCommandLine);

		/*EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);*/

		/**
		 * @brief Called when something is changed in the flight plan (used for route updates)
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
		 * @brief Called when a flight plan disconnects from the network
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
		
		//************************************
		// Description: Syncs present requests for the given flight plan
		// Method:    syncReq
		// FullName:  vsid::VSIDPlugin::syncReq
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		void syncReq(EuroScopePlugIn::CFlightPlan& FlightPlan);

		/*std::string syncReq(EuroScopePlugIn::CFlightPlan& FlightPlan);*/ // #dev - mutex sync
		
		
		//************************************
		// Description: Syncs saved gnd states and clearance flag
		// Method:    syncStates
		// FullName:  vsid::VSIDPlugin::syncStates
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		void syncStates(EuroScopePlugIn::CFlightPlan& FlightPlan);

		bool outOfVis(EuroScopePlugIn::CFlightPlan& FlightPlan);
		
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
			int FunctionId, POINT Pt, RECT Area);
		
	private:
		std::map<std::string, vsid::Airport> activeAirports;
		std::map<std::string, vsid::Fpln> processed;
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
		std::map<std::string, vsid::Fpln> savedFplnInfo = {};
		//************************************
		// Description: Stores requests during airport updates
		// Param 1: std::string - airport icao
		// Param 2: std::string - request type
		// Param 3 (pair): std::string - callsign
		// Param 4 (pair): long long - time
		//************************************
		std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>> savedRequests = {};
		//************************************
		// Description: Stores runway requests during airport updates
		// Param 1: std::string - airport icao
		// Param 2: std::string - request type
		// Param 3: std::string - runway
		// Param 4 (pair): std::string - callsign
		// Param 5 (pair): long long - time
		//************************************
		std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>> savedRwyRequests = {};
		// list of ground states set by controllers
		std::string gsList;
		std::map<std::string, std::string> actAtc;
		std::set<std::string> ignoreAtc;
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

		//************************************
		// Description: Tracks if multiple scratchpad entries for the same callsign are save to be sent
		// Param 1: std::string - callsign
		// Param 2: std::string - last entry processed
		//************************************
		std::unordered_map<std::string, bool> spReleased;
		std::unordered_map<std::string, std::deque<std::string>> syncQueue;
		bool spWorkerActive = false;
		std::atomic_bool queueInProcess = false;
		
		inline void processSPQueue()
		{
			if (queueInProcess || this->syncQueue.empty()) return;
			messageHandler->writeMessage("DEBUG", "Started sync processing queue... (size: " + std::to_string(this->syncQueue.size()) + ")", vsid::MessageHandler::DebugArea::Dev);

			queueInProcess = true;

			for (std::unordered_map<std::string, std::deque<std::string>>::iterator it = this->syncQueue.begin(); it != this->syncQueue.end();)
			{
				std::string callsign = it->first;

				EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());

				if (!FlightPlan.IsValid())
				{
					this->spReleased.erase(callsign);
					it = this->syncQueue.erase(it);
					continue;
				}

				messageHandler->writeMessage("DEBUG", "[" + callsign + "] queue processing... total msg queue size: " + std::to_string(it->second.size()), vsid::MessageHandler::DebugArea::Dev);

				if (this->spReleased.contains(callsign) && this->spReleased[callsign])
				{
					std::string scratch = std::move(it->second.front());
					it->second.pop_front();

					// #evaluate - removing double entries - + 8 lines below

					size_t pos = 0;
					for (auto jt = it->second.begin(); jt != it->second.end(); ++jt)
					{
						if (*jt == scratch) ++pos;
					}

					if (pos != 0 && pos <= it->second.size())
					{
						it->second.erase(it->second.begin(), it->second.begin() + pos);
					}

					messageHandler->writeMessage("DEBUG", "[" + callsign + "] #1 (Queue) sync released... " + ((this->spReleased[callsign]) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Dev);
					
					this->spReleased[callsign] = false;
					spWorkerActive = true; // reset on received update

					messageHandler->writeMessage("DEBUG", "[" + callsign + "] (Queue) scratchpad sending scratch \"" + scratch + "\"", vsid::MessageHandler::DebugArea::Dev);

					FlightPlan.GetControllerAssignedData().SetScratchPadString(vsid::utils::trim(scratch).c_str());

					if (vsid::utils::tolower(scratch).find(".vsid") == std::string::npos)
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] (Queue) setting released inside worker... TRUE", vsid::MessageHandler::DebugArea::Dev);
						this->spReleased[callsign] = true;
					}
				}
				else if (this->spReleased.contains(callsign) && !this->spReleased[callsign]) // #dev - sync - remove only debugging
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] #2 (Queue) sync released... " + ((this->spReleased[callsign]) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Dev);
				}
				else if (!spReleased.contains(callsign))
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] (Queue) release check not found...", vsid::MessageHandler::DebugArea::Dev);

					this->spReleased.erase(callsign);
					it = this->syncQueue.erase(it);
					continue;
				}

				if (it->second.size() == 0)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] no more messages in queue. Removing... ", vsid::MessageHandler::DebugArea::Dev);
					it = this->syncQueue.erase(it);
					continue;
				}

				++it;
			}
			queueInProcess = false;
			messageHandler->writeMessage("DEBUG", "Finished sync processing queue...", vsid::MessageHandler::DebugArea::Dev);
		}

		inline void addSyncQueue(const std::string& callsign, const std::string& scratch)
		{
			if (scratch.find(".vsid_error") != std::string::npos)
			{
				messageHandler->writeMessage("ERROR", "Failed to get updated scratchpad entry for sync queue.");
				return;
			}

			messageHandler->writeMessage("DEBUG", "[" + callsign + "] adding scratch to queue \"" + scratch + "\"", vsid::MessageHandler::DebugArea::Dev);

			if (!this->spReleased.contains(callsign)) this->spReleased[callsign] = true;
			this->syncQueue[callsign].push_back(scratch);
		}
		
		inline void updateSPSyncRelease(std::string callsign)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] updating sync release", vsid::MessageHandler::DebugArea::Dev);

			this->spReleased[callsign] = true;
		}
		// dev
	};
}
