/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim of vSID is to ease the work of any controller that edits and assigns
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

#include "area.h"
#include "sid.h"
#include "utils.h"
#include "logger.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <type_traits>

namespace vsid::apt
{
	struct AtcData {
		std::string si;
		int facility;
		double freq;
		std::unordered_set<std::string> Icaos;
	};

	struct AirportData
	{
		//************************************
		// Description: Compare operator for custom request sorting.
		//************************************
		struct compreq
		{
			bool operator()(auto l, auto r) const
			{
				return l.second > r.second;
			}
		};

		//************************************
		// Description: Rules map with case insensitive compare
		// first: std::string - rule name
		// second: bool - if the rule is active
		//************************************
		using CustomRulesMap = std::map<std::string, bool, vsid::utils::CICompare>;

		//************************************
		// Description: Area map with case insensitive compare
		// first: std::string - area name
		// second: vsid::Area
		//************************************
		using CustomAreaMap = std::map<std::string, vsid::Area, vsid::utils::CICompare>;

		//************************************
		// Description: Set containing pair of callsign and request time
		// std::pair first: std::string - callsign
		// std::pair second: long long - request time
		//************************************
		using RequestSet = std::set<std::pair<std::string, long long>, compreq>;

		//************************************
		// Description: Request map with case insensitive compare
		// first: std::string - request name
		// std::pair second: RequestSet
		//************************************
		using CustomRequestMap = std::map<std::string, RequestSet, vsid::utils::CICompare>;

		//************************************
		// Description: RWY Request map with case insensitive compare
		// first: std::string - request name
		// second: std::map<std::string (rwy), RequestSet>
		//************************************
		using CustomRwyRequestMap = std::map<std::string, std::map<std::string, RequestSet, vsid::utils::CICompare>, vsid::utils::CICompare>;

		std::string icao = "";
		int elevation = 0;
		bool equipCheck = true;
		bool enableRVSids = true;
		std::vector<std::string> allRwys = {};
		std::set<std::string> depRwys = {};
		std::set<std::string> arrRwys = {};
		CustomRulesMap customRules = {};
		CustomAreaMap areas = {};
		//************************************
		// Description: Stores rwy intersections for intsec menu
		// Param 1: std::string - rwy
		// Param 2 (vec): std::string - available intsec for the rwy
		//************************************
		std::map<std::string, std::vector<std::string>> intsec = {};
		std::vector<vsid::Sid> sids = {};
		std::vector<vsid::Sid> timeSids = {};
		std::string timezone = "";
		std::map<std::string, int> appSI = {};
		//bool arrAsDep = false;
		int transAlt = 0;
		int maxInitialClimb = 0;
		bool autoHandoff = true;
		std::map<std::string, bool> settings = {};
		std::unordered_map<std::string, vsid::apt::AtcData, vsid::utils::StringHash, std::equal_to<>> controllers = {};
		//************************************
		// Description: Stores requests during airport updates
		// Param 1: std::string - request type
		// Param 2 (pair): std::string - callsign
		// Param 3 (pair): long long - time
		//************************************
		CustomRequestMap requests = {};
		//************************************
		// Description: Stores runway requests
		// Param 1: std::string - request type
		// Param 2: std::string - runway
		// Param 3 (pair): std::string - callsign
		// Param 4 (pair): long long - time
		//************************************
		CustomRwyRequestMap rwyrequests = {};
		bool forceAuto = false;
		/**
		 * @brief Checks if another controller with a lower facility is online
		 * 
		 * @param myself - Controller().ControllerMyself()
		 * @param toActivate - if the check should consider activation of automode (true) or only if it needs to be disabled
		 */
		inline bool hasLowerAtc(const EuroScopePlugIn::CController &myself, bool toActivate = false) const
		{
			if (std::all_of(controllers.begin(), controllers.end(), [&](auto controller)
				{
					if (toActivate)
					{
						return controller.second.facility > myself.GetFacility();
					}
					else return controller.second.facility >= myself.GetFacility();
					
				}))
			{
				return false;
			}

			if (myself.GetFacility() >= 5 &&
				std::none_of(controllers.begin(), controllers.end(), [&](auto controller)
					{
						if (controller.second.facility < myself.GetFacility()) return true;

						if (appSI.contains(myself.GetPositionId()) &&
							appSI.contains(controller.second.si) &&
							((appSI.at(myself.GetPositionId()) > appSI.at(controller.second.si) && !toActivate) ||
							(appSI.at(myself.GetPositionId()) >= appSI.at(controller.second.si) && toActivate)))
							 return true;
						
						if (!appSI.contains(myself.GetPositionId()) &&
							appSI.contains(controller.second.si)) return true;
						
						return false;
					}))
			{
				return false;
			}
			
			return true;
		}

