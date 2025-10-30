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
#include "eseparser.h"

namespace vsid
{
	const std::string pluginName = "vSID";
	const std::string pluginVersion = "0.14.1";
	const std::string pluginAuthor = "Gameagle";
	const std::string pluginCopyright = "GPL v3";
	const std::string pluginViewAviso = "";

	class Display; // forward declaration

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

		//************************************
		// Description: Returns all stored active airports
		// Method:    getActiveApts
		// FullName:  vsid::VSIDPlugin::getActiveApts
		// Access:    public 
		// Returns:   const std::map<std::string, vsid::Airport>&
		// Qualifier: const
		//************************************
		inline const std::map<std::string, vsid::Airport>& getActiveApts() const { return this->activeAirports; };

		//************************************
		// Description: Returns the SID waypoint found in the route or empty if none was found. Checks ES determined SID first.
		// Considers transition waypoints
		// Method:    findSidWpt
		// FullName:  vsid::VSIDPlugin::findSidWpt
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		//************************************
		std::string findSidWpt(EuroScopePlugIn::CFlightPlan FlightPlan);

		//************************************
		// Description: Iterates over all loaded plugins in search for TopSky and CCAMS
		// Method:    detectPlugins
		// FullName:  vsid::VSIDPlugin::detectPlugins
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void detectPlugins();

		//************************************
		// Description: Search for a matching SID depending on current RWYs in use, SID wpt
		// and configured values in config
		// Method:    processSid
		// FullName:  vsid::VSIDPlugin::processSid
		// Access:    public 
		// Returns:   vsid::Sid
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: std::string atcRwy
		//************************************
		vsid::Sid processSid(EuroScopePlugIn::CFlightPlan& FlightPlan, std::string atcRwy = "");


		//************************************
		// Description Tries to set a clean route without SID. SID will then be placed as first item
		// Processed flight plans are stored.
		// Method:    processFlightplan
		// FullName:  vsid::VSIDPlugin::processFlightplan
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: bool checkOnly
		// Parameter: std::string atcRwy
		// Parameter: vsid::Sid manualSid
		//************************************
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
		inline vsid::ConfigParser& getConfigParser() { return this->configParser; }
		
