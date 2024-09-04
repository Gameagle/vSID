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

#include "area.h"
#include "sid.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>

// dev
#include "messageHandler.h"
// end dev

namespace vsid
{
	struct Controller
	{
		std::string si = "";
		int facility = 0;
		double freq = 0.0;
	};
	struct Airport
	{
		/**
		 * @brief Compare operator for custom request sorting.
		 *
		 */
		struct compreq
		{
			bool operator()(auto l, auto r) const
			{
				return l.second > r.second;
			}
		};

		std::string icao = "";
		int elevation = 0;
		bool equipCheck = true;
		std::vector<std::string> allRwys = {};
		std::set<std::string> depRwys = {};
		std::set<std::string> arrRwys = {};
		std::map<std::string, bool> customRules = {};
		std::map<std::string, vsid::Area> areas = {};
		std::vector<vsid::Sid> sids = {};
		std::vector<vsid::Sid> timeSids = {};
		std::string timezone = "";
		std::map<std::string, int> appSI = {};
		//bool arrAsDep = false;
		int transAlt = 0;
		int maxInitialClimb = 0;
		std::map<std::string, bool> settings = {};
		std::map<std::string, vsid::Controller> controllers = {};
		std::map<std::string, std::set<std::pair<std::string, long long>, compreq>> requests = {};
		bool forceAuto = false;

		/**
		 * @brief Checks if another controller with a lower facility is online
		 * 
		 * @param myself - Controller().ControllerMyself()
		 * @param toActivate - if the check should consider activation of automode (true) or only if it needs to be disabled
		 */
		inline bool hasLowerAtc(const EuroScopePlugIn::CController &myself, bool toActivate = false)
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
			else if (myself.GetFacility() >= 5 &&
				std::none_of(controllers.begin(), controllers.end(), [&](auto controller)
					{
						if (controller.second.facility < myself.GetFacility()) return true;
						else if (appSI.contains(myself.GetPositionId()) &&
							appSI.contains(controller.second.si) &&
							((appSI[myself.GetPositionId()] > appSI[controller.second.si] && !toActivate) ||
							(appSI[myself.GetPositionId()] >= appSI[controller.second.si] && toActivate)))
							 return true;
						else if (!appSI.contains(myself.GetPositionId()) &&
							appSI.contains(controller.second.si)) return true;
						else return false;
					}))
			{
				return false;
			}
			else return true;
		}

		inline bool isSidWpt(const std::string& wpt)
		{
			return std::any_of(sids.begin(), sids.end(), [&](vsid::Sid sid)
				{
					return wpt == sid.waypoint;
				});
		}
	};
}
