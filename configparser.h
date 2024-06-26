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
#include "nlohmann/json.hpp"

#include <string>
#include <map>

using json = nlohmann::ordered_json;

namespace vsid
{
	class ConfigParser
	{
	public:
		ConfigParser();
		virtual ~ConfigParser();

		/**
		 * @brief Loads the configs for active airports
		 * 
		 * @param activeAirports 
		 * @param savedCustomRules - customRules that are transferred inbetween rwy change updates
		 * @param savedSettings - settings that are transferred inbetween rwy change updates
		 * @param savedAreas - area settings that are transferred inbetween rwy change updates
		 */
		void loadAirportConfig(std::map<std::string, vsid::Airport> &activeAirports,
							std::map<std::string, std::map<std::string, bool>>& savedCustomRules,
							std::map<std::string, std::map<std::string, bool>>& savedSettings,
							std::map<std::string, std::map<std::string, vsid::Area>>& savedAreas,
							std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>& savedRequests
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
		 * @brief Fetches a specific color from the settings file
		 * 
		 * @param color - name of the key in the settings file
		 * @return COLORREF 
		 */
		COLORREF getColor(std::string color);

		int getReqTime(std::string time);
		json grpConfig;
		
	private:
		std::set<std::filesystem::path> configPaths;
		std::map<std::string, COLORREF> colors;
		std::map<std::string, int> reqTimes;
		json parsedConfig;
		json vSidConfig;
	};
}