		inline bool isSidWpt(const std::string& wpt) const
		{
			return std::any_of(sids.begin(), sids.end(), [&](vsid::Sid sid)
				{
					return wpt == sid.waypoint;
				});
		}

		//************************************
		// Method:    isDepRwy
		// FullName:  vsid::Airport::isDepRwy
		// Access:    public 
		// Returns:   bool
		// Qualifier: const
		// Parameter: const std::string & rwy - the runway to check
		// Parameter: bool arrAsDep - if arr runways should count as dep rwy
		//************************************
		inline bool isDepRwy(const std::string& rwy, bool arrAsDep = false) const
		{
			if (depRwys.contains(rwy)) return true;
			else if (arrAsDep == true && arrRwys.contains(rwy)) return true;
			else return false;
		}
	};

	using AirportData = vsid::apt::AirportData;
	using AtcData = vsid::apt::AtcData;

	class AirportManager
	{
		AirportManager() { vsid::Logger::log(LogLevel::Debug, "Airport Manager initialized", DebugLevel::Gen); };
		~AirportManager() { vsid::Logger::log(LogLevel::Debug, "Airport Manager destroyed", DebugLevel::Gen); };

	public:

		//************************************
		// Description: Returns all stored active airports
		// Method:    getAirports
		// FullName:  vsid::apt::AirportManager::getAirports
		// Access:    public static 
		// Returns:   const std::map<std::string, vsid::apt::Airport, vsid::utils::CICompare>&
		// Qualifier:
		//************************************
		[[nodiscard]]
		static const std::map<std::string, AirportData, vsid::utils::CICompare>& getAirports() { return activeAirports_; };

		static const AirportData* getAirport(std::string_view icao) 
		{
			const auto it = activeAirports_.find(icao);
			if (it == activeAirports_.end()) return nullptr;

			return &it->second;
		}

		//************************************
		// Description: Checks if an airport is active in the airport manager
		// Method:    isActive
		// FullName:  vsid::apt::AirportManager::isActive
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view icao
		//************************************
		static bool isActive(std::string_view icao) { return activeAirports_.contains(icao); };

		//************************************
		// Description: Adds an airport to the airport manager
		// Method:    addAirport
		// FullName:  vsid::apt::AirportManager::addAirport
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & icao
		// Parameter: const Airport & airport
		//************************************
		static void add(const std::string& icao, const AirportData& airport) { activeAirports_[icao] = std::move(airport); }

		//************************************
		// Description: Removes an airport from the airport manager
		// Method:    removeAirport
		// FullName:  vsid::apt::AirportManager::removeAirport
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		// Parameter: std::string_view icao
		//************************************
		static void remove(const std::string& icao) { activeAirports_.erase(icao); };

		//************************************
		// Description: Updates (multiple) airport data for a given icao if present
		// Method:    update
		// FullName:  vsid::apt::AirportManager::update
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view icao
		// Parameter: Func & & func
		//************************************
		template<typename Func>
			requires std::invocable<Func&&, AirportData&>
		[[nodiscard]]
		static bool update(std::string_view icao, Func&& func)
		{
			auto it = activeAirports_.find(icao);

			if (it == activeAirports_.end())
			{
				vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] could not update airport. Info not held.", icao),
					vsid::DebugLevel::Fpln);

				return false;
			}

			AirportData& data = it->second;

			std::invoke(std::forward<Func>(func), data);

			return true;

			// Usage
			/*
			AirportManager::update(icao, [](AirportData& data) {
				data.enableRVSids = true;
				data.timeSids = {};
				data.XYZ ...
			});
			*/
		}

		//************************************
		// Description: Sets a single airport data member via invoking the update function
		// Method:    set
		// FullName:  vsid::apt::AirportManager::set
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view icao
		// Parameter: Member FplnData:: * member
		// Parameter: Value & & value
		//************************************
		template<typename Member, typename Value>
			requires std::is_assignable_v<Member&, Value&&>
		[[nodiscard]]
		static bool set(std::string_view icao, Member AirportData::* member, Value&& value)
		{
			return update(icao, [member, &value](AirportData& data)
				{
					data.*member = std::forward<Value>(value);
				});

			// Usage
			/*
			AirportManager::set(icao, &AirportData::enableRVSids, true);
			AirportManager::set(icao, &AirportData::customRules, std::move(newCustomRules);
			*/
		}

		static bool empty() { return activeAirports_.empty(); };

	private:
		inline static std::map<std::string, AirportData, vsid::utils::CICompare> activeAirports_;
	};
}
