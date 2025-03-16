/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flightplans.

Copyright (C) 2024 Gameagle (Philip Maier)

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

#include "airport.h"
#include "include/nlohmann/json.hpp"

#include <string>
#include <map>
#include <filesystem>

using json = nlohmann::ordered_json;

namespace vsid
{
	struct Clrf
	{
		double distWarning = 0.0;
		double distCaution = 0.0;
		int altWarning = 0;
		int altCaution = 0;
	};

	struct tmpSidSettings
	{
		std::string base = "";
		std::string wpt;
		std::string id = "";
		std::string desig = "";
		std::map<std::string, vsid::Transition> transition = {};
		std::vector<std::string> rwys = {};
		int prio = 99;
		int initial = 0;
		bool via = false;
		bool pilotfiled = false;
		std::string wingType = "?LSAT";
		std::map<std::string, bool> acftType = {};
		std::map<std::string, bool> dest = {};
		std::map<std::string, std::map<std::string, std::vector<std::string>>> route = {};
		std::string wtc = "";
		std::string engineType = "";
		int engineCount = 0;
		int mtow = 0;
		std::string customRule = "";
		std::string area = "";
		std::map<std::string, bool> equip = { {"RNAV", true} };
		int lvp = -1;
		std::map<std::string, std::map<std::string, std::string>> actArrRwy = {};
		std::map<std::string, std::map<std::string, std::string>> actDepRwy = {};
		int timeFrom = -1;
		int timeTo = -1;
		bool sidHighlight = false;
		bool clmbHighlight = false;
	};

	class ConfigParser
	{
	public:
		ConfigParser();
		virtual ~ConfigParser();

		/**
		 * @brief Loads the configs for active airports
		 * 
		 * @param activeAirports 
		 * @param savedCustomRules - customRules that are transferred in between rwy change updates
		 * @param savedSettings - settings that are transferred in between rwy change updates
		 * @param savedAreas - area settings that are transferred in between rwy change updates
		 */
		void loadAirportConfig(std::map<std::string, vsid::Airport> &activeAirports,
							std::map<std::string, std::map<std::string, bool>>& savedCustomRules,
							std::map<std::string, std::map<std::string, bool>>& savedSettings,
							std::map<std::string, std::map<std::string, vsid::Area>>& savedAreas,
							std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>& savedRequests,
							std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>>& savedRwyRequests
							);
		/**
		 * @brief Loads vsid config
		 *
		 */
		void loadMainConfig();
		/**
		 * @brief Loads the grp config
		 * 
		 */
		void loadGrpConfig();
		/**
		 * @brief load list of RNAV capable acft
		 */
		void loadRnavList();
		//************************************
		// Description: grants read-only access to vSID main config
		// Method:    getMainConfig
		// FullName:  vsid::ConfigParser::getMainConfig
		// Access:    public 
		// Returns:   const json&
		// Qualifier: const
		//************************************
		inline json& getMainConfig()
		{
			return this->vSidConfig;
		}
		/**
		 * @brief Fetches a specific color from the settings file
		 * 
		 * @param color - name of the key in the settings file
		 * @return COLORREF 
		 */
		const COLORREF getColor(std::string color);

		//************************************
		// Method:    getClrfMinimums
		// FullName:  vsid::ConfigParser::getClrfMinimums
		// Access:    public 
		// Returns:   vsid::Clrf&
		// Qualifier:
		//************************************
		inline Clrf& getClrfMinimums() { return this->clrf; };

		int getReqTime(std::string time);
		json grpConfig;
		std::set<std::string> rnavList;
		
	private:
		std::set<std::filesystem::path> configPaths;
		std::map<std::string, COLORREF> colors;
		std::map<std::string, int> reqTimes;
		Clrf clrf;
		json parsedConfig;
		json vSidConfig;

		inline bool isConfigValue(const std::string& value) const
		{
			std::set<std::string> configValues = { "rwy", "prio", "initial", "climbvia", "wpt", "trans", "pilotfiled", "wingType", "acftType",
				"dest", "route", "wtc", "engineType", "engineCount", "mtow", "customRule", "area", "equip", "lvp", "actArrRwy",
				"actDepRwy", "timeFrom", "timeTo", "sidHighlight", "clmbHighlight"};

			return configValues.contains(value);
		}
	};
}