		//************************************
		// Description: Called with every function call (list interaction) inside ES
		// Method:    OnFunctionCall
		// FullName:  vsid::VSIDPlugin::OnFunctionCall
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: int FunctionId
		// Parameter: const char * sItemString
		// Parameter: POINT Pt
		// Parameter: RECT Area
		//************************************
		void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area);

		//************************************
		// Description: Handles events on tag item updates
		// Method:    OnGetTagItem
		// FullName:  vsid::VSIDPlugin::OnGetTagItem
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		// Parameter: EuroScopePlugIn::CRadarTarget RadarTarget
		// Parameter: int ItemCode
		// Parameter: int TagData
		// Parameter: char sItemString[16]
		// Parameter: int * pColorCode
		// Parameter: COLORREF * pRGB
		// Parameter: double * pFontSize
		//************************************
		void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize);
		
		//************************************
		// Description: Called when a command is used in chat and ES couldn't resolve it.
		// Method:    OnCompileCommand
		// FullName:  vsid::VSIDPlugin::OnCompileCommand
		// Access:    public 
		// Returns:   bool - always needs to be true if a matching command should be removed from the chat area
		// Qualifier:
		// Parameter: const char * sCommandLine
		//************************************
		bool OnCompileCommand(const char* sCommandLine);
		
		//************************************
		// Description: Called when something is changed in the flight plan (e.g. route)
		// Method:    OnFlightPlanFlightPlanDataUpdate
		// FullName:  vsid::VSIDPlugin::OnFlightPlanFlightPlanDataUpdate
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		//************************************
		void OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan);
		
		//************************************
		// Description: Called when something is changed in the controller assigned data
		// Method:    OnFlightPlanControllerAssignedDataUpdate
		// FullName:  vsid::VSIDPlugin::OnFlightPlanControllerAssignedDataUpdate
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		// Parameter: int DataType
		//************************************
		void OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType);
		
		//************************************
		// Description: Called when a flight plan disconnects from the network
		// Method:    OnFlightPlanDisconnect
		// FullName:  vsid::VSIDPlugin::OnFlightPlanDisconnect
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		//************************************
		void OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan);

		//************************************
		// Description: Called when a position for radar target is updated
		// Method:    OnRadarTargetPositionUpdate
		// FullName:  vsid::VSIDPlugin::OnRadarTargetPositionUpdate
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CRadarTarget RadarTarget
		//************************************
		void OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget RadarTarget);
		
		//************************************
		// Description: Called whenever a controller position is updated. ~ every 5 seconds
		// Method:    OnControllerPositionUpdate
		// FullName:  vsid::VSIDPlugin::OnControllerPositionUpdate
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CController Controller
		//************************************
		void OnControllerPositionUpdate(EuroScopePlugIn::CController Controller);
		
		//************************************
		// Description: Called if a controller disconnects
		// Method:    OnControllerDisconnect
		// FullName:  vsid::VSIDPlugin::OnControllerDisconnect
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CController Controller
		//************************************
		void OnControllerDisconnect(EuroScopePlugIn::CController Controller);
		
		//************************************
		// Description: Called when the user clicks on the ok button of the runway selection dialog
		// Method:    OnAirportRunwayActivityChanged
		// FullName:  vsid::VSIDPlugin::OnAirportRunwayActivityChanged
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void OnAirportRunwayActivityChanged();
		
		//************************************
		// Description: Called once a second
		// Method:    OnTimer
		// FullName:  vsid::VSIDPlugin::OnTimer
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: int Counter
		//************************************
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

		//************************************
		// Description: Checks if a flight plan (or radar target) is inside the vis range of the controller
		// Method:    outOfVis
		// FullName:  vsid::VSIDPlugin::outOfVis
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		bool outOfVis(EuroScopePlugIn::CFlightPlan& FlightPlan);
		
		//************************************
		// Description: Called when a new radar screen is created (e.g. opening a new .asr file)
		// Method:    OnRadarScreenCreated
		// FullName:  vsid::VSIDPlugin::OnRadarScreenCreated
		// Access:    public 
		// Returns:   EuroScopePlugIn::CRadarScreen*
		// Qualifier:
		// Parameter: const char * sDisplayName
		// Parameter: bool NeedRadarContent
		// Parameter: bool GeoReferenced
		// Parameter: bool CanBeSaved
		// Parameter: bool CanBeCreated
		//************************************
		EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);

		//************************************
		// Description: Resets and deletes stored pointers to radar screens
		// Method:    deleteScreen
		// FullName:  vsid::VSIDPlugin::deleteScreen
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: int id
		//************************************
		void deleteScreen(int id);

		//************************************
		// Description: Cleanup work before the plugin is unloaded or ES is exited
		// Method:    exit
		// FullName:  vsid::VSIDPlugin::exit
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void exit();

		//************************************
		// Description: Calling ES StartTagFunction with a reference to any of the stored (valid) screens. A radar screen is needed for execution.
		// Method:    callExtFunc
		// FullName:  vsid::VSIDPlugin::callExtFunc
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const char * sCallsign - acft which TAG is clicked
		// Parameter: const char * sItemPlugInName - item provider plugin (for base ES use NULL)
		// Parameter: int ItemCode - the item code
		// Parameter: const char * sItemString - string of the selected item
		// Parameter: const char * sFunctionPlugInName - item provider plugin (for base ES use NULL)
		// Parameter: int FunctionId - id of the function
		// Parameter: POINT Pt - mouse position
		// Parameter: RECT Area - area covered by tag item
		//************************************
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
		//************************************
		// Description: Stores runway requests during airport updates
		// Param 1: int - id of the saved screen pointer (always increased during runtime)
		// Param 2: shared_ptr - derived class of CRadarScreens
		//************************************
		std::map<int, std::shared_ptr<vsid::Display>> radarScreens = {};
		int screenId = 0;
		// pointer to plugin itself for data exchange and control of loading/unloading
		std::shared_ptr<vsid::VSIDPlugin> shared;
		// internal storage of parsed atc stations
		std::set<vsid::SectionAtc> sectionAtc;
		// internal storage of parsed sids
		std::set<vsid::SectionSID> sectionSids;

		//************************************
		// Description: Loads and updates the active airports with available configs
		// Method:    UpdateActiveAirports
		// FullName:  vsid::VSIDPlugin::UpdateActiveAirports
		// Access:    private 
		// Returns:   void
		// Qualifier:
		//************************************
		void UpdateActiveAirports();

		//************************************
		// Description: Tracks if multiple scratch pad entries for the same callsign are save to be sent
		// Param 1: std::string - callsign
		// Param 2: std::string - last entry processed
		//************************************
		std::unordered_map<std::string, bool> spReleased;

		//************************************
		// Description: List of messages to be send for syncing infos for different callsigns
		// Param 1: std::string - callsign
		// Param 2: std::deque - messages list
		// Param 2a: std::string - new scratch pad msg
		// Param 2b: std::string - old scratch pad msg (to be restored)
		//************************************
		std::unordered_map<std::string, std::deque<std::pair<std::string, std::string>>> syncQueue;
		bool spWorkerActive = false;
		std::atomic_bool queueInProcess = false;

		std::map<std::string, int> atcSiFailCounter;
		
		//************************************
		// Description: Processes all sync messages for held callsigns - each run works on all callsigns that are released
		// for new msgs
		// Method:    processSPQueue
		// FullName:  vsid::VSIDPlugin::processSPQueue
		// Access:    private 
		// Returns:   void
		// Qualifier:
		//************************************
		void processSPQueue();

		//************************************
		// Description: Adds a new msg pair to the sync queue
		// Method:    addSyncQueue
		// FullName:  vsid::VSIDPlugin::addSyncQueue
		// Access:    private 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: const std::string & newScratch
		// Parameter: const std::string & oldScratch
		//************************************
		inline void addSyncQueue(const std::string& callsign, const std::string& newScratch, const std::string& oldScratch)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] adding scratch to queue. New: \"" +
				newScratch + "\" | Old: \"" + oldScratch + "\"", vsid::MessageHandler::DebugArea::Dev);

			if (!this->spReleased.contains(callsign)) this->spReleased[callsign] = true;
			this->syncQueue[callsign].push_back({newScratch, oldScratch});
		}		
		
		//************************************
		// Description: Updates a callsign to be released for new scratch pad messages to be sent
		// Method:    updateSPSyncRelease
		// FullName:  vsid::VSIDPlugin::updateSPSyncRelease
		// Access:    private 
		// Returns:   void
		// Qualifier:
		// Parameter: std::string callsign
		//************************************
		inline void updateSPSyncRelease(std::string callsign)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] updating sync release", vsid::MessageHandler::DebugArea::Dev);

			if(this->spReleased.contains(callsign)) this->spReleased[callsign] = true;
			else messageHandler->writeMessage("DEBUG", "[" + callsign + "] failed to update sync release. Not held in release list.", vsid::MessageHandler::DebugArea::Dev);
		}

		//************************************
		// Description: Load ese file and parse it if found
		// Method:    loadEse
		// FullName:  vsid::VSIDPlugin::loadEse
		// Access:    private 
		// Returns:   void
		// Qualifier:
		//************************************
		void loadEse();
	};
}
