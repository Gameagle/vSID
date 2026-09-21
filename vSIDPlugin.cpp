#include "pch.h"

#include "vSIDPlugin.h"
#include "timeHandler.h"
#include "messageHandler.h"
#include "area.h"
#include "Psapi.h"

#include <set>
#include <algorithm>
#include <ranges>

#include "display.h"
#include "airportManager.h"
#include "fplnManager.h"
#include "syncManager.h"
#include "crashhandler.h"

using FplnManager = vsid::fpln::FplnManager;
using AirportManager = vsid::apt::AirportManager;
using SyncManager = vsid::sync::SyncManager;

vsid::VSIDPlugin* vsidPlugin; // pointer needed for ES

vsid::VSIDPlugin::VSIDPlugin() : EuroScopePlugIn::CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, pluginName.c_str(), pluginVersion.c_str(), pluginAuthor.c_str(), pluginCopyright.c_str()) {
	vsid::Logger::initialize(); // initialize logger
	vsid::Logger::setLogDevOnly(this->configParser.logDevOnly);
	vsid::time::logTzdbVersion();

	this->detectPlugins();
	this->configParser.loadMainConfig();
	this->configParser.loadGrpConfig();
	this->configParser.loadRnavList();

	/* takes over pointer control of vsidPlugin - no deletion needed for unloading*/
	this->shared = std::shared_ptr<vsid::VSIDPlugin>(this);
	instance_ = this;

	// messageHandler->setLevel("INFO"); #dev - new logger
	
	RegisterTagItemType("SID", TAG_ITEM_VSID_SIDS);
	RegisterTagItemFunction("Set/Reset suggested SID", TAG_FUNC_VSID_SIDS_AUTO);
	RegisterTagItemFunction("SIDs Menu", TAG_FUNC_VSID_SIDS_MAN);

	RegisterTagItemType("Departure Transition", TAG_ITEM_VSID_TRANS);
	RegisterTagItemFunction("Transition Menu", TAG_FUNC_VSID_TRANS);

	RegisterTagItemType("Initial Climb", TAG_ITEM_VSID_CLIMB);
	RegisterTagItemFunction("Climb Menu", TAG_FUNC_VSID_CLMBMENU);

	RegisterTagItemType("Departure Runway", TAG_ITEM_VSID_RWY);
	RegisterTagItemFunction("Runway Menu", TAG_FUNC_VSID_RWYMENU);

	RegisterTagItemType("Squawk", TAG_ITEM_VSID_SQW);

	RegisterTagItemType("Request", TAG_ITEM_VSID_REQ);
	RegisterTagItemFunction("Request Menu", TAG_FUNC_VSID_REQMENU);

	RegisterTagItemType("Request Timer", TAG_ITEM_VSID_REQTIMER);

	RegisterTagItemType("Cleared to land flag", TAG_ITEM_VSID_CTLF);
	RegisterTagItemFunction("Set cleared to land flag", TAG_FUNC_VSID_CTL);

	RegisterTagItemType("Cleared to land flag (local)", TAG_ITEM_VSID_CTLF_LOCAL);
	RegisterTagItemFunction("Set cleared to land flag (local)", TAG_FUNC_VSID_CTL_LOCAL);

	RegisterTagItemType("Clearance received flag (CRF)", TAG_ITEM_VSID_CLR);
	RegisterTagItemFunction("Set CRF and SID", TAG_FUNC_VSID_CLR_SID);
	RegisterTagItemFunction("Set CRF, SID and Startup state", TAG_FUNC_VSID_CLR_SID_SU);

	RegisterTagItemType("Intersection", TAG_ITEM_VSID_INTS);
	RegisterTagItemFunction("Set runway intersection", TAG_FUNC_VSID_INTS_SET);
	RegisterTagItemFunction("Select runway intersection as able", TAG_FUNC_VSID_INTS_ABLE);

	RegisterTagItemFunction("Auto-Assign Squawk (TopSky)", TAG_FUNC_VSID_TSSQUAWK);

	RegisterTagItemType("Handover Flag", TAG_ITEM_VSID_HOVF);
	RegisterTagItemFunction("Set handover flag", TAG_FUNC_VSID_HOV);

	this->loadEse(); // load and parse ese file

	if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
		vsid::Logger::log(LogLevel::Error, "Failed to init curl_global");
	else this->curlInit = true;

	DisplayUserMessage("Message", "vSID", std::string("Version " + pluginVersion + " loaded").c_str(), true, true, false, false, false);	
}

vsid::VSIDPlugin::~VSIDPlugin()
{
	vsid::Logger::log(LogLevel::Debug, "VSIDPlugin destroyed.", DebugLevel::Gen);
};

/*
* BEGIN OWN FUNCTIONS
*/

void vsid::VSIDPlugin::detectPlugins()
{
	HMODULE hmods[1024];
	HANDLE hprocess;
	DWORD cbneeded;
	unsigned int i;

	hprocess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	if (hprocess == NULL) return;

	if (EnumProcessModules(hprocess, hmods, sizeof(hmods), &cbneeded))
	{
		for (i = 0; i < (cbneeded / sizeof(HMODULE)); i++)
		{
			TCHAR szModName[MAX_PATH];

			if (GetModuleFileNameEx(hprocess, hmods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
			{
				std::string modname = vsid::utils::tolower(szModName);

				//if (modname == "CCAMS.dll")
				if (modname.find("ccams.dll") != std::string::npos)
				{
					vsid::Logger::log(LogLevel::Debug, "CCAMS Plugin detected.", vsid::DebugLevel::Gen);
					this->ccamsLoaded = true;
				}
				//if (modname == "TopSky.dll")
				if (modname.find("topsky.dll") != std::string::npos)
				{
					vsid::Logger::log(LogLevel::Debug, "TopSky Plugin detected.", vsid::DebugLevel::Gen);
					this->topskyLoaded = true;
				}
			}
		}
	}
	CloseHandle(hprocess);
}

vsid::Sid vsid::VSIDPlugin::processSid(EuroScopePlugIn::CFlightPlan& FlightPlan, std::string atcRwy)
{
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string adep = fplnData.GetOrigin();
	std::string ades = fplnData.GetDestination();
	std::string callsign = FlightPlan.GetCallsign();
	std::vector<std::string> filedRoute = vsid::utils::split(std::string(fplnData.GetRoute()), ' ');
	std::string sidWpt = vsid::fpln::findSidWpt(FlightPlan);
	vsid::Sid setSid = {};
	int prio = 99;
	bool customRuleActive = false;
	std::set<std::string> wptRules = {};
	std::set<std::string> actTSid = {};
	bool validEquip = true;

	if (!AirportManager::isActive(adep))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] is not an active airport. Skipping all SID checks", adep), DebugLevel::Sid);

		return vsid::Sid();
	}

	const auto aptData = AirportManager::getData(adep);

	if (aptData == nullptr)
	{
		vsid::Logger::log(
			LogLevel::Warning,
			std::format("[{}] failed to get the airport. Returning empty SID", adep)
		);

		return {};
	}

	auto& processed = FplnManager::getProcessed();

	std::set<std::string> depRwys = aptData->depRwys; // dep rwys to merge with arr rwys if needed

	// determine if a rule is active

	if (!aptData->customRules.empty())
	{
		customRuleActive = std::any_of(
			aptData->customRules.begin(),
			aptData->customRules.end(),
			[](auto item)
			{
				return item.second;
			}
		);
	}

	if (customRuleActive) // #evaluate - arrrwy check for areas below ~ 325
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}] Custom rules active. Checking for avbl areas and possible arr as dep rwys.",
			callsign, adep), DebugLevel::Sid);

		vsid::apt::AirportData::CustomRulesMap customRules = aptData->customRules;

		for (auto it = aptData->sids.begin(); it != aptData->sids.end();)
		{
			if (it->customRule == "" ||
				(aptData->customRules.contains(it->customRule) && // #refactor - remove double lookup
					!aptData->customRules.at(it->customRule)))
			{
				++it; continue;
			}

			if (it->area != "")
			{
				for (std::string area : vsid::utils::split(it->area, ','))
				{
					if (aptData->areas.contains(area) && // #refactor - remove double lookup
						aptData->areas.at(area).isActive &&
						aptData->areas.at(area).arrAsDep)
					{
						std::set<std::string> arrRwys = aptData->arrRwys;
						depRwys.merge(arrRwys);
					}
				}
			}
			++it;
		}
	}

	// determining "night" SIDs (any SID with a set time frame)

	if (aptData->settings.at("time") &&
		aptData->timeSids.size() > 0 &&
		std::any_of(aptData->timeSids.begin(),
			aptData->timeSids.end(),
			[&](auto item)
			{
				return sidWpt == item.waypoint;
			}
		))
	{
		for (const vsid::Sid& sid : aptData->timeSids)
		{
			if (sid.waypoint != sidWpt) continue;
			if (vsid::time::isActive(aptData->timezone, sid.timeFrom, sid.timeTo))
			{
				actTSid.insert(sid.waypoint);
			}
		}
	}

	// include atc set rwy if present as dep rwy

	std::string actAtcRwy = "";

	if (!atcRwy.empty())
	{
		if (auto it = processed.find(callsign); it != processed.end())
		{
			if(it->second.atcRWY) actAtcRwy = atcRwy;
		}
		else if(aptData->settings.at("auto") &&
			(vsid::fpln::findRemarks(FlightPlan, "VSID/RWY") || fplnData.IsAmended())) actAtcRwy = atcRwy;
	}

	for (const vsid::Sid& currSid : aptData->sids)
	{
		if (!actAtcRwy.empty()) depRwys.insert(actAtcRwy);

		// skip if current SID does not match found SID wpt
		if (currSid.transition.empty() && currSid.waypoint != sidWpt && currSid.waypoint != "XXX") continue;
		else if (!currSid.transition.empty() && !currSid.transition.contains(sidWpt)) continue;

		bool rwyMatch = false;
		bool restriction = false; // #evaluate - can probably be removed, tmp unused
		validEquip = true;


		// checking areas for arrAsDep - actual area evaluation down below

		if (currSid.area != "")
		{
			std::vector<std::string> sidAreas = vsid::utils::split(currSid.area, ',');

			if (std::any_of(sidAreas.begin(), sidAreas.end(), [&](auto sidArea) // #evaluate - wrong areas might result in an arr rwy becoming dep rwy
				{
					if (aptData->areas.contains(sidArea) &&
						aptData->areas.at(sidArea).isActive &&
						aptData->areas.at(sidArea).inside(FlightPlan.GetFPTrackPosition().GetPosition()))
					{
						return true;
					}
					else return false;
				}))
			{
				for (std::string& area : sidAreas)
				{
					if (aptData->areas.contains(area) && aptData->areas.at(area).arrAsDep)
					{
						std::set<std::string> arrRwys = aptData->arrRwys;
						depRwys.merge(arrRwys);
					}
				}
			}
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Checking SID [{}]", callsign, currSid.idName()), DebugLevel::Sid);

		// skip if SID needs active arr rwys

		if (!currSid.actArrRwy.empty())
		{
			if (currSid.actArrRwy.contains("allow"))
			{
				if (currSid.actArrRwy.at("allow").at("all") != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy.at("allow").at("all"), ',');
					if (!std::all_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (aptData->arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because not all of the required arr Rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
				else if (currSid.actArrRwy.at("allow").at("any") != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy.at("allow").at("any"), ',');
					if (std::none_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (aptData->arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because none of the required arr rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
			}

			if (currSid.actArrRwy.contains("deny"))
			{
				if (currSid.actArrRwy.at("deny").at("all") != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy.at("deny").at("all"), ',');
					if (std::all_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (aptData->arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because all of the forbidden arr Rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
				else if (currSid.actArrRwy.at("deny").at("any") != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy.at("deny").at("any"), ',');
					if (std::any_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (aptData->arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because at least one of the forbidden arr rwys is active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
			}
		}

		// skip if SID needs active dep rwys

		if (!currSid.actDepRwy.empty())
		{
			if (currSid.actDepRwy.contains("allow"))
			{
				if (currSid.actDepRwy.at("allow").at("all") != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy.at("allow").at("all"), ',');
					if (!std::all_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because not all of the required dep Rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
				else if (currSid.actDepRwy.at("allow").at("any") != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy.at("allow").at("any"), ',');
					if (std::none_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because none of the required dep rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
			}

			if (currSid.actDepRwy.contains("deny"))
			{
				if (currSid.actDepRwy.at("deny").at("all") != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy.at("deny").at("all"), ',');
					if (std::all_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because all of the forbidden dep Rwys are active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
				else if (currSid.actDepRwy.at("deny").at("any") != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy.at("deny").at("any"), ',');
					if (std::any_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because at least one of the forbidden dep rwys is active",
							callsign, currSid.idName()), DebugLevel::Sid);
						continue;
					}
				}
			}
		}

		// skip if current SID rwys don't match dep rwys

		std::vector<std::string> skipAtcRWY = {};
		std::vector<std::string> skipSidRWY = {};

		for (std::string depRwy : depRwys)
		{
			// skip if a rwy has been set manually and it doesn't match available dep rwys
			if (atcRwy != "" && atcRwy != depRwy)
			{
				skipAtcRWY.push_back(depRwy);
				continue;
			}
			// skip if airport dep rwys are not part of the SID

			if (!vsid::utils::contains(currSid.rwys, depRwy))
			{
				skipSidRWY.push_back(depRwy);
				continue;
			}
			else
			{
				rwyMatch = true;
				break;
			}
		}
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}] available rwys (merged if arrAsDep in active area or rwy set by atc): {}",
			callsign, adep, vsid::utils::join(depRwys)), DebugLevel::Sid);

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}] Skipped ATC RWYs [{}] | Skipped SID RWY [{}]",
			callsign, adep, vsid::utils::join(skipAtcRWY), vsid::utils::join(skipSidRWY)), DebugLevel::Sid);

		// skip if custom rules are active but the current sid has no rule or has a rule but this is not active

		if (customRuleActive && currSid.customRule != "" &&
			aptData->customRules.contains(currSid.customRule) &&
			!aptData->customRules.at(currSid.customRule))
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because the SID custom rule is not active.",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// skip if aircraft type does not match (heli, fixed wing) - unknown aircraft is never skipped

		if (currSid.wingType.find(fplnData.GetAircraftType()) == std::string::npos && fplnData.GetAircraftType() != '?')
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because wing type [{}] doesn't match.",
				callsign, currSid.idName(), fplnData.GetAircraftType()), DebugLevel::Sid);

			continue;
		}

		// skip if equipment does not match

		if (aptData->equipCheck)
		{
			std::string equip = vsid::fpln::getEquip(FlightPlan, this->configParser.rnavList);
			std::string pbn = vsid::fpln::getPbn(FlightPlan);

			if (currSid.equip.contains("RNAV"))
			{
				if (equip.size() > 1 && equip.find_first_of("ABGRI") == std::string::npos && pbn == "")
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because RNAV ('A', 'B', 'G', 'R' or 'I') "
						"is required, but not found in equipment [{}] and PBN is empty",
						callsign, currSid.idName(), equip), DebugLevel::Sid);

					validEquip = false;
					continue;
				}
			}
			else if (currSid.equip.contains("NON-RNAV"))
			{
				if (equip.size() < 2 && pbn == "")
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because only NON-RNAV is allowed, but equipment "
						"[{}] has less than 2 entries and PBN is empty",
						callsign, currSid.idName(), equip), DebugLevel::Sid);

					validEquip = false;
					continue;
				}

				if (equip.find_first_of("GRI") != std::string::npos || pbn != "")
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because only NON-RNAV is allowed, but RNAV ('G', 'R' or 'I') "
						"was found in equipment [{}] or PBN is not empty",
						callsign, currSid.idName(), equip), DebugLevel::Sid);

					validEquip = false;
					continue;
				}
			}
			else
			{
				for (const auto& sidEquip : currSid.equip)
				{
					if (sidEquip.second && equip.find(sidEquip.first) == std::string::npos)
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because equipment [{}] is mandatory but was not found in equipment [{}]",
							callsign, currSid.idName(), sidEquip.first, equip), DebugLevel::Sid);

						validEquip = false;
						continue;
					}
					if (!sidEquip.second && equip.find(sidEquip.first) != std::string::npos)
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because equipment [{}] is forbidden but was found in equipment [{}]",
							callsign, currSid.idName(), sidEquip.first, equip), DebugLevel::Sid);

						validEquip = false;
						continue;
					}
				}
			}
		}

		// skip if transition part of SID and it doesn't match

		if (!currSid.transition.empty())
		{
			bool transMatch = false;
			for (auto& [base, _] : currSid.transition)
			{
				if (std::find(filedRoute.begin(), filedRoute.end(), base) == filedRoute.end()) continue;
				else transMatch = true;
			}

			if (!transMatch)
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because none of the transitions found in the route",
					callsign, currSid.idName()), DebugLevel::Sid);

				continue;
			}
		}

		// skip if custom rules are inactive but a rule exists in sid

		if (!customRuleActive && currSid.customRule != "")
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because no rule is active and the SID has a rule configured",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// area checks

		if (currSid.area != "")
		{
			std::vector<std::string> sidAreas = vsid::utils::split(currSid.area, ',');

			// skip if areas are inactive but set
			if (std::all_of(sidAreas.begin(),
				sidAreas.end(),
				[&](auto sidArea)
				{
					if (aptData->areas.contains(sidArea))
					{
						if (!aptData->areas.at(sidArea).isActive) return true;
						else return false;
					}
					else return false; // warning for wrong config in next area check to prevent doubling
				}))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because all areas are inactive but one is set in the SID",
					callsign, currSid.idName()), DebugLevel::Sid);

				continue;
			}

			// skip if area is active + fpln outside
			if (std::none_of(sidAreas.begin(),
				sidAreas.end(),
				[&](auto sidArea)
				{
					if (aptData->areas.contains(sidArea))
					{
						if (!aptData->areas.at(sidArea).isActive ||
							(aptData->areas.at(sidArea).isActive &&
								!aptData->areas.at(sidArea).inside(FlightPlan.GetFPTrackPosition().GetPosition()))
							) return false;
						else return true;
					}
					else
					{
						vsid::Logger::log(LogLevel::Warning, std::format("Area [{}] not in config for [{}]. Check your config.", sidArea, adep));

						return false;
					} // fallback
				}))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because the plane is not in one of the active areas",
					callsign, currSid.idName()), DebugLevel::Sid);
				continue;
			}
		}

		// skip if lvp ops are active but SID is not configured for lvp ops and lvp is not disabled for SID
		if (aptData->settings.at("lvp") && !currSid.lvp && currSid.lvp != -1)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because LVP is active and SID is not configured for LVP "
				"or LVP check is not disabled for SID",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// skip if lvp ops are inactive but SID is configured for lvp ops

		if (!aptData->settings.at("lvp") && currSid.lvp && currSid.lvp != -1)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because LVP is inactive and SID is configured for LVP "
				"or LVP check is not disabled for SID",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// skip if no matching rwy was found in SID;

		if (!rwyMatch)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of a RWY mismatch between SID rwy and active DEP rwy",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}


		// skip if engine type doesn't match

		if (currSid.engineType != "" && currSid.engineType.find(fplnData.GetEngineType()) == std::string::npos)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of a mismatch in engineType",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}
		else if (currSid.engineType != "")
		{
			restriction = true;
		}

		// skip if an aircraft type is set in sid but is set to false

		if ((currSid.acftType.contains(fplnData.GetAircraftFPType()) &&
			!currSid.acftType.at(fplnData.GetAircraftFPType())) ||
			std::any_of(currSid.acftType.begin(), currSid.acftType.end(), [&](auto type)
				{
					return type.second && !currSid.acftType.contains(fplnData.GetAircraftFPType());
				}))
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of a mismatch in aircraft type "
				"(type is set to false or type is set to true but plane is not of the type)",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}
		else if (!currSid.acftType.empty())
		{
			restriction = true;
		}

		// skip if SID has engineNumber requirement and acft doesn't match
		if (!vsid::utils::containsDigit(currSid.engineCount, fplnData.GetEngineNumber()))
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of a mismatch in engine number",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}
		else if (currSid.engineCount > 0)
		{
			restriction = true;
		}

		// skip if SID has WTC requirement and acft doesn't match
		if (currSid.wtc != "" && currSid.wtc.find(fplnData.GetAircraftWtc()) == std::string::npos)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of mismatch in WTC",
				callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}
		else if (currSid.wtc != "")
		{
			restriction = true;
		}

		// skip if SID has mtow requirement and acft is too heavy - if grp config has not yet been loaded load it
		if (currSid.mtow)
		{
			if (this->configParser.grpConfig.size() == 0)
			{
				this->configParser.loadGrpConfig();
			}
			std::string acftType = fplnData.GetAircraftFPType();
			bool mtowMatch = false;
			for (auto it = this->configParser.grpConfig.begin(); it != this->configParser.grpConfig.end(); ++it)
			{
				if (it->value("ICAO", "") != acftType) continue;
				if (it->value("MTOW", 0) > currSid.mtow) break; // acft icao found but to heavy, no further checks
				mtowMatch = true;
				break; // acft light enough, no further checks
			}
			if (!mtowMatch)
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because of MTOW",
					callsign, currSid.idName()), DebugLevel::Sid);

				continue;
			}
			else restriction = true;
		}

		// skip if destination restrictions present and flight plan doesn't match

		if (!currSid.dest.empty())
		{
			if (currSid.dest.contains(ades) && !currSid.dest.at(ades))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because destination [{}] is not allowed",
					callsign, currSid.idName(), ades), DebugLevel::Sid);

				continue;
			}
			else if (!currSid.dest.contains(ades) && std::any_of(currSid.dest.begin(), currSid.dest.end(), [](const std::pair<std::string, bool>& sidDest)
				{
					return sidDest.second;
				}))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because destination [{}] was not found for this SID and the SID has mandatory destinations",
					callsign, currSid.idName(), ades), DebugLevel::Sid);

				continue;
			}

		}

		// skip if route restrictions present and flight plan doesn't match

		if (!currSid.route.empty())
		{
			if (currSid.route.contains("allow"))
			{
				bool validRoute = false;
				for (const auto& [_, route] : currSid.route.at("allow"))
				{
					std::vector<std::string>::iterator startPos;

					try
					{
						startPos = std::find(filedRoute.begin(), filedRoute.end(), route.at(0));

						if (startPos == filedRoute.end())
							vsid::Logger::log(
								LogLevel::Debug,
								std::format(
									"[{}] skipping SID [{}] route checking for [{}] because first mandatory wpt [{}] "
									"was not found in route",
									callsign,
									currSid.idName(),
									vsid::utils::join(route, " "),
									route.at(0)),
								DebugLevel::Sid
							);

						messageHandler->removeFplnError(callsign, ERROR_FPLN_ALLOWROUTE);
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_ALLOWROUTE))
						{
							vsid::Logger::log(
								LogLevel::Error,
								std::format(
									"[{}] Failed to get first position for allowed route in SID [{}] "
									"checking SID route [{}]. Code: {}",
									callsign,
									currSid.idName(),
									vsid::utils::join(route, " "),
									ERROR_FPLN_ALLOWROUTE)
							);

							messageHandler->addFplnError(callsign, ERROR_FPLN_ALLOWROUTE);
						}
					}

					bool lastMatch = false;
					bool allowSkip = false;
					bool stopRoute = false;

					for (const std::string& wpt : route)
					{
						if (stopRoute) break;

						if (wpt == std::string("..."))
						{
							allowSkip = true;
							continue;
						}

						for (std::vector<std::string>::iterator it = startPos; it != filedRoute.end();)
						{
							if (*it == std::string("DCT"))
							{
								startPos++;
								it++;
								continue;
							}

							vsid::Logger::log(LogLevel::Debug, std::format("[{}] checking mand. wpt [{}] vs [{}]", callsign, wpt, *it), DebugLevel::Sid, true);

							if (wpt == *it)
							{
								allowSkip = false;
								lastMatch = true;
								startPos++;
								break;
							}
							else if (allowSkip)
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] skipping wpt [{}] as skipping is allowed", callsign, *it), DebugLevel::Sid);
								lastMatch = false;
								it++;
								startPos++;
								continue;
							}
							else
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] mismatch between mand. wpt [{}] vs [{}]", callsign, wpt, *it), DebugLevel::Sid);
								lastMatch = false;
								stopRoute = true;
								break;
							}
						}
					}

					if (lastMatch)
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] mand. route in [{}] was found. Accepting.", callsign, currSid.idName()), DebugLevel::Sid);
						validRoute = true;
						break;
					}
				}
				if (!validRoute)
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] skipping SID [{}] because none of the allowed routes matched", callsign,
						currSid.idName()), DebugLevel::Sid);

					continue;
				}
			}

			if (currSid.route.contains("deny"))
			{
				bool invalidRoute = false;

				for (const auto& [_, route] : currSid.route.at("deny"))
				{
					std::vector<std::string>::iterator startPos;

					try
					{
						startPos = std::find(filedRoute.begin(), filedRoute.end(), route.at(0));

						if (startPos == filedRoute.end())
						{
							vsid::Logger::log(LogLevel::Debug, std::format("[{}] accepting SID [{}] because first waypoint of denied route wasn't found",
								callsign, currSid.idName()), DebugLevel::Sid);

							messageHandler->removeFplnError(callsign, ERROR_FPLN_DENYROUTE);

							break;
						}
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_DENYROUTE))
						{
							vsid::Logger::log(
								LogLevel::Error,
								std::format(
									"[{}] Failed to get first position for denied route in SID {} "
									"checking SID route [{}]. Code: {}",
									callsign,
									currSid.idName(),
									vsid::utils::join(route, " "),
									ERROR_FPLN_DENYROUTE)
							);

							messageHandler->addFplnError(callsign, ERROR_FPLN_DENYROUTE);
						}
					}

					bool lastMatch = false;
					bool allowSkip = false;
					bool stopRoute = false;

					for (const std::string& wpt : route)
					{
						if (stopRoute) break;

						if (wpt == std::string("..."))
						{
							allowSkip = true;
							continue;
						}

						for (std::vector<std::string>::iterator it = startPos; it != filedRoute.end();)
						{
							if (*it == std::string("DCT"))
							{
								startPos++;
								it++;
								continue;
							}

							vsid::Logger::log(LogLevel::Debug, std::format("[{}] checking forb. wpt [{}] vs [{}]", callsign, wpt, *it), DebugLevel::Sid, true);

							if (wpt == *it)
							{
								allowSkip = false;
								lastMatch = true;
								startPos++;
								break;
							}
							else if (allowSkip)
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] skipping wpt [{}] as it is allowed", callsign, *it), DebugLevel::Sid);

								lastMatch = false;
								it++;
								startPos++;
								continue;
							}
							else
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] mismatch between forb. wpt [{}] vs. [{}]", callsign, wpt, *it), DebugLevel::Sid);
								lastMatch = false;
								stopRoute = true;
								break;
							}
						}
					}

					if (lastMatch)
					{
						invalidRoute = true;
						break;
					}
				}

				if (invalidRoute)
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] skipping SID [{}] because a denied route matched", callsign, currSid.idName()), DebugLevel::Sid);

					continue;
				}
				else
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] accepting SID [{}] because no denied route matched", callsign, currSid.idName()), DebugLevel::Sid);
			}
		}

		// skip if sid has night times set but they're not active
		if (!actTSid.contains(currSid.waypoint) &&
			(currSid.timeFrom != -1 || currSid.timeTo != -1)
			)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because time is set and not active", callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// skip the SID if only accepted as pilot filed but it differs

		if (currSid.pilotfiled && currSid.name() != fplnData.GetSidName())
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] as pilot filed only. Filed SID [{}]", callsign, currSid.idName(), fplnData.GetSidName()), DebugLevel::Sid);

			continue;
		}

		// if a SID has the special prio "0" return an empty SID for forced manual selection
		if (currSid.prio == 0)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] special prio value '0' detected. Returning empty SID for forced manual mode", callsign), DebugLevel::Sid);

			if (auto it = processed.find(callsign); it != processed.end())
			{
				FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
					{
						data.validEquip = true;
					});
			}

			return vsid::Sid();
		}

		if (currSid.prio == 99)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because prio is 99 (manual only SID)", callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		if (currSid.prio > prio)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping SID [{}] because prio is higher", callsign, currSid.idName()), DebugLevel::Sid);

			continue;
		}

		// update SID if a better prio is found for non-pilot filed SIDs

		setSid = currSid;
		prio = currSid.prio;
	}

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] Setting SID [{}]", callsign, setSid.idName()), DebugLevel::Sid);

	// if the last valid SID fails due to equipment return a special "EQUIP" sid to also handle yet unprocessed fplns
	// reset in processFlightplan()
	if (!validEquip && setSid.empty())
	{
		setSid.base = "EQUIP";

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Re-Setting special SID base 'EQUIP' as the last possible SID failed due to equipment checks", callsign), DebugLevel::Sid);
	}

	return(setSid);
}

void vsid::VSIDPlugin::removeFromRequests(const std::string& callsign, const std::string& icao)
{
	if (AirportManager::isActive(icao))
	{
		const auto aptData = AirportManager::getData(icao);

		if (aptData == nullptr)
		{
			vsid::Logger::log(
				LogLevel::Warning,
				std::format("[{}] couldn't be removed from requests at [{}] as the airport was invalid.", callsign, icao)
			);
		}

		auto& processed = FplnManager::getProcessed();

		// create mutable request list and update airport data accordingly

		auto mutableRequests = aptData->requests;

		for (auto it = mutableRequests.begin(); it != mutableRequests.end(); ++it)
		{
			for (std::set<std::pair<std::string, long long>>::iterator jt = it->second.begin(); jt != it->second.end();)
			{
				if (jt->first != callsign)
				{
					++jt;
					continue;
				}
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] erasing from request [{}] at [{}]", callsign, it->first, icao), DebugLevel::Req);

				if (processed.contains(callsign))
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.request = "";
							data.reqTime = -1;
						});
				}
				
				jt = it->second.erase(jt);
			}
		}

		(void)AirportManager::update(icao, [&mutableRequests](vsid::apt::AirportData& data)
			{
				data.requests = mutableRequests;
			});

		// create mutable rwy request list and update airport data accordingly

		auto mutableRwyRequests = aptData->rwyrequests;

		for (auto& [type, rwys] : mutableRwyRequests)
		{
			for (auto it = rwys.begin(); it != rwys.end(); ++it)
			{
				for (auto jt = it->second.begin(); jt != it->second.end();)
				{
					if (jt->first != callsign)
					{
						++jt;
						continue;
					}
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] erasing from rwy request [{}] at [{}]", callsign, type, icao), DebugLevel::Req);

					if (processed.contains(callsign))
					{
						FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
							{
								data.request = "";
								data.reqTime = -1;
							});
					}
					
					jt = it->second.erase(jt);
				}
			}
		}

		(void)AirportManager::update(icao, [&mutableRwyRequests](vsid::apt::AirportData& data)
			{
				data.rwyrequests = mutableRwyRequests;
			});
	}
}

bool vsid::VSIDPlugin::outOfVis(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (!FlightPlan.IsValid()) return true; // if the flight plan is invalid it cannot be in vis range

	const std::string callsign = FlightPlan.GetCallsign();
	const EuroScopePlugIn::CController me = ControllerMyself();
	const int myRange = me.GetRange();
	const std::string myCallsign = me.GetCallsign();
	const double myFreq = me.GetPrimaryFrequency();
	const EuroScopePlugIn::CPosition myPos = me.GetPosition();
	const EuroScopePlugIn::CRadarTarget rt = this->RadarTargetSelect(callsign.c_str());

	// assume target is in vis range if RT is invalid (= uncorrelated in S/C-Mode in ES settings) and if callsign selected RT is also invalid
	if (!FlightPlan.GetCorrelatedRadarTarget().IsValid())
	{
		if (!rt.IsValid()) return false;

		const EuroScopePlugIn::CPosition rtPos = rt.GetPosition().GetPosition();

		if (myPos.DistanceTo(rtPos) > myRange)
		{
			for (auto& atc : this->sectionAtc)
			{
				if (myCallsign == atc.callsign || atcFreqMatch(ControllerMyself(), atc))
				{
					if (atc.visPoints.empty()) return true;

					return std::all_of(atc.visPoints.begin(), atc.visPoints.end(), [myRange, rtPos](const EuroScopePlugIn::CPosition& visPoint)
						{
							return visPoint.DistanceTo(rtPos) > myRange;
						});
				}
			}
			return true;
		}
		return false;
	}

	const EuroScopePlugIn::CPosition fpPos = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition();

	if (myPos.DistanceTo(fpPos) > myRange)
	{
		for (auto& atc : this->sectionAtc)
		{
			if (myCallsign == atc.callsign || myFreq == atc.freq)
			{
				if (atc.visPoints.empty()) return true;

				return std::all_of(atc.visPoints.begin(), atc.visPoints.end(), [myRange, fpPos](const EuroScopePlugIn::CPosition& visPoint)
					{
						return visPoint.DistanceTo(fpPos) > myRange;
					});
			}
		}
		return true;
	}
	return false;
}

void vsid::VSIDPlugin::loadEse()
{
	json& vSidConfig = this->configParser.getMainConfig();

	if (vSidConfig.is_null())
	{
		vsid::Logger::log(LogLevel::Error, "Failed to parse main config. (Critical!)");

		return;
	}
		
	if (!vSidConfig.contains("esePath"))
	{
		vsid::Logger::log(LogLevel::Error, "Config value esePath is missing. (Critical!)");

		return;
	}

	std::string pathBuffer(MAX_PATH, '\0');
	DWORD len = GetModuleFileNameA((HINSTANCE)&__ImageBase, pathBuffer.data(), MAX_PATH);
	pathBuffer.resize(len);

	std::filesystem::path basePath = pathBuffer;
	basePath.remove_filename();

	std::string esePath = vSidConfig.at("esePath");
	basePath.append(esePath);
	basePath = basePath.lexically_normal();
	basePath.make_preferred();

	std::vector<std::filesystem::path> eseFileNames;
		
	try
	{
		for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
		{
			if (!std::filesystem::is_directory(entry) && entry.extension() == ".ese")
			{
				eseFileNames.push_back(entry.filename());
			}
		}

		if (eseFileNames.empty())
		{
			vsid::Logger::log(LogLevel::Error, std::format("Couldn't find .ese file(s) in [{}]", basePath.string()));
			return;
		}
	}
	catch (std::filesystem::filesystem_error& e)
	{
		vsid::Logger::log(LogLevel::Error, std::format("Failed to scan ese directory [{}]", e.what()));
	}
	
	bool expected = false;
	if (!this->parsingActive_.compare_exchange_strong(expected, true))
	{
		vsid::Logger::log(LogLevel::Warning, "ESE parsing already running in the background.");
		return;
	}

	this->parserThread_ = std::jthread([this, basePath, eseFileNames = std::move(eseFileNames)]()
		{
			vsid::Logger::log(LogLevel::Debug, "Started async parsing", DebugLevel::Ese);
			try
			{
				vsid::EseParser eseParser;
				

				for (const auto& fileName : eseFileNames)
				{
					eseParser.parseEse(basePath, fileName);
				}

				vsid::EseBuffer tmpBuffer = eseParser.getBuffer();

				vsid::Logger::log(LogLevel::Info,
					std::format("ESE parsing complete. [{}] stations | [{}] SIDs",
						tmpBuffer.sectionAtc.size(),
						tmpBuffer.sectionSids.size()));

				{
					std::lock_guard<std::mutex> lock(this->bufferMtx_);
					this->eseBuffer_ = std::move(tmpBuffer);
				}		

				this->eseDataRdy_ = true;
			}
			catch (const std::exception& e)
			{
				vsid::Logger::log(LogLevel::Error, std::format("Critical error in async ESE parsing: {}", e.what()));
			}
			catch (...)
			{
				vsid::Logger::log(LogLevel::Error, "Unknown error occurred in async ESE parsing.");
			}

			this->parsingActive_ = false;
		});
}

void vsid::VSIDPlugin::addOrSetSquawk(const std::string& callsign, bool forceTS)
{
	long long timeDiff = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastSquawkTP).count();

	if (timeDiff >= 2)
	{
		if (this->topskyLoaded && (forceTS || this->getConfigParser().preferTopsky))
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling TS Squawk func", callsign), DebugLevel::Func);

			this->callExtFunc(callsign.c_str(), "TopSky plugin", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "TopSky plugin", 667, POINT(), RECT());

			this->lastSquawkTP = std::chrono::steady_clock::now();
		}
		else if (this->ccamsLoaded)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling CCAMS Squawk func", callsign), DebugLevel::Func);

			this->callExtFunc(callsign.c_str(), "CCAMS", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "CCAMS", 871, POINT(), RECT());

			this->lastSquawkTP = std::chrono::steady_clock::now();
		}
	}
	else
	{
		auto it = std::find(this->squawkQueue.begin(), this->squawkQueue.end(), callsign);

		if (it == this->squawkQueue.end())
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] not in squawk list. Adding it at the end", callsign), DebugLevel::Fpln);

			this->squawkQueue.push_back(callsign);
		}
		else if (it != this->squawkQueue.end() && it != this->squawkQueue.begin())
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] already in squawk list. Moving to front", callsign), DebugLevel::Fpln);

			this->squawkQueue.splice(this->squawkQueue.begin(), this->squawkQueue, it);
		}
	}
}

bool vsid::VSIDPlugin::atcFreqMatch(const EuroScopePlugIn::CController& other, const vsid::SectionAtc& local)
{
	if (other.GetPrimaryFrequency() != local.freq)
	{
		return false;
	}

	const std::string atcCallsign = other.GetCallsign();
	const std::string myCallsign = local.callsign;
	std::optional<std::string> atcIcao = std::nullopt;
	std::optional<std::string> myIcao = std::nullopt;

	try
	{
		atcIcao = vsid::utils::split(atcCallsign, '_').at(0);
		messageHandler->removeGenError(ERROR_ATC_ICAOSPLIT_FREQ_OTH + "_" + atcCallsign);
	}
	catch (std::out_of_range)
	{
		if (!messageHandler->genErrorsContains(ERROR_ATC_ICAOSPLIT_FREQ_OTH + "_" + atcCallsign))
		{
			vsid::Logger::log(LogLevel::Error, std::format("Failed to get ICAO part of other controller callsign [{}] in Freq match check. Code: {}", atcCallsign,
				ERROR_ATC_ICAOSPLIT_FREQ_OTH));

			messageHandler->addGenError(ERROR_ATC_ICAOSPLIT_FREQ_OTH + "_" + atcCallsign);
		}
	}

	try
	{
		myIcao = vsid::utils::split(myCallsign, '_').at(0);
		messageHandler->removeGenError(ERROR_ATC_ICAOSPLIT_FREQ_MY + "_" + myCallsign);
	}
	catch (std::out_of_range)
	{
		if (!messageHandler->genErrorsContains(ERROR_ATC_ICAOSPLIT_FREQ_MY + "_" + myCallsign))
		{
			vsid::Logger::log(LogLevel::Error, std::format("Failed to get ICAO part of my controller callsign [{}] in Freq match check. Code: {}", myCallsign,
				ERROR_ATC_ICAOSPLIT_FREQ_MY));

			messageHandler->addGenError(ERROR_ATC_ICAOSPLIT_FREQ_MY + "_" + myCallsign);
		}
	}

	if (atcIcao && myIcao && *atcIcao == *myIcao && other.GetFacility() == local.facility)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[FREQ Match] other: {}({}) IS local: {}({})", atcCallsign, other.GetPrimaryFrequency(),
			myCallsign, local.freq), DebugLevel::Atc);
		return true;
	}
	
	if (other.GetFacility() != local.facility) // #dev - debugging purpose
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[FREQ Match] other fac: {}({}) is NOT local fac: {}({})", atcCallsign, other.GetFacility(),
			myCallsign, local.facility), DebugLevel::Atc, true);

		return false;

	}
	
	return false; // default fallback state
}

std::optional<vsid::Command> vsid::VSIDPlugin::parseCommand(std::string_view commandLine)
{
	if (commandLine.empty()) return std::nullopt;
	if (!vsid::utils::startsWithCi(commandLine, ".vsid")) return std::nullopt;

	vsid::Command cmd;

	commandLine = commandLine.substr(std::string_view(".vsid").length());

	auto getNext = [&commandLine]() -> std::optional<std::string_view>
		{
			auto start = commandLine.find_first_not_of(' ');
			if (start == std::string_view::npos) return std::nullopt;

			auto end = commandLine.find(' ', start);
			auto next = commandLine.substr(start, end - start);

			commandLine = (end == std::string_view::npos) ? "" : commandLine.substr(end);

			return next;
		};

	auto command = getNext();
	if (!command) return std::nullopt;
	cmd.command = *command;

	while (auto param = getNext())
	{
		cmd.params.push_back(*param);
	}
	
	return cmd;
}

/*
* END OWN FUNCTIONS
*/

/*
* BEGIN ES FUNCTIONS
*/

EuroScopePlugIn::CRadarScreen* vsid::VSIDPlugin::OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated)
{
	this->screenId++;
	this->radarScreens.insert({ this->screenId, std::make_shared<vsid::Display>(this->screenId, this->shared, sDisplayName) });

	vsid::Logger::log(LogLevel::Debug, std::format("Screen created with id [{}]", this->screenId), DebugLevel::Menu);

	for (auto [id, screen] : this->radarScreens)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("radarScreens id [{}] | screen valid [{}]", id, (this->radarScreens.at(id) ? "TRUE" : "FALSE")),
			DebugLevel::Menu, true);
	}

	try
	{
		if (this->radarScreens.at(this->screenId)) return this->radarScreens.at(this->screenId)->getRadarScreen();
		else return nullptr;
	}
	catch (std::out_of_range)
	{
		vsid::Logger::log(LogLevel::Error, std::format("Failed to return Radar Screen with id [{}]. Code: {}", this->screenId, ERROR_DSP_SCREENCREATE));

		return nullptr;
	}

	return nullptr;
}

void vsid::VSIDPlugin::OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area) {
	
	// if logged in as observer disable functions

	if (!ControllerMyself().IsController()) return;

	EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelectASEL();
	std::string callsign = fpln.GetCallsign();

	if (!fpln.IsValid())
	{
		vsid::Logger::log(LogLevel::Error, std::format("Couldn't process flight plan as it was reported invalid (technical invalid) "
			"OnFunctionCall. Code: {}", ERROR_FPLN_INVALID));
		return;
	}

	EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
	const std::string ades = fplnData.GetDestination();
	const std::string& adep = fplnData.GetOrigin();

	auto& processed = FplnManager::getProcessed();


	/* DOCUMENTATION 
		//for (int i = 0; i < 8; i++) // test on how to get annotations
		//{
		//	std::string annotation = flightPlanASEL.GetControllerAssignedData().GetFlightStripAnnotation(i);
		//	vsid::messagehandler::LogMessage("Debug", "Annotation: " + annotation);
		//}

		delete FlightPlan;
	}*/

	// dynamically fill sid selection list with valid sids

	if(auto it = processed.find(callsign); it != processed.end())
	{
		auto& processedFpln = it->second;

		if (FunctionId == TAG_FUNC_VSID_SIDS_MAN)
		{
			std::string filedSidWpt = vsid::fpln::findSidWpt(fpln);
			std::map<std::string, vsid::Sid> validDepartures;
			std::string depRWY = vsid::fpln::getAtcBlock(fpln).second;
			const std::string& adep = fplnData.GetOrigin();

			if (!AirportManager::isActive(adep)) return;

			const auto aptData = AirportManager::getData(adep);

			if (aptData == nullptr)
			{
				vsid::Logger::log(
					LogLevel::Warning,
					std::format("[{}] failed to get airport [{}] in manual SID selection", callsign, adep)
				);

				return;
			}

			// deprwy is set and known

			if (!depRWY.empty() && processedFpln.atcRWY)
			{
				for (const vsid::Sid& sid : aptData->sids)
				{
					if ((sid.waypoint == filedSidWpt || sid.waypoint == "XXX" ||
						std::any_of(sid.transition.begin(), sid.transition.end(), [&](std::pair<std::string, vsid::Transition> trans)
							{
								return trans.first == filedSidWpt;
							})) && vsid::utils::contains(sid.rwys, depRWY))
					{
						validDepartures[sid.base + sid.number + sid.designator] = sid;
						if (aptData->enableRVSids)
						{
							validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", { depRWY });
						}
					}
					else if (filedSidWpt == "" && vsid::utils::contains(sid.rwys, depRWY))
					{
						validDepartures[sid.base + sid.number + sid.designator] = sid;
						if (aptData->enableRVSids)
						{
							validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", { depRWY });
						}
					}
				}
			}
			// deprwy is not set
			else if (depRWY.empty())
			{
				for (const vsid::Sid& sid : aptData->sids)
				{
					if (sid.waypoint == filedSidWpt || sid.waypoint == "XXX" ||
						std::any_of(sid.transition.begin(), sid.transition.end(), [&](std::pair<std::string, vsid::Transition> trans)
							{
								return trans.first == filedSidWpt;
							}))
					{
						for (const std::string& sidRwy : sid.rwys)
						{
							if (aptData->depRwys.contains(sidRwy)) validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							else if (!sid.area.empty() && aptData->areas.contains(sid.area) && aptData->areas.at(sid.area).arrAsDep && // #refactor - remove double lookup
								aptData->areas.at(sid.area).isActive &&
								aptData->areas.at(sid.area).inside(fpln.GetFPTrackPosition().GetPosition()))
							{
								validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							}
						}
					}
					else if (filedSidWpt.empty())
					{
						for (const std::string& sidRwy : sid.rwys)
						{
							if (aptData->depRwys.contains(sidRwy)) validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							else if (!sid.area.empty() && aptData->areas.contains(sid.area) && aptData->areas.at(sid.area).arrAsDep &&
								aptData->areas.at(sid.area).isActive &&
								aptData->areas.at(sid.area).inside(fpln.GetFPTrackPosition().GetPosition()))
							{
								validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							}
						}
					}
				}
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Select SID", 1);
				if (validDepartures.size() == 0)
				{
					this->AddPopupListElement("NO SID", "NO SID", TAG_FUNC_VSID_SIDS_MAN, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				}
				for (const auto& sid : validDepartures)
				{
					this->AddPopupListElement(sid.first.c_str(), sid.first.c_str(), TAG_FUNC_VSID_SIDS_MAN, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				}
				if (std::string(fplnData.GetPlanType()) == "V")
				{
					this->AddPopupListElement("VFR", "VFR", TAG_FUNC_VSID_SIDS_MAN, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, true);
				}
			}

			if (std::string(sItemString) == "VFR")
			{
				std::vector<std::string> filedRoute = vsid::fpln::clean(fpln, filedSidWpt);

				if (!depRWY.empty())
				{
					std::ostringstream ss;
					ss << fplnData.GetOrigin() << "/" << depRWY;
					filedRoute.insert(filedRoute.begin(), ss.str());
				}

				if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to change flight plan! Code: {}", callsign, ERROR_FPLN_SETROUTE));
				}

				if (!fplnData.AmendFlightPlan())
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND));
				}
			}
			else if (strlen(sItemString) != 0 && std::string(sItemString) != "NO SID")
			{
				if (depRWY.empty())
				{
					if (std::string(sItemString).find("-") != std::string::npos)
					{
						try
						{
							depRWY = vsid::utils::split(sItemString, '-').at(1);
						}
						catch (std::out_of_range)
						{
							vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to retrieve rwy from selected SID [{}]", callsign, sItemString));
						};
					}
					else
					{
						vsid::Logger::log(LogLevel::Error, std::format("[{}] couldn't retrieve rwy from selected SID (no rwy present in SID) [{}]", callsign, sItemString));
					}
				}

				if (!depRWY.empty())
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] Calling manual SID with rwy [{}]", callsign, depRWY), DebugLevel::Sid);

					FplnManager::processFlightplan(fpln, false, depRWY, validDepartures[sItemString]);
				}
				else
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] didn't receive runway for selected SID [{}] and didn't process. Code: {}",
						callsign, sItemString, ERROR_FPLN_SIDMAN_RWY));
				}
			}
			// FlightPlan->flightPlan.GetControllerAssignedData().SetFlightStripAnnotation(0, sItemString) // test on how to set annotations (-> exclusive per plugin)
		}

		if (FunctionId == TAG_FUNC_VSID_SIDS_AUTO)
		{
			if (!AirportManager::isActive(adep)) return;

			std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
			std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(fpln);

			if (std::string(fplnData.GetPlanType()) == "I")
			{
				// if a non standard SID is detected reset the SID to the standard SID
				if (atcBlock.first != "" && atcBlock.first != std::string(fplnData.GetOrigin()))
				{
					if (std::find(atcBlock.first.begin(), atcBlock.first.end(), 'x') != atcBlock.first.end() || // #refactor - remove checks for xX
						std::find(atcBlock.first.begin(), atcBlock.first.end(), 'X') != atcBlock.first.end())
					{
						atcBlock.first = vsid::fpln::splitTransition(atcBlock.first).first;
					}

					if (atcBlock.first != processedFpln.sid.name()) FplnManager::processFlightplan(fpln, false);
					else FplnManager::restoreIC(callsign);
				}
				// if only a rwy is detected set the SID based on that RWY
				else if (processedFpln.atcRWY && !atcBlock.second.empty())
				{
					FplnManager::processFlightplan(fpln, false, atcBlock.second);
				}
				// if nothing is detected set the default SID
				else FplnManager::processFlightplan(fpln, false);
			}
		}

		if (FunctionId == TAG_FUNC_VSID_TRANS)
		{
			std::map<std::string, vsid::Transition> validDepartures;
			const std::string filedSidWpt = vsid::fpln::findSidWpt(fpln);
			auto [blockSid, depRwy] = vsid::fpln::getAtcBlock(fpln);

			if (!AirportManager::isActive(adep)) return;

			const auto aptData = AirportManager::getData(adep);

			if (aptData == nullptr)
			{
				vsid::Logger::log(
					LogLevel::Warning,
					std::format("[{}] failed to get airport [{}] in transition selection menu", callsign, adep)
				);

				return;
			}

			if (!processedFpln.sid.empty() &&
				!processedFpln.sid.transition.empty() && processedFpln.customSid.empty())
			{
				for (auto& [base, trans] : processedFpln.sid.transition)
				{
					if (filedSidWpt != "" && filedSidWpt != base) continue;

					validDepartures[trans.base + trans.number + trans.designator] = trans;
				}
			}
			else if (!processedFpln.customSid.empty() &&
				!processedFpln.customSid.transition.empty())
			{
				for (auto& [base, trans] : processedFpln.customSid.transition)
				{
					if (filedSidWpt != "" && filedSidWpt != base) continue;

					validDepartures[trans.base + trans.number + trans.designator] = trans;
				}
			}
			else if (!blockSid.empty() && !depRwy.empty())
			{
				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fpln::splitTransition(blockSid).first;
				}

				for (const vsid::Sid& sid : aptData->sids)
				{
					if (blockSid == sid.name())
					{
						for (auto& [base, trans] : sid.transition)
						{
							if (!filedSidWpt.empty() && filedSidWpt != base) continue;

							validDepartures[trans.base + trans.number + trans.designator] = trans;
						}
					}
				}
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Select Trans", 1);

				if (blockSid == adep || blockSid.empty())
				{
					this->AddPopupListElement("SELECT SID", "SELECT SID", TAG_FUNC_VSID_TRANS, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				}
				else if (validDepartures.empty())
				{
					this->AddPopupListElement("NO TRANS", "NO TRANS", TAG_FUNC_VSID_TRANS, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				}
				else
				{
					for (const auto& sid : validDepartures)
					{
						this->AddPopupListElement(sid.first.c_str(), sid.first.c_str(), TAG_FUNC_VSID_TRANS, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
					}
				}
			}
			else if (strlen(sItemString) != 0 && std::string(sItemString) != "NO TRANS" && std::string(sItemString) != "SELECT SID")
			{
				std::vector<std::string> filedRoute = vsid::fpln::clean(fpln, filedSidWpt);
				std::string sid;

				if (!processedFpln.sid.empty() && processedFpln.customSid.empty())
					sid = processedFpln.sid.name();
				else if (!processedFpln.customSid.empty())
					sid = processedFpln.customSid.name();

				if (sid != "")
				{
					std::ostringstream ss;
					ss << sid << "x" << sItemString << "/" << depRwy;
					filedRoute.insert(filedRoute.begin(), ss.str());

					if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
					{
						vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to change flight plan! Code: {}", callsign, ERROR_FPLN_SETROUTE));
					}

					if (!fplnData.AmendFlightPlan())
					{
						vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to amend flight plan! Code: {}",callsign, ERROR_FPLN_AMEND));
					}
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_CLMBMENU)
		{
			if (!AirportManager::isActive(adep)) return;

			const auto aptData = AirportManager::getData(adep);

			if (aptData == nullptr)
			{
				vsid::Logger::log(
					LogLevel::Warning,
					std::format("[{}] failed to get airport [{}] in climb menu", callsign, adep)
				);

				return;
			}

			std::map<std::string, int> alt;

			// code order important! the pop up list may only be generated when no sItemString is present (if a button has been clicked)
			// if the list gets set up again while clicking a button wrong values might occur

			for (int i = aptData->maxInitialClimb; i >= vsid::utils::getMinClimb(aptData->elevation); i -= 500)
			{
				std::string menuElem = (i > aptData->transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
				alt[menuElem] = i;
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Select Climb", 1);
				for (int i = aptData->maxInitialClimb; i >= vsid::utils::getMinClimb(aptData->elevation); i -= 500)
				{
					std::string clmbElem = (i > aptData->transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
					this->AddPopupListElement(clmbElem.c_str(), clmbElem.c_str(), TAG_FUNC_VSID_CLMBMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				}

				if (alt.size() == 0)
					this->AddPopupListElement("NO MAX CLIMB", "NO MAX CLIMB", EuroScopePlugIn::TAG_ITEM_FUNCTION_NO, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
			}
			if (strlen(sItemString) != 0)
			{
				if (!fpln.GetControllerAssignedData().SetClearedAltitude(alt[sItemString]))
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to set cleared altitude. Code: {}", callsign, ERROR_FPLN_SETALT));
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_RWYMENU)
		{
			if (!AirportManager::isActive(adep)) return;

			const auto aptData = AirportManager::getData(adep);

			if (aptData == nullptr)
			{
				vsid::Logger::log(
					LogLevel::Warning,
					std::format("[{}] failed to get airport [{}] in runway selection menu", callsign, adep)
				);

				return;
			}

			std::vector<std::string> allRwys = aptData->allRwys;
			std::string rwy;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "RWY", 1);
				for (std::vector<std::string>::iterator it = allRwys.begin(); it != allRwys.end();)
				{
					rwy = *it;
					if (aptData->depRwys.contains(rwy) || aptData->arrRwys.contains(rwy))
					{
						this->AddPopupListElement(rwy.c_str(), rwy.c_str(), TAG_FUNC_VSID_RWYMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
						it = allRwys.erase(it);
					}
					else ++it;
				}

				if (!allRwys.empty())
				{
					for (std::vector<std::string>::iterator it = allRwys.begin(); it != allRwys.end(); ++it)
					{
						rwy = *it;
						this->AddPopupListElement(rwy.c_str(), rwy.c_str(), TAG_FUNC_VSID_RWYMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
					}
				}
			}

			if (strlen(sItemString) != 0)
			{
				std::vector<std::string> filedRoute = vsid::fpln::clean(fpln);
				std::ostringstream ss;
				ss << fplnData.GetOrigin() << "/" << sItemString;
				filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));

				// clear before changing the route - the fpln update triggered by SetRoute / Amend reprocesses the fpln

				if (aptData->settings.at("auto") &&
					std::string(fpln.GetFlightPlanData().GetPlanType()) == "I" &&
					(!processedFpln.sid.empty() || !processedFpln.customSid.empty())
					)
				{
					FplnManager::clearSidData(callsign);
				}

				if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to change flight plan! Code: {}", callsign, ERROR_FPLN_SETROUTE));
				}

				if (!vsid::fpln::findRemarks(fpln, "VSID/RWY"))
				{
					if (!vsid::fpln::addRemark(fpln, "VSID/RWY"))
					{
						vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to set remarks! Code: {}", callsign, ERROR_FPLN_REMARKSET));
					}
				}

				if (!fplnData.AmendFlightPlan())
				{
					vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND));
				}
				else
				{
					FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.atcRWY = true;
						});
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_REQMENU)
		{
			if (!AirportManager::isActive(adep))
			{
				vsid::Logger::log(LogLevel::Warning, std::format("[{}] Airport [{}] not active, can't process request!", callsign, adep));
				return;
			}

			const auto aptData = AirportManager::getData(adep);

			if (aptData == nullptr)
			{
				vsid::Logger::log(
					LogLevel::Warning,
					std::format("[{}] failed to get airport [{}] in request menu", callsign, adep)
				);

				return;
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "REQ", 1);

				this->AddPopupListElement("No Req", "No Req", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);

				this->AddPopupListElement("Clearance", "Clearance", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Startup", "Startup", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("RWY Startup", "RWY Startup", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Pushback", "Pushback", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Taxi", "Taxi", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Departure", "Departure", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				if (fpln.GetFlightPlanData().GetPlanType() == std::string("V"))
				{
					this->AddPopupListElement("VFR", "VFR", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				}
			}
			else if (strlen(sItemString) != 0)
			{
				std::string req = vsid::utils::tolower(sItemString);
				bool isRwyReq = req.find("rwy") != std::string::npos;

				if (isRwyReq)
				{
					try
					{
						req = vsid::utils::split(vsid::utils::tolower(sItemString), ' ').at(1);
					}
					catch (std::out_of_range&)
					{ 
						vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to split RWY from request [{}]", callsign, sItemString));
						return;
					}
				}
				
				bool isFplRwyReq = processedFpln.request.find("rwy") != std::string::npos;
				std::string newScratch = "";
				long long now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

				// check existing requests to preserve request times when switching from norm to rwy request and vice versa

				if (!isRwyReq && !isFplRwyReq && processedFpln.request == req) // all req other than rwq requests
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] no rwy req and stored req matches current req", callsign), vsid::DebugLevel::Req);

					if (aptData->requests.contains(req)) // #refactor - remove double lookup
					{
						for (auto& [reqCallsign, _] : aptData->requests.at(req))
						{
							if (reqCallsign == callsign)
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] already in request list [{}]", callsign, req), vsid::DebugLevel::Req);
								return;
							}
						}
					}
				}
				else if (isRwyReq && isFplRwyReq && processedFpln.request.find(req)) // both current and stored req are rwy req and a (partial) match
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] rwy req and stored req (partial) matches current req", callsign), vsid::DebugLevel::Req);

					if (aptData->rwyrequests.contains(req)) // #refactor - remove double lookup
					{
						for (auto& [reqRwy, fp] : aptData->rwyrequests.at(req))
						{
							for (auto& [reqCallsign, _] : fp)
							{
								if (reqCallsign == callsign)
								{
									vsid::Logger::log(LogLevel::Debug, std::format("[{}] already in request list [{}] for rwy [{}]", callsign, req, reqRwy),
										vsid::DebugLevel::Req);
									return;
								}
							}
						}
					}
				}
				else if (!isRwyReq && isFplRwyReq && processedFpln.request.find(req))
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] has rwy request and current non-rwy req (partial) matches", callsign), vsid::DebugLevel::Req);

					if (aptData->rwyrequests.contains(req)) // #refactor - remove double lookup
					{
						bool stop = false;
						for (auto& [reqRwy, _req] : aptData->rwyrequests.at(req))
						{
							for (auto& [reqCallsign, reqTime] : _req)
							{
								if (reqCallsign != callsign) continue;

								now = reqTime;
								stop = true;
								break;
							}

							if (stop) break;
						}
					}
				}
				else if (isRwyReq && !isFplRwyReq && processedFpln.request == req)
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] has no rwy req, but current req is a rwy req", callsign), vsid::DebugLevel::Req);

					if (aptData->requests.contains(req)) // #refactor - remove double lookup
					{
						for (auto& [reqCallsign, reqTime] : aptData->requests.at(req))
						{
							if (reqCallsign != callsign) continue;

							vsid::Logger::log(LogLevel::Debug, std::format("[{}] found in req list [{}]. Storing time.", callsign, req), vsid::DebugLevel::Req);

							now = reqTime;
							break;
						}
					}
				}

				if (req != "clearance" && vsid::fpln::getAtcBlock(fpln).second.empty())
				{
					vsid::Logger::log(LogLevel::Warning, std::format("[{}] no departure runway found in flight plan for request [{}]. Not setting the request!", callsign, req));
					return;
				}

				newScratch = ".vsid_req_" + std::string(sItemString) + "/" + std::to_string(now); // #refactor - now check for empty string needed

				if (!newScratch.empty()) SyncManager::add(callsign, newScratch, fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_CLR_SID)
		{
			if (!AirportManager::isActive(adep)) return;

			auto [atcSid, atcRwy] = vsid::fpln::getAtcBlock(fpln);

			this->callExtFunc(callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
				callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_SET_CLEARED_FLAG, POINT(), RECT());

			if (std::string(fpln.GetFlightPlanData().GetPlanType()) != "V" &&
				(atcSid.empty() || atcSid == adep))
			{
				FplnManager::processFlightplan(fpln, false, atcRwy);
			}
		}

		if (FunctionId == TAG_FUNC_VSID_CLR_SID_SU)
		{
			if (!AirportManager::isActive(adep)) return;

			auto [atcSid, atcRwy] = vsid::fpln::getAtcBlock(fpln);

			if (!fpln.GetClearenceFlag())
			{
				this->callExtFunc(callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
					callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_SET_CLEARED_FLAG, POINT(), RECT());
			}

			if (std::string(fpln.GetFlightPlanData().GetPlanType()) != "V" &&
				(atcSid.empty() || atcSid == adep))
			{
				FplnManager::processFlightplan(fpln, false, atcRwy);
			}

			if (!std::string(fpln.GetGroundState()).empty()) SyncManager::add(callsign, "STUP", fpln.GetControllerAssignedData().GetScratchPadString());
		}

		if (FunctionId == TAG_FUNC_VSID_INTS_SET)
		{
			if (!AirportManager::isActive(adep)) return;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Set Int", 1);

				std::string depRwy = vsid::fpln::getAtcBlock(fpln).second;
				const auto aptData = AirportManager::getData(adep);

				if (aptData == nullptr)
				{
					vsid::Logger::log(
						LogLevel::Warning,
						std::format("[{}] failed to get airport [{}] in intersection (set) menu", callsign, adep)
					);

					return;
				}

				if (depRwy == "") this->AddPopupListElement("NO RWY", "NO RWY", TAG_FUNC_VSID_INTS_SET, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				else if (aptData->intsec.contains(depRwy)) // #refactor - remove double lookup
				{
					for (const std::string& intsec : aptData->intsec.at(depRwy))
					{
						this->AddPopupListElement(intsec.substr(0, 3).c_str(), intsec.substr(0, 3).c_str(), TAG_FUNC_VSID_INTS_SET,
												 false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
					}
				}
				else this->AddPopupListElement("NO INTS", "NO INTS", TAG_FUNC_VSID_INTS_SET, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);

				this->AddPopupListElement("Custom", "Custom", TAG_FUNC_VSID_INTS_SET, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, true);
				this->AddPopupListElement("Clear", "Clear", TAG_FUNC_VSID_INTS_SET, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, true);
			}
			else
			{
				if (std::string(sItemString) == "Custom") this->OpenPopupEdit(Area, TAG_FUNC_VSID_INTS_SET, "");
				else if (std::string(sItemString) == "Clear") SyncManager::add(callsign, ".vsid_int_none_false", fpln.GetControllerAssignedData().GetScratchPadString());
				else if (std::string(sItemString) == "NO RWY") return;
				else if (std::string(sItemString) == "NO INTS") return;
				else SyncManager::add(callsign, ".vsid_int_" + std::string(sItemString).substr(0, 3) + "_true", fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_INTS_ABLE)
		{
			if (!AirportManager::isActive(adep)) return;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Able Int", 1);

				std::string depRwy = vsid::fpln::getAtcBlock(fpln).second;
				const auto aptData = AirportManager::getData(adep);

				if (aptData == nullptr)
				{
					vsid::Logger::log(
						LogLevel::Warning,
						std::format("[{}] failed to get airport [{}] in intersection (able) menu", callsign, adep)
					);

					return;
				}

				if (depRwy.empty()) this->AddPopupListElement("NO RWY", "NO RWY", TAG_FUNC_VSID_INTS_ABLE, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				else if (aptData->intsec.contains(depRwy)) // #refactor - remove double lookup
				{
					for (const std::string& intsec : aptData->intsec.at(depRwy))
					{
						this->AddPopupListElement(intsec.substr(0, 3).c_str(), intsec.substr(0, 3).c_str(), TAG_FUNC_VSID_INTS_ABLE,
												 false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
					}
				}
				else this->AddPopupListElement("NO INTS", "NO INTS", TAG_FUNC_VSID_INTS_ABLE, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);

				this->AddPopupListElement("Custom", "Custom", TAG_FUNC_VSID_INTS_ABLE, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, true);
				this->AddPopupListElement("Clear", "Clear", TAG_FUNC_VSID_INTS_ABLE, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, true);
			}
			else
			{
				if (std::string(sItemString) == "Custom") this->OpenPopupEdit(Area, TAG_FUNC_VSID_INTS_ABLE, "");
				else if (std::string(sItemString) == "Clear") SyncManager::add(callsign, ".VSID_INT_NONE_FALSE", fpln.GetControllerAssignedData().GetScratchPadString());
				else if (std::string(sItemString) == "NO RWY") return;
				else if (std::string(sItemString) == "NO INTS") return;
				else SyncManager::add(callsign, ".VSID_INT_" + std::string(sItemString).substr(0, 3) + "_FALSE", fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_TSSQUAWK)
		{
			if (this->topskyLoaded) this->addOrSetSquawk(callsign, true);
			else vsid::Logger::log(LogLevel::Error, "TopSky auto-assign squawk called, but TopSky was not detected.");
		}

		if (FunctionId == TAG_FUNC_VSID_HOV)
		{
			FplnManager::update(callsign, [&processedFpln](vsid::fpln::FplnData& data) // #evaluate - remove setting and only update if scratchpad was received
				{
					data.hov = !processedFpln.hov;
				});

			SyncManager::add(callsign, std::format(".VSID_HOV_{}", processedFpln.hov ? "TRUE" : "FALSE"),
				fpln.GetControllerAssignedData().GetScratchPadString());
		}
	}

	if (FunctionId == TAG_FUNC_VSID_CTL)
	{
		auto& processed = FplnManager::getProcessed();

		if (auto it = processed.find(callsign); it != processed.end())
		{
			FplnManager::update(callsign, [it](vsid::fpln::FplnData& data)
				{
					data.ctl = !it->second.ctl;
				});
		}
		else
		{
			FplnManager::add(callsign); // creation of flight plan of arriving traffic
			FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
				{
					data.ctl = true;
					data.sidProcessed = true; // prevent sid processing for arriving tfc
				});
		}

		std::string ctl = (FplnManager::getProcessed().at(callsign).ctl) ? "TRUE" : "FALSE";
		SyncManager::add(callsign, ".VSID_CTL_" + ctl, fpln.GetControllerAssignedData().GetScratchPadString());
	}

	if (FunctionId == TAG_FUNC_VSID_CTL_LOCAL)
	{
		auto& processed = FplnManager::getProcessed();

		if (auto it = processed.find(callsign); it != processed.end())
		{
			FplnManager::update(callsign, [it](vsid::fpln::FplnData& data)
				{
					data.ctlLocal = !it->second.ctlLocal;
				});
		}
		else
		{
			FplnManager::add(callsign); // creation of flight plan of arriving traffic
			FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
				{
					data.ctl = true;
					data.sidProcessed = true; // prevent sid processing for arriving tfc
				});
		}
	}
}

void vsid::VSIDPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	if (!FlightPlan.IsValid()) return;

	SyncManager::processQueue(this); // process sync queue on each tagItem update

	if (this->outOfVis(FlightPlan)) return;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string_view callsign = FlightPlan.GetCallsign();
	std::string_view adep = fplnData.GetOrigin(); // #continue - replace GetOrigin with adep below
	std::string_view ades = fplnData.GetDestination();

	auto& processed = FplnManager::getProcessed();
	auto processedIt = processed.find(callsign);
	auto [blockSid, blockRwy] = vsid::fpln::getAtcBlock(FlightPlan);

	if (AirportManager::isActive(adep))
	{
		const auto aptData = AirportManager::getData(adep);

		if (aptData == nullptr)
		{
			if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_ITEM_APT))
			{
				vsid::Logger::log(
					LogLevel::Warning,
					vsid::DebugLevel::Rwy,
					false,
					"[{}] failed to get airport [{}] in OnGetTagItem", callsign, adep
				);

				messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);
			}

			return;
		}
		messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);

		if (ItemCode == TAG_ITEM_VSID_SIDS) // processed fpln check inside to be able to evaluate unprocessed fplns
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

			if(processedIt != processed.end())
			{
				auto& processedFpln = processedIt->second;

				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fpln::splitTransition(blockSid).first;
				}

				const std::string& sidName = processedFpln.sid.name();
				const std::string& customSidName = processedFpln.customSid.name();

				// set sid item color and text

				const bool isIfr = std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "I";
				const bool isVfr = std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V";
				const bool bsEmpty = blockSid.empty();
				const bool hasCustomSid = !processedFpln.customSid.empty();
				const bool hasSid = !processedFpln.sid.empty();

				*pRGB = RGB(140, 140, 60); // failure color

				auto setNoSidText = [&]()
				{
					if (processedFpln.validEquip && processedFpln.sidWpt.empty()) strcpy_s(sItemString, 16, "MANUAL");
					else if (processedFpln.validEquip && !processedFpln.sidWpt.empty()) strcpy_s(sItemString, 16, processedFpln.sidWpt.c_str());
					else if (!processedFpln.validEquip) strcpy_s(sItemString, 16, "EQUIP");
				};

				if (isIfr)
				{
					if (!bsEmpty) // fpln carries an entry
					{
						if (blockSid == adep)
						{
							if (processedFpln.atcRWY && !blockRwy.empty())
							{
								if (hasCustomSid)
								{
									if (vsid::utils::contains(processedFpln.customSid.getRwys(), blockRwy))
									{
										*pRGB = this->configParser.getColor("customSidSuggestion"); // rwy matches stored custom sid
										strcpy_s(sItemString, 16, customSidName.c_str());
									}
									else
									{
										*pRGB = this->configParser.getColor("noSid"); // custom sid set but rwy doesn't match it
										setNoSidText();
									}
								}
								else if (!hasSid || !vsid::utils::contains(processedFpln.sid.getRwys(), blockRwy))
								{
									*pRGB = this->configParser.getColor("noSid"); // no custom sid and rwy doesn't match default sid
									setNoSidText();
								}
								else
								{
									*pRGB = this->configParser.getColor("sidSuggestion"); // sid match
									strcpy_s(sItemString, 16, sidName.c_str());
								}
							}
							else if (hasCustomSid)
							{
								*pRGB = this->configParser.getColor("customSidSuggestion");
								strcpy_s(sItemString, 16, customSidName.c_str());
							}
							else if (hasSid)
							{
								*pRGB = this->configParser.getColor("sidSuggestion");
								strcpy_s(sItemString, 16, sidName.c_str());
							}
							else
							{
								*pRGB = this->configParser.getColor("noSid"); // no atc rwy and no sid found
								setNoSidText();
							}
						}
						else if ((blockSid == sidName && processedFpln.sid.sidHighlight) ||
							(blockSid == customSidName && processedFpln.customSid.sidHighlight))
						{
							*pRGB = this->configParser.getColor("sidHighlight");
							strcpy_s(sItemString, 16, blockSid.c_str());
						}
						else if (blockSid == customSidName)
						{
							*pRGB = this->configParser.getColor("customSidSet");
							strcpy_s(sItemString, 16, blockSid.c_str());
						}
						else if (blockSid == sidName)
						{
							*pRGB = this->configParser.getColor("suggestedSidSet");
							strcpy_s(sItemString, 16, blockSid.c_str());
						}
						else
						{
							*pRGB = this->configParser.getColor("customSidSet"); // unkown value, e.g. old airac
							strcpy_s(sItemString, 16, blockSid.c_str());
						}
					}
					else // fpln carries NO entry
					{
						if (hasCustomSid)
						{
							*pRGB = this->configParser.getColor("customSidSuggestion");
							strcpy_s(sItemString, 16, customSidName.c_str());
						}
						else if (hasSid)
						{
							*pRGB = this->configParser.getColor("sidSuggestion");
							strcpy_s(sItemString, 16, sidName.c_str());
						}
						else
						{
							*pRGB = this->configParser.getColor("noSid"); // no sid found
							setNoSidText();
						}
					}
				}
				else if (isVfr)
				{
					*pRGB = this->configParser.getColor("sidSuggestion");
					strcpy_s(sItemString, 16, "VFR");
				}
				// neither IFR nor VFR here is a bug - *pRGB stays the RGB(140, 140, 60) failure color set above
			}
			else if (RadarTarget.GetGS() <= 50)
			{
				FplnManager::add(std::string(callsign)); // creation of new flight plan if adep is active

				bool checkOnly = !aptData->settings.at("auto");

				if (!checkOnly)
				{
					if (FlightPlan.GetClearenceFlag() ||
						std::string(fplnData.GetPlanType()) == "V")/* ||
						(atcBlock.first != "" && atcBlock.first != fplnData.GetOrigin())*/
					{
						checkOnly = true;
					}
					else if (!FlightPlan.GetClearenceFlag() && blockSid != adep && fplnData.IsAmended())
					{
						// prevent automode to use rwys set before
						blockRwy = "";
					}
				}
				if (!blockRwy.empty() &&
					(vsid::fpln::findRemarks(FlightPlan, "VSID/RWY") || blockSid != adep || fplnData.IsAmended()))
				{
					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Sid, false,
						"[{}] not yet processed, calling processFlightplan with atcRwy [{}] {}",
						callsign,
						blockRwy,
						(!checkOnly) ? "and setting the fpln." : "and only checking the fpln"
					);

					FplnManager::processFlightplan(FlightPlan, checkOnly, blockRwy);
				}
				else
				{
					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Sid, false,
						"[{}] not yet processed, calling processFlightplan without atcRwy {}",
						callsign,
						(!checkOnly) ? "and setting the fpln." : "and only checking the fpln"
					);

					FplnManager::processFlightplan(FlightPlan, checkOnly);
				}
			}
			
			// if the airborne aircraft has no SID set display the first waypoint of the route

			if (std::string(fplnData.GetPlanType()) != "V" && RadarTarget.GetGS() > 50 &&
				RadarTarget.GetPosition().GetPressureAltitude() >= aptData->elevation + 100)
			{
				std::vector<std::string> route = vsid::utils::split(fplnData.GetRoute(), ' ');

				if ((!blockSid.empty() && blockSid == adep) || blockSid.empty())
				{
					*pRGB = this->configParser.getColor("customSidSuggestion");
					bool validWpt = false;

					for (const std::string& wpt : route)
					{
						if (aptData->isSidWpt(wpt))
						{
							strcpy_s(sItemString, 16, wpt.c_str());
							validWpt = true;
							break;
						}
					}

					if (!validWpt) strcpy_s(sItemString, 16, "");
					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Sid, true, "[{}] airborne and atc.first [{}] is no SID", callsign, blockSid);
				}
				// processed flight plans are managed above - this is for already airborne flight plans after connecting

				else if (!FplnManager::getProcessed().contains(callsign) && !blockSid.empty() && blockSid != adep)
				{
					*pRGB = this->configParser.getColor("customSidSuggestion");
					strcpy_s(sItemString, 16, blockSid.c_str());
					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Sid, true, "[{}] airborne and unknown. SID [{}]", callsign, blockSid);
				}
			}
		}

		if (processedIt != processed.end())
		{
			auto& processedFpln = processedIt->second;

			const std::string& sidName = processedFpln.sid.name();
			const std::string& customSidName = processedFpln.customSid.name();

			if (ItemCode == TAG_ITEM_VSID_TRANS)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				std::string transition = "";

				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					transition = blockSid;
					blockSid = vsid::fpln::splitTransition(blockSid).first;
					transition.erase(transition.find(blockSid), blockSid.length());

					if (!transition.empty() && (transition.at(0) == 'X' || transition.at(0) == 'x')) transition.erase(0, 1);
				}

				if (blockSid.empty())
				{
					*pRGB = this->configParser.getColor("sidSuggestion");

					if (!processedFpln.transition.empty())
						strcpy_s(sItemString, 16, processedFpln.transition.c_str());
					else strcpy_s(sItemString, 16, "---");
				}
				else
				{
					if (blockSid == adep && !processedFpln.transition.empty() &&
						!processedFpln.customSid.empty() &&
						processedFpln.sid != processedFpln.customSid)
					{
						*pRGB = this->configParser.getColor("customSidSuggestion");
					}
					else if (!blockSid.empty() && !transition.empty() && processedFpln.transition == transition &&
						((blockSid == sidName && processedFpln.sid.sidHighlight) ||
							(blockSid == customSidName && processedFpln.customSid.sidHighlight)))
					{
						*pRGB = this->configParser.getColor("sidHighlight");
					}
					else if (!transition.empty() && processedFpln.transition == transition)
						*pRGB = this->configParser.getColor("suggestedSidSet");
					else if (!transition.empty() && processedFpln.transition != transition)
						*pRGB = this->configParser.getColor("customSidSet");
					/*else if (blockSid != adep && transition == "" && this->processed[callsign].transition != "")
						*pRGB = this->configParser.getColor("noSid");*/
					else *pRGB = this->configParser.getColor("sidSuggestion");

					if (transition != "") strcpy_s(sItemString, 16, transition.c_str());
					else if (transition.empty() && !processedFpln.transition.empty() &&
						((blockSid != adep && !processedFpln.sid.empty()) ||
							(blockSid == adep && !processedFpln.customSid.empty())))
					{
						strcpy_s(sItemString, 16, processedFpln.transition.c_str());
					}

					else strcpy_s(sItemString, 16, "---");
				}
			}

			if (ItemCode == TAG_ITEM_VSID_CLIMB)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				const int transAlt = aptData->transAlt;
				int tempAlt = 0;
				bool climbVia = false;

				const int finalAlt = FlightPlan.GetFinalAltitude();
				const int clearedAlt = FlightPlan.GetClearedAltitude();

				// determine if a found non-standard sid exists in valid sids and set it for ic comparison

				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fpln::splitTransition(blockSid).first;
				}


				// if an unknown Sid is set (non-standard or non-custom) try to find matching Sid in config

				if (!blockSid.empty() && blockSid != adep && blockSid != sidName && blockSid != customSidName)
				{
					for (const vsid::Sid& sid : aptData->sids)
					{
						if (blockSid == sid.name() && processedFpln.sid != sid)
						{
							(void)FplnManager::update(callsign, [&sid](vsid::fpln::FplnData& data)
								{
									data.customSid = sid;
								});

							break;
						}
					}
				}

				// determine if climb via is needed depending on customSid

				if ((blockSid.empty() || blockSid == sidName || blockSid == adep) && sidName != customSidName) // #evaluate code complexity
				{
					climbVia = processedFpln.sid.climbvia;
				}
				else if (blockSid.empty() || blockSid == customSidName || blockSid == adep)
				{
					climbVia = processedFpln.customSid.climbvia;
				}

				// determine initial climb depending on customSid

				bool rflBelowInitial = false; // additional check for suggestion coloring

				if (processedFpln.sid.initialClimb != 0 &&
					processedFpln.customSid.empty() &&
					(blockSid == sidName || blockSid.empty() || blockSid == adep)
					)
				{
					if (finalAlt < processedFpln.sid.initialClimb)
					{
						tempAlt = finalAlt;
						rflBelowInitial = true;
					}
					else tempAlt = processedFpln.sid.initialClimb;
				}
				else if (processedFpln.customSid.initialClimb != 0 &&
					(blockSid == customSidName || blockSid.empty() || blockSid == adep)
					)
				{
					if (finalAlt < processedFpln.customSid.initialClimb)
					{
						tempAlt = finalAlt;
						rflBelowInitial = true;
					}
					else tempAlt = processedFpln.customSid.initialClimb;
				}

				if (clearedAlt == finalAlt && !rflBelowInitial && tempAlt != finalAlt)
				{
					*pRGB = this->configParser.getColor("suggestedClmb"); // white
				}
				else if ((clearedAlt != finalAlt || tempAlt == processedFpln.sid.initialClimb ||
					tempAlt == processedFpln.customSid.initialClimb) &&
					clearedAlt == tempAlt
					)
				{
					if ((tempAlt == processedFpln.sid.initialClimb && processedFpln.sid.clmbHighlight) ||
						(tempAlt == processedFpln.customSid.initialClimb && processedFpln.customSid.clmbHighlight))
					{
						*pRGB = this->configParser.getColor("clmbHighlight");
					}
					else if (climbVia)
					{
						*pRGB = this->configParser.getColor("clmbViaSet"); // green
					}
					else
					{
						*pRGB = this->configParser.getColor("clmbSet"); // cyan
					}
				}
				else
				{
					*pRGB = this->configParser.getColor("customClmbSet"); // orange
				}

				// determine the initial climb depending on existing customSid

				if (clearedAlt == finalAlt)
				{
					if (tempAlt == 0)
					{
						strcpy_s(sItemString, 16, std::string("---").c_str());
					}
					else if (tempAlt <= transAlt)
					{
						strcpy_s(sItemString, 16, std::string("A").append(std::to_string(tempAlt / 100)).c_str());
					}
					else
					{
						if (tempAlt / 100 >= 100)
						{
							strcpy_s(sItemString, 16, std::to_string(tempAlt / 100).c_str());
						}
						else
						{
							strcpy_s(sItemString, 16, std::string("0").append(std::to_string(tempAlt / 100)).c_str());
						}
					}
				}
				else
				{
					if (clearedAlt == 0)
					{
						strcpy_s(sItemString, 16, std::string("---").c_str());
					}
					else if (clearedAlt <= transAlt)
					{
						strcpy_s(sItemString, 16, std::string("A").append(std::to_string(clearedAlt / 100)).c_str());
					}
					else
					{
						if (clearedAlt / 100 >= 100)
						{
							strcpy_s(sItemString, 16, std::to_string(clearedAlt / 100).c_str());
						}
						else
						{
							strcpy_s(sItemString, 16, std::string("0").append(std::to_string(clearedAlt / 100)).c_str());
						}
					}
				}
			}

			if (ItemCode == TAG_ITEM_VSID_RWY)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				if (blockSid == adep &&
					!processedFpln.atcRWY &&
					!processedFpln.remarkChecked
					)
				{
					(void)FplnManager::update(callsign, [FlightPlan](vsid::fpln::FplnData& data)
						{
							data.atcRWY = vsid::fpln::findRemarks(FlightPlan, "VSID/RWY");
							data.remarkChecked = true;
						});

					if (processedFpln.atcRWY)
					{
						vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Rwy, false, "[{}] accepted RWY because remarks are found.", callsign);
					}
				}
				else if (blockSid == adep && !processedFpln.atcRWY && fplnData.IsAmended())
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.atcRWY = true;
						});

					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Rwy, false, "[{}] accepted RWY because FPLN is amended and ICAO is found", callsign);
				}
				else if (!processedFpln.atcRWY &&
					!blockSid.empty() &&
					(blockSid == processedFpln.sid.name() ||
						blockSid == processedFpln.customSid.name())
					)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.atcRWY = true;
						});

					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Rwy, false,
						"[{}] accepted RWY because SID/RWY is found [{}/{}]. SID [{}] | Custom SID [{}]",
						callsign, blockSid, blockRwy, processedFpln.sid.name(), processedFpln.customSid.name());
				}
				else if (!processedFpln.atcRWY &&
					!blockSid.empty() &&
					blockSid != adep &&
					(fplnData.IsAmended() || FlightPlan.GetClearenceFlag())
					)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.atcRWY = true;
						});

					vsid::Logger::log(LogLevel::Debug, vsid::DebugLevel::Rwy, false,
						"[{}] accepted RWY because no ICAO is found and other than configured SID is found and {}",
						callsign, (fplnData.IsAmended()) ? " fpln is amended" : "", (FlightPlan.GetClearenceFlag() ? " clearance flag set" : ""));
				}

				if (!blockRwy.empty() &&
					aptData->depRwys.contains(blockRwy) &&
					processedFpln.atcRWY
					)
				{
					*pRGB = this->configParser.getColor("rwySet");
				}
				else if (!blockRwy.empty() &&
					!aptData->depRwys.contains(blockRwy) &&
					processedFpln.atcRWY
					)
				{
					*pRGB = this->configParser.getColor("notDepRwySet");
				}
				else *pRGB = this->configParser.getColor("rwyNotSet");

				if (!blockRwy.empty() && processedFpln.atcRWY)
				{
					strcpy_s(sItemString, 16, blockRwy.c_str());
				}
				else
				{
					std::string sidRwy;

					if (!processedFpln.sid.empty())
					{
						try // #checkforremoval
						{
							bool arrAsDep = false;
							const std::string& sidArea = processedFpln.sid.area;

							if (!sidArea.empty() && aptData->areas.contains(sidArea) && // #refactor - remove double lookups - if not removed, see above
								aptData->areas.at(sidArea).isActive &&
								aptData->areas.at(sidArea).inside(FlightPlan.GetFPTrackPosition().GetPosition()))
							{
								arrAsDep = aptData->areas.at(sidArea).arrAsDep;
							}

							for (const std::string& rwy : processedFpln.sid.rwys)
							{
								if (aptData->isDepRwy(rwy, arrAsDep))
								{
									sidRwy = rwy;
									break;
								}
							}

							messageHandler->removeFplnError(std::string(callsign), ERROR_CONF_RWYMENU);
						}
						catch (std::out_of_range)
						{
							if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_CONF_RWYMENU))
							{
								vsid::Logger::log(LogLevel::Error, vsid::DebugLevel::Rwy, false,
									"Failed to get RWY in the RWY menu. Check config [{}] for SID [{}]. RWY value is [{}]",
									adep, processedFpln.sid.idName(),
									vsid::utils::join(processedFpln.sid.rwys));

								messageHandler->addFplnError(std::string(callsign), ERROR_CONF_RWYMENU);
							}
						}
					}

					if (sidRwy.empty()) sidRwy = "---";
					strcpy_s(sItemString, 16, sidRwy.c_str());
				}
			}

			if (ItemCode == TAG_ITEM_VSID_REQ)
			{
				if (!processedFpln.request.empty())
				{

					std::string request = processedFpln.request;
					bool isFplRwyReq = request.find("rwy") != std::string::npos;

					if (isFplRwyReq)
					{
						try
						{
							request = vsid::utils::split(request, ' ').at(1);

							messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_REQSPLIT);
						}
						catch (std::out_of_range&)
						{
							if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_REQSPLIT))
							{
								vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to split stored request [{}] on tagItem update. Code: {}",
									callsign, request, ERROR_FPLN_REQSPLIT));

								messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_REQSPLIT);
							}
						}
					}

					// check rwy requests first

					if (isFplRwyReq && aptData->rwyrequests.contains(request)) // #refactor - remove double lookups
					{
						for (auto& [rwy, rwyreq] : aptData->rwyrequests.at(request))
						{
							for (auto it = rwyreq.begin(); it != rwyreq.end(); ++it)
							{
								if (it->first == callsign)
								{
									int pos = std::distance(it, rwyreq.end());
									std::string req = "R" + std::to_string(pos);
									strcpy_s(sItemString, 16, req.c_str());
									break;
								}
							}
						}
					}
					// check normal requests
					else if (!isFplRwyReq && aptData->requests.contains(request)) // #refactor - remove double lookups
					{
						for (auto it = aptData->requests.at(request).begin();
							it != aptData->requests.at(request).end(); ++it)
						{
							if (it->first == callsign)
							{
								int pos = std::distance(it, aptData->requests.at(request).end());
								std::string req = vsid::utils::toupper(request).at(0) + std::to_string(pos);
								strcpy_s(sItemString, 16, req.c_str());
								break;
							}
						}
					}
				}
			}

			if (ItemCode == TAG_ITEM_VSID_REQTIMER)
			{
				if (!processedFpln.request.empty())
				{
					*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
					long long now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

					std::string request = processedFpln.request;
					bool isFplRwyReq = request.find("rwy") != std::string::npos;

					if (isFplRwyReq)
					{
						try
						{
							request = vsid::utils::split(request, ' ').at(1);

							messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_REQSPLIT);
						}
						catch (std::out_of_range&)
						{
							if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_REQSPLIT))
							{
								vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to split stored request [{}] on tagItem update. Code: {}",
									callsign, request, ERROR_FPLN_REQSPLIT));

								messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_REQSPLIT);
							}
						}
					}

					// determine rwy request timer on rwy requests
					if (aptData->rwyrequests.contains(request)) // #refactor - remove double lookups
					{
						for (auto& [rwy, rwyReq] : aptData->rwyrequests.at(request))
						{
							for (auto& [reqCallsign, reqTime] : rwyReq)
							{
								if (reqCallsign != callsign) continue;

								int minutes = static_cast<int>((now - reqTime) / 60);

								if (minutes < this->configParser.getReqTime("caution")) *pRGB = this->configParser.getColor("requestNeutral");
								else if (minutes >= this->configParser.getReqTime("caution") &&
									minutes < this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestCaution");
								else if (minutes >= this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestWarning");

								strcpy_s(sItemString, 16, (std::to_string(minutes) + "m").c_str());
							}
						}
					}
					// determin normal request timer
					else if (aptData->requests.contains(request))  // #refactor -> remove double lookups
					{
						for (auto& [reqCallsign, reqTime] : aptData->requests.at(request))
						{
							if (reqCallsign != callsign) continue;

							int minutes = static_cast<int>((now - reqTime) / 60);

							if (minutes < this->configParser.getReqTime("caution")) *pRGB = this->configParser.getColor("requestNeutral");
							else if (minutes >= this->configParser.getReqTime("caution") &&
								minutes < this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestCaution");
							else if (minutes >= this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestWarning");

							strcpy_s(sItemString, 16, (std::to_string(minutes) + "m").c_str());
						}
					}
				}
			}

			if (ItemCode == TAG_ITEM_VSID_CLR)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				*pRGB = RGB(255, 255, 255);

				if (FlightPlan.GetClearenceFlag()) strcpy_s(sItemString, 16, "\xA4");
				else strcpy_s(sItemString, 16, "\xAC");
			}

			if (ItemCode == TAG_ITEM_VSID_INTS)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				if (processedFpln.intsec.second) *pRGB = this->configParser.getColor("intsecSet");
				else *pRGB = this->configParser.getColor("intsecAble");

				strcpy_s(sItemString, 16, processedFpln.intsec.first.c_str());
			}

			if (ItemCode == TAG_ITEM_VSID_HOVF)
			{
				if (aptData->autoHandoff) return;
				if (RadarTarget.GetGS() < 50) return;

				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				if (!processedFpln.hov)
				{
					if (RadarTarget.GetPosition().GetPressureAltitude() >= FlightPlan.GetClearedAltitude() - this->getConfigParser().hovWarningAlt)
					{
						*pRGB = this->configParser.getColor("hovWarning");
						strcpy_s(sItemString, 16, "HOV!");
					}
					else
					{
						*pRGB = this->configParser.getColor("hovNeutral");
						strcpy_s(sItemString, 16, "HOV");
					}
				}
			}
		}
	}

	if (ItemCode == TAG_ITEM_VSID_CTLF)
	{
		if (RadarTarget.GetGS() < 50) return;

		*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

		double dtg = FlightPlan.GetDistanceToDestination();
		int alt = RadarTarget.GetPosition().GetPressureAltitude();
		vsid::Clrf &clrf = this->configParser.getClrfMinimums();

		if (processedIt == processed.end())
		{
			if (std::string(fplnData.GetPlanType()) == "V" || !AirportManager::isActive(ades)) return;

			const auto aptData = AirportManager::getData(ades);

			if (aptData == nullptr)
			{
				if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_ITEM_APT))
				{
					vsid::Logger::log(
						LogLevel::Warning,
						vsid::DebugLevel::Rwy,
						false,
						"[{}] failed to get airport [{}] in OnGetTagItem (CTLF)", callsign, adep
					);

					messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);
				}

				return;
			}

			messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);

			if (dtg <= clrf.distWarning && alt <= aptData->elevation + clrf.altWarning)
			{
				*pRGB = this->configParser.getColor("clrfWarning");
				strcpy_s(sItemString, 16, "CLR!");
			}
			else if (dtg <= clrf.distCaution && alt <= aptData->elevation + clrf.altCaution)
			{				
				FplnManager::add(std::string(callsign)); // creation of flight plan of arriving traffic
				FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
					{
						data.ctl = true;
						data.sidProcessed = true; // prevent sid processing for arriving tfc
					});

				*pRGB = this->configParser.getColor("clrfCaution");
				strcpy_s(sItemString, 16, "CLR");
			}
		}
		else
		{
			auto& processedFpln = processedIt->second;
			const auto aptData = AirportManager::getData(ades);

			if (processedFpln.ctl) // ctl flag set - independent from active airports
			{
				if (processedFpln.ldgAlt == 0)
				{
					FplnManager::update(callsign, [alt](vsid::fpln::FplnData& data)
						{
							data.ldgAlt = alt;
						});
				}

				*pRGB = this->configParser.getColor("clrfSet");
				strcpy_s(sItemString, 16, "CTL");
			}

			if (processedFpln.mapp)
			{
				if (aptData != nullptr &&
					((alt > aptData->elevation + clrf.altCaution + 200) || alt > clrf.altCaution + 200))
				{
					FplnManager::remove(callsign);

					return;
				}
				
				if (aptData == nullptr) // remove mapp fplns - independent from active airports)
				{
					FplnManager::remove(callsign);

					return;
				}
			} 

			if(!AirportManager::isActive(ades)) return;

			if (aptData == nullptr)
			{
				if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_ITEM_APT))
				{
					vsid::Logger::log(
						LogLevel::Warning,
						vsid::DebugLevel::Rwy,
						false,
						"[{}] failed to get airport [{}] in OnGetTagItem (CTLF)", callsign, adep
					);

					messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);
				}

				return;
			}

			messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);
			
			if (!processedFpln.ctl)
			{
				if (std::string(fplnData.GetPlanType()) == "V") return;
				if (processedFpln.mapp) return;

				if (dtg <= clrf.distWarning && alt <= aptData->elevation + clrf.altWarning)
				{
					*pRGB = this->configParser.getColor("clrfWarning");
					strcpy_s(sItemString, 16, "CLR!");
				}
				else if (dtg <= clrf.distCaution && alt <= aptData->elevation + clrf.altCaution)
				{
					if (processedFpln.ldgAlt == 0)
					{
						(void)FplnManager::update(callsign, [alt](vsid::fpln::FplnData& data)
							{
								data.ldgAlt = alt;
							});
					}

					*pRGB = this->configParser.getColor("clrfCaution");
					strcpy_s(sItemString, 16, "CLR");
				}
			}
		}
	}

	if (ItemCode == TAG_ITEM_VSID_CTLF_LOCAL)
	{
		if (RadarTarget.GetGS() < 50) return;

		*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
		
		if (processedIt == processed.end())
		{
			if (std::string(fplnData.GetPlanType()) == "V") return;
		}
		else
		{
			auto& processedFpln = processedIt->second;

			// alt & clrf only for mapp calculation

			int alt = RadarTarget.GetPosition().GetPressureAltitude();
			vsid::Clrf& clrf = this->configParser.getClrfMinimums();

			if (processedFpln.mapp)
			{
				if (alt > clrf.altCaution + 200) // general alt check - airport check below after pointer validity
				{
					FplnManager::remove(callsign);
					return;
				}

				if (!AirportManager::isActive(ades)) return;

				const auto aptData = AirportManager::getData(ades);

				if (aptData == nullptr)
				{
					if (!messageHandler->getFplnErrors(std::string(callsign)).contains(ERROR_FPLN_ITEM_APT))
					{
						vsid::Logger::log(
							LogLevel::Warning,
							vsid::DebugLevel::Rwy,
							false,
							"[{}] failed to get airport [{}] in OnGetTagItem (CTLF LOCAL)", callsign, ades
						);

						messageHandler->addFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);
					}

					return;
				}
				messageHandler->removeFplnError(std::string(callsign), ERROR_FPLN_ITEM_APT);

				if ((alt > aptData->elevation + clrf.altCaution + 200))
				{
					FplnManager::remove(callsign);
					return;
				}
			}

			if (processedFpln.ctlLocal)
			{
				if (processedFpln.ldgAlt == 0)
				{
					(void)FplnManager::update(callsign, [alt](vsid::fpln::FplnData& data)
						{
							data.ldgAlt = alt;
						});
				}

				*pRGB = this->configParser.getColor("clrfSet");
				strcpy_s(sItemString, 16, "CTL");
			}
		}
	}

	if (ItemCode == TAG_ITEM_VSID_SQW)
	{
		if (AirportManager::isActive(adep))
		{
			if (auto it = std::find(this->squawkQueue.begin(), this->squawkQueue.end(), callsign); it != this->squawkQueue.end())
			{
				*pRGB = RGB(255, 255, 255);

				if (it == this->squawkQueue.begin()) strcpy_s(sItemString, 16, "NEXT");
				else strcpy_s(sItemString, 16, "STBY");

				return; // prevent displaying of old squawk until new is set
			}
		}
		
		std::string setSquawk = FlightPlan.GetFPTrackPosition().GetSquawk();
		std::string assignedSquawk = FlightPlan.GetControllerAssignedData().GetSquawk();

		if (setSquawk != assignedSquawk)
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
			*pRGB = this->configParser.getColor("squawkNotSet");
		}
		else
		{
			if (this->configParser.getColor("squawkSet") == RGB(300, 300, 300))
			{
				if (FlightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED)
				{
					*pColorCode = EuroScopePlugIn::TAG_COLOR_NON_CONCERNED;
				}
				else if (FlightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_NOTIFIED)
				{
					*pColorCode = EuroScopePlugIn::TAG_COLOR_NOTIFIED;
				}
				else if (FlightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_ASSUMED)
				{
					*pColorCode = EuroScopePlugIn::TAG_COLOR_ASSUMED;
				}
				else
				{
					*pColorCode = EuroScopePlugIn::TAG_COLOR_DEFAULT;
				}
			}
			else
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = this->configParser.getColor("squawkSet");
			}
		}

		if (assignedSquawk != "0000" && assignedSquawk != "1234") strcpy_s(sItemString, 16, assignedSquawk.c_str());
	}
}

bool vsid::VSIDPlugin::OnCompileCommand(const char* sCommandLine)
{
	std::vector<std::string> command = vsid::utils::split(sCommandLine, ' ');

	if (auto cmdOpt = this->parseCommand(sCommandLine); cmdOpt.has_value())
	{
		vsid::Command cmd = cmdOpt.value();

		vsid::Logger::log(LogLevel::Debug, std::format("Executing command: [{}] with parameters [{}]", cmd.command, vsid::utils::join(cmd.params)), DebugLevel::Cmd);

		/*if (!ControllerMyself().IsController())
		{
			vsid::Logger::log(LogLevel::Error, "Commands not available for observer");
			return true;
		}*/

		if (vsid::utils::svEqualCi(cmd.command, "help"))
		{
			vsid::Logger::log(LogLevel::Info, "Available commands: "
				"version / "
				"auto [icao] - activate automode for icao(s) - sets force mode if lower atc online / "
				"area [icao] [areaname] - toggle area(s) for icao / "
				"rule icao [rulename] - toggle rule(s) for icao or lists rules if no rule is specified / "
				"rule rulename - toggle rule for any active airport / "
				"night [icao] - toggle night mode for icao / "
				"lvp [icao] - toggle lvp ops for icao / "
				"req icao - lists request list entries / "
				"req icao reset [listname] - resets all request lists or specified list / "
				"reload [ese] - reloads the main config or the ese file / "
				"Debug - toggle debug mode");

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "version"))
		{
			vsid::Logger::log(LogLevel::Info, std::format("vSID Version {} loaded. Using nlohmann json ({}.{}.{}). Using libcurl ({})",
				pluginVersion,
				NLOHMANN_JSON_VERSION_MAJOR,
				NLOHMANN_JSON_VERSION_MINOR,
				NLOHMANN_JSON_VERSION_PATCH,
				curl_version()));

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "removed")) // debugging only
		{
			for (auto& elem : this->removeProcessed)
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] being removed at [{}] and is disconnected [{}]", elem.first,
					vsid::time::toFullString(elem.second.first), (elem.second.second) ? "YES" : "NO"));
			}
			if (this->removeProcessed.size() == 0)
			{
				vsid::Logger::log(LogLevel::Debug, "Removed list empty", vsid::DebugLevel::Dev);
			}

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "rule"))
		{
			if (cmd.params.empty()) // list all rules for active airports
			{
				for (const auto& [icao, airport] : AirportManager::getAirports())
				{
					if (!airport.customRules.empty())
					{
						std::string rules;

						for (const auto& [ruleName, isActive] : airport.customRules)
						{
							std::format_to(std::back_inserter(rules), "{}: {} ", ruleName, (isActive) ? "ON" : "OFF");
						}
						vsid::Logger::log(LogLevel::Info, std::format("[{}] Rules [{}]", icao, rules));
					}
					else vsid::Logger::log(LogLevel::Info, std::format("[{}] Rules: No rules configured", icao));
				}

				return true;
			}

			bool rulesChanged = false;

			if (cmd.params.size() == 1)
			{
				std::string_view param = cmd.params[0];

				// check if param is an ICAO and in the active airport list
				if (AirportManager::isActive(param))
				{
					const auto aptData = AirportManager::getData(param);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format("[{}] failed to get airport [{}] when listing rules", param, param),
							vsid::DebugLevel::Rwy
						);

						return true;
					}

					if (!aptData->customRules.empty())
					{
						std::string rules;
						for (const auto& [ruleName, isActive] : aptData->customRules)
						{
							std::format_to(std::back_inserter(rules), "{}: {} ", ruleName, (isActive) ? "ON" : "OFF");
						}
						vsid::Logger::log(LogLevel::Info, std::format("[{}] Rules [{}]", param, rules));
					}
					else vsid::Logger::log(LogLevel::Info, std::format("[{}] Rules: No rules configured", param));

					return true;
				}

				// param was no ICAO, check for possible rule
				bool ruleFound = false;

				for (auto& [icao, airport] : AirportManager::getAirports())
				{
					if (airport.customRules.empty()) continue;

					if (auto it = airport.customRules.find(param); it != airport.customRules.end())
					{
						const bool updatedValue = !it->second;
						vsid::apt::AirportData::CustomRulesMap mutableRules = airport.customRules;
						mutableRules.at(it->first) = updatedValue;
						
						const bool updated = AirportManager::update(icao, [&mutableRules](vsid::apt::AirportData& data)
							{
								data.customRules = std::move(mutableRules);
							});

						if (updated)
						{
							rulesChanged = true;
							ruleFound = true;

							vsid::Logger::log(
								LogLevel::Info,
								std::format("[{}] Rule [{}] [{}]", icao, param, updatedValue ? "ON" : "OFF")
							);
						}
						else vsid::Logger::log(
							LogLevel::Warning,
							std::format("[{}] Rule [{}] failed to update", icao, param)
						);
					}
				}

				if (!ruleFound)
				{
					vsid::Logger::log(
						LogLevel::Info,
						std::format("Rule [{}] not found in any active airport.", param)
					);
					return true;
				}
			}

			// toggle / update rule(s) for a given airport
			if (cmd.params.size() >= 2)
			{
				std::string_view icao = cmd.params[0];

				if (AirportManager::isActive(icao))
				{
					const auto aptData = AirportManager::getData(icao);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format(
								"Failed to get airport [{}] when toggling rules [{}]",
								icao,
								vsid::utils::join(cmd.params | std::views::drop(1)))
						);

						return true;
					}

					for (size_t i = 1; i < cmd.params.size(); ++i)
					{
						std::string_view rule = cmd.params[i];

						if (auto it = aptData->customRules.find(rule); it != aptData->customRules.end())
						{
							const bool updatedValue = !it->second;
							auto mutableRules = aptData->customRules;
							mutableRules.at(it->first) = updatedValue;
							
							const bool updated = AirportManager::update(icao, [&mutableRules](vsid::apt::AirportData& data)
								{
									data.customRules = std::move(mutableRules);
								});

							if (updated)
							{
								rulesChanged = true;

								vsid::Logger::log(
									LogLevel::Info,
									std::format("[{}] Rule [{}] [{}]", icao, rule, updatedValue ? "ON" : "OFF")
								);
							}
							else vsid::Logger::log(
								LogLevel::Warning,
								std::format("[{}] Rule [{}] failed to update.", icao, rule)
							);
							
						}
						else
						{
							vsid::Logger::log(
								LogLevel::Info,
								std::format("[{}] [{}]: Rule is unknown", icao, rule)
							);
						}					
					}
				}
				else vsid::Logger::log(
					LogLevel::Info,
					std::format("[{}] not in active airports", icao)
				);
			}

			if (rulesChanged)
			{
				// cmd.params[0] is the ICAO as otherwise rulesChanged would be false

				vsid::Logger::log(
					LogLevel::Debug,
					std::format("[{}] Rechecking due to rule change.", cmd.params[0]),
					vsid::DebugLevel::Sid
				);

				FplnManager::reprocessAll();
			}
			
			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "lvp"))
		{
			if (cmd.params.empty()) // list LVP status for active airports
			{
				if (AirportManager::empty()) vsid::Logger::log(LogLevel::Info, "No active airports.");

				bool first = true;
				std::string lvpList;

				for (const auto& [icao, airport] : AirportManager::getAirports())
				{
					if (!first) lvpList += " | ";

					std::format_to(
						std::back_inserter(lvpList),
						"[{}] LVP [{}]", icao, airport.settings.at("lvp") ? "ON" : "OFF"
					);
					first = false;
				}
				vsid::Logger::log(LogLevel::Info, lvpList);
			}
			else // set lvp status for airports
			{
				bool lvpChanged = false;
				bool first = true;
				std::string lvpList;
				for (std::string_view param : cmd.params)
				{
					if(!first) lvpList += " | ";

					if (AirportManager::isActive(param))
					{
						const auto aptData = AirportManager::getData(param);

						if (aptData == nullptr)
						{
							vsid::Logger::log(
								LogLevel::Warning,
								std::format(
									"[{}] failed to get airport when toggling LVP",	param)
							);

							continue;
						}

						auto mutableSettings = aptData->settings;

						bool &lvpStatus = mutableSettings.at("lvp");
						lvpStatus = !lvpStatus;

						const bool updated = AirportManager::update(param, [&mutableSettings](vsid::apt::AirportData& data)
							{
								data.settings = std::move(mutableSettings);
							});

						if (updated)
						{
							lvpChanged = true;

							std::format_to(std::back_inserter(lvpList), "[{}] LVP [{}]", param, lvpStatus ? "ON" : "OFF");
						}
						else std::format_to(std::back_inserter(lvpList), "[{}] LVP failed to update", param);
					}
					else
						std::format_to(std::back_inserter(lvpList), "[{}] not in active airports", param);

					first = false;
				}

				vsid::Logger::log(LogLevel::Info, lvpList);

				if (lvpChanged) this->UpdateActiveAirports();
			}
			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "night") || vsid::utils::svEqualCi(cmd.command, "time"))
		{
			std::string timeList;
			bool first = true;

			if (cmd.params.empty())
			{
				if (AirportManager::empty()) vsid::Logger::log(LogLevel::Info, "No active airports.");

				for (const auto& [icao, airport] : AirportManager::getAirports())
				{
					if (!first) timeList += " | ";

					std::format_to(std::back_inserter(timeList), "[{}] Time [{}]", icao, airport.settings.at("time") ? "ON" : "OFF");
				}
				vsid::Logger::log(LogLevel::Info, timeList);
			}
			else
			{
				bool timeChanged = false;

				for (const auto& param : cmd.params)
				{
					if (!first) timeList += " | ";

					bool updatedValue = false;

					const bool updated = AirportManager::update(param, [&updatedValue](vsid::apt::AirportData& data)
						{
							auto& timeStatus = data.settings.at("time");
							timeStatus = !timeStatus;
							updatedValue = timeStatus;
						});

					if (updated)
					{
						timeChanged = true;

						std::format_to(std::back_inserter(timeList), "[{}] Time [{}]", param, updatedValue ? "ON" : "OFF");
					}

					first = false;
				}

				vsid::Logger::log(LogLevel::Info, timeList);

				if (timeChanged) this->UpdateActiveAirports();
			}
			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "auto"))
		{
			std::string atcSI = ControllerMyself().GetPositionId();
			std::string atcIcao;

			auto splitMyCallsign = vsid::utils::split(ControllerMyself().GetCallsign(), '_');

			if(!splitMyCallsign.empty())
				atcIcao = splitMyCallsign[0];
			else
				vsid::Logger::log(LogLevel::Error, "Failed to get own ATC ICAO for automode. Code: " + ERROR_CMD_ATCICAO);

			if (cmd.params.empty())
			{
				// string populating with apt states and delimiter for non-first entries
				bool first = true;
				std::string autoList;

				bool autoChanged = false;		

				if (AirportManager::empty()) vsid::Logger::log(LogLevel::Info, "No active airports.");

				for (auto& [icao, airport] : AirportManager::getAirports())
				{
					if (ControllerMyself().GetFacility() >= 2 && ControllerMyself().GetFacility() <= 4 && !vsid::utils::svEqualCi(atcIcao, icao))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping auto mode because own ATC ICAO does not match", icao), vsid::DebugLevel::Atc);
						continue;
					}

					if (ControllerMyself().GetFacility() > 4 && !airport.appSI.contains(atcSI) && !vsid::utils::svEqualCi(atcIcao, icao))
					{
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Skipping auto mode because own SI is not in apt appSI "
							"or own ATC ICAO does not match", icao), vsid::DebugLevel::Atc);
						continue;
					}

					auto mutableSettings = airport.settings;
					auto& autoStatus = mutableSettings.at("auto");

					if (!autoStatus && airport.controllers.empty())
					{
						autoStatus = true;
						autoChanged = true;

						if (!first) autoList += " | ";

						std::format_to(std::back_inserter(autoList), "[{}] Automode [{}]", icao, autoStatus ? "ON" : "OFF");
						first = false;
					}
					else if (!autoStatus && !airport.controllers.empty() && !airport.hasLowerAtc(ControllerMyself(), true))
					{
						autoStatus = true;
						autoChanged = true;

						if (!first) autoList += " | ";

						std::format_to(std::back_inserter(autoList), "[{}] Automode [{}]", icao, autoStatus ? "ON" : "OFF");
						first = false;
					}
					else if (!autoStatus)
					{
						vsid::Logger::log(LogLevel::Info, std::format("[{}] Cannot activate automode. Lower or same level controller online.", icao));

						std::string atcList;
						bool first = true;
						for (const auto& [atcCallsign, controller] : airport.controllers)
						{
							if (!first) atcList += " | ";
							atcList += std::format("{} ({})", atcCallsign, controller.si);
							first = false;
						}
						vsid::Logger::log(LogLevel::Debug, std::format("[{}] Own ATC Facility: {}. Own ATC ICAO: {}. "
							"Controllers at airport [{}]", icao, ControllerMyself().GetFacility(),
							atcIcao, atcList), vsid::DebugLevel::Atc);
					}

					// update via manager

					AirportManager::update(icao, [&mutableSettings](vsid::apt::AirportData& data)
						{
							data.settings = std::move(mutableSettings);
						});
				}

				if (autoChanged)
				{
					FplnManager::reprocessAll();

					vsid::Logger::log(LogLevel::Info, autoList);
				}
				else vsid::Logger::log(LogLevel::Info, "No new automode. Check .vsid auto status for active ones.");
			}
			else if (cmd.params.size() >= 1)
			{
				// string populating with apt states and delimiter for non-first entries
				bool first = true;
				std::string autoList;

				if (vsid::utils::svEqualCi(cmd.params[0], "status"))
				{
					bool autoActive = false;

					for (const auto& [icao, airport] : AirportManager::getAirports())
					{
						if (!first) autoList += " | ";
						std::format_to(std::back_inserter(autoList), "[{}] Automode [{}]", icao, airport.settings.at("auto") ? "ON" : "OFF");
						if (airport.settings.at("auto")) autoActive = true;

						first = false;
					}
					if (autoActive)
						vsid::Logger::log(LogLevel::Info, autoList);
					else
						vsid::Logger::log(LogLevel::Info, "No active automode.");

					return true;
				}

				if (vsid::utils::svEqualCi(cmd.params[0], "off"))
				{
					for (auto& [icao, airport] : AirportManager::getAirports())
					{
						auto mutableSettings = airport.settings;
						mutableSettings.at("auto") = false;

						AirportManager::update(icao, [&mutableSettings](vsid::apt::AirportData& data)
							{
								data.settings = std::move(mutableSettings);
							});
					}
					vsid::Logger::log(LogLevel::Info, "Automode OFF for all airports.");

					return true;
				}

				for (const auto& param : cmd.params)
				{
					if (AirportManager::isActive(param))
					{
						const auto aptData = AirportManager::getData(param);

						if (aptData == nullptr)
						{
							vsid::Logger::log(
								LogLevel::Warning,
								std::format(
									"[{}] failed to get airport when toggling automode", param)
							);

							continue;
						}

						int myFacility = ControllerMyself().GetFacility();

						if (myFacility >= 2 && myFacility <= 4 && !vsid::utils::svEqualCi(atcIcao, param))
						{
							vsid::Logger::log(
								LogLevel::Debug,
								std::format("[{}] Skipping (force) auto mode because own ATC ICAO does not match", param),
								vsid::DebugLevel::Atc
							);

							continue;
						}

						if (myFacility > 4 && !aptData->appSI.contains(atcSI) && !vsid::utils::svEqualCi(atcIcao, param))
						{
							vsid::Logger::log(
								LogLevel::Debug,
								std::format(
									"[{}] Skipping (force) auto mode because own SI is not in apt appSI or "
									"own ATC ICAO does not match",
									param),
								vsid::DebugLevel::Atc
							);

							continue;
						}

						auto mutableSettings = aptData->settings;
						auto& autoStatus = mutableSettings.at("auto");
						autoStatus = !autoStatus; // toggle automode

						if (!first) autoList += " | ";
						std::format_to(
							std::back_inserter(autoList),
							"[{}] automode [{}]", param, autoStatus ? "ON" : "OFF"
						);
						first = false;

						if (autoStatus)
						{
							// reset processed flight plans if they're not cleared or if the set rwy is not part of depRwys anymore

							for (auto& [callsign, fplnData] : FplnManager::getProcessed())
							{
								EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());
								const auto adep = FlightPlan.GetFlightPlanData().GetOrigin();

								if (vsid::utils::svEqualCi(param, adep) && !FlightPlan.GetClearenceFlag() && !fplnData.atcRWY)
								{
									FplnManager::clearSidData(callsign);
								}
							}
						}
						if (autoStatus && aptData->hasLowerAtc(ControllerMyself()))
						{
							(void)AirportManager::update(param, [](vsid::apt::AirportData& data)
								{
									data.forceAuto = true;
								});
						}
						else if (!autoStatus)
						{
							(void)AirportManager::update(param, [](vsid::apt::AirportData& data)
								{
									data.forceAuto = false;
								});
						}

						(void)AirportManager::update(param, [&mutableSettings](vsid::apt::AirportData& data)
							{
								data.settings = std::move(mutableSettings);
							});
					}
					else
					{
						std::format_to(
							std::back_inserter(autoList),
							"[{}] not in active airports. Cannot set automode", param
						);

						first = false;
					}
				}

				vsid::Logger::log(LogLevel::Info, autoList);
			}
		
			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "area"))
		{
			if (cmd.params.empty())
			{
				for (const auto& [icao, airport] : AirportManager::getAirports())
				{
					if (!airport.areas.empty())
					{
						std::string areaList;
						bool first = true;

						std::format_to(std::back_inserter(areaList), "[{}] Area ", icao);

						for (const auto& [name, area] : airport.areas)
						{
							if (!first) areaList += " | ";
							std::format_to(std::back_inserter(areaList), "[{}][{}]", name, area.isActive ? "ON" : "OFF");
							first = false;
						}

						vsid::Logger::log(LogLevel::Info, areaList);
					}
				}

				return true;
			}

			bool areaChanged = false;
			
			if (cmd.params.size() == 1) // list areas for airport
			{
				const auto& param = cmd.params[0];

				// check if param is an ICAO and in the active airport list
				if (AirportManager::isActive(param))
				{
					const auto aptData = AirportManager::getData(param);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format(
								"[{}] failed to get airport when listing areas", param)
						);

						return true;
					}

					if (!aptData->areas.empty())
					{
						std::string areaList;
						bool first = true;

						std::format_to(std::back_inserter(areaList), "[{}] Area ", param);

						for (const auto& [name, area] : aptData->areas)
						{
							if (!first) areaList += " | ";
							std::format_to(
								std::back_inserter(areaList),
								"[{}][{}]", name, area.isActive ? "ON" : "OFF"
							);
							first = false;
						}

						vsid::Logger::log(LogLevel::Info, areaList);
					}
					else vsid::Logger::log(LogLevel::Info, std::format("[{}] No areas configured.", param));

					return true;
				}

				// param was no ICAO, check for possible area
				bool areaFound = false;

				for (auto& [icao, airport] : AirportManager::getAirports())
				{
					if (airport.areas.empty()) continue;

					if (auto it = airport.areas.find(param); it != airport.areas.end())
					{
						auto mutableAreas = airport.areas;
						const bool updatedValue = it->second.isActive;
						mutableAreas.at(it->first).isActive = updatedValue;

						areaFound = true;
						const bool updated = AirportManager::update(icao, [&mutableAreas](vsid::apt::AirportData& data)
							{
								data.areas = std::move(mutableAreas);
							});

						if (updated)
						{
							areaChanged = true;

							vsid::Logger::log(
								LogLevel::Info,
								std::format("[{}] Area [{}][{}]", icao, it->first, updatedValue ? "ON" : "OFF")
							);
						}
						else vsid::Logger::log(
							LogLevel::Info,
							std::format("[{}] Area [{}] failed to update", icao, it->first)
						);
					}
				}

				if (!areaFound)
				{
					vsid::Logger::log(
						LogLevel::Info,
						std::format("Area [{}] not found in any active airports.", param)
					);

					return true;
				}
			}

			if (cmd.params.size() >= 2)
			{
				const auto& icao = cmd.params[0];

				if (AirportManager::isActive(icao))
				{
					const auto aptData = AirportManager::getData(icao);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format(
								"[{}] failed to get airport when toggling LVP list [{}]",
								icao,
								vsid::utils::join(cmd.params | std::views::drop(1))
							)
						);

						return true;
					}

					if (aptData->areas.empty())
					{
						vsid::Logger::log(
							LogLevel::Info,
							std::format("[{}] has no areas configured. Aborting processing.", icao)
						);

						return true;
					}

					if (vsid::utils::svEqualCi(cmd.params[1], "off"))
					{
						const bool updated = AirportManager::update(icao, [](vsid::apt::AirportData& data)
							{
								for (auto& [_, area] : data.areas)
								{
									area.isActive = false;
								}
							});

						if (updated) {
							vsid::Logger::log(
								LogLevel::Info,
								std::format("[{}] disabled all Areas.", icao)
							);
						}
						else
						{
							vsid::Logger::log(
								LogLevel::Error,
								std::format("[{}] failed to disable all Areas.", icao)
							);
						}
						
						areaChanged = true;
					}
					else
					{
						for (size_t i = 1; i < cmd.params.size(); ++i)
						{
							const bool updated = AirportManager::update(
								icao,
								[&icao, &cmd, i, &areaChanged](vsid::apt::AirportData& data)
								{
									if (auto jt = data.areas.find(cmd.params[i]); jt != data.areas.end())
									{
										jt->second.isActive = !jt->second.isActive;

										vsid::Logger::log(
											LogLevel::Info,
											std::format(
												"[{}] Area [{}][{}]",
												icao,
												jt->first,
												jt->second.isActive ? "ON" : "OFF"
											)
										);

										areaChanged = true;
									}
									else
									{
										vsid::Logger::log(
											LogLevel::Info,
											std::format("[{}] [{}]: Area  is unknown.", icao, cmd.params[i])
										);
									}
								}
							);
						}
					}	
				}
				else
				{
					vsid::Logger::log(
						LogLevel::Info,
						std::format("[{}] not in active airports", icao)
					);
				}
			}

			if (areaChanged)
			{
				// cmd.params[0] is the ICAO as otherwise rulesChanged would be false

				vsid::Logger::log(
					LogLevel::Debug,
					std::format("[{}] Rechecking due to area change.", cmd.params[0]),
					vsid::DebugLevel::Sid
				);

				FplnManager::reprocessAll();
			}

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "sync"))
		{
			vsid::Logger::log(LogLevel::Debug, "Syncing all requests.", vsid::DebugLevel::Sync);

			// #dev - temporary info who synced
			std::string syncMyCallsign = std::format(".vsid_syncby_{}", ControllerMyself().GetCallsign());
			// end dev

			for (const auto& [callsign, fpln] : FplnManager::getProcessed())
			{
				EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());
				if (!FlightPlan.IsValid()) continue;

				vsid::Logger::log(LogLevel::Debug, std::format("[{}] syncing...", callsign), vsid::DebugLevel::Sync);

				std::string_view adep = FlightPlan.GetFlightPlanData().GetOrigin();
				std::string_view ades = FlightPlan.GetFlightPlanData().GetDestination();
				std::string oldScratchPad = FlightPlan.GetControllerAssignedData().GetScratchPadString();

				// #dev - temporary info who synced	
				FlightPlan.GetControllerAssignedData().SetScratchPadString(syncMyCallsign.c_str());
				FlightPlan.GetControllerAssignedData().SetScratchPadString(oldScratchPad.c_str());
				// end dev

				// sync requests

				SyncManager::syncReq(FlightPlan);	

				if (AirportManager::isActive(adep))
				{
					// sync states
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling sync state.", callsign), vsid::DebugLevel::Sync);
					SyncManager::syncStates(FlightPlan);

					// sync intersections
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] syncing intersection.", callsign), vsid::DebugLevel::Sync);

					if (std::string_view intersection = fpln.intsec.first; !intersection.empty())
					{
						SyncManager::add(callsign,
							std::format(".VSID_INT_{}_{}", intersection, fpln.intsec.second ? "TRUE" : "FALSE"),
							oldScratchPad);
					}
				}

				// sync cleared to land flag

				if (AirportManager::isActive(ades))
				{
					vsid::Logger::log(LogLevel::Debug, std::format("[{}] syncing ctlf.", callsign), vsid::DebugLevel::Sync);

					SyncManager::add(callsign, std::format(".VSID_CTL_{}", fpln.ctl ? "TRUE" : "FALSE"), oldScratchPad);
				}
			}
			return true;
		} 

		if (vsid::utils::svEqualCi(cmd.command, "req"))
		{
			if (cmd.params.empty())
			{
				vsid::Logger::log(LogLevel::Info, "ICAO is missing for request command");

				return false;
			}

			if (cmd.params.size() == 1)
			{
				std::string_view icao = cmd.params[0];

				if (AirportManager::isActive(icao))
				{
					const auto aptData = AirportManager::getData(icao);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format("[{}] failed to get airport when listing requests", icao)
						);

						return true;
					}

					std::string reqList;
					std::format_to(std::back_inserter(reqList), "[{}] ", icao);

					for (const auto& [reqType, reqInfo] : aptData->requests)
					{
						if (reqInfo.empty())
						{
							std::format_to(std::back_inserter(reqList), "[{}] no requests. ", reqType);

							continue;
						}

						bool first = true;

						std::format_to(std::back_inserter(reqList), "[{}]: ", reqType);

						for (const auto& [callsign, _] : reqInfo)
						{
							if (!first) reqList += " | ";
							
							reqList += callsign;
							first = false;
						}

						reqList += " ";
					}

					vsid::Logger::log(LogLevel::Info, reqList);

					if (aptData->rwyrequests.empty())
					{
						vsid::Logger::log(LogLevel::Info, std::format("[{}] no rwy requests.", icao));
						return true;
					}

					vsid::Logger::log(
						LogLevel::Debug,
						std::format("[{}] rwyrequsts.size() {}", icao, aptData->rwyrequests.size()),
						DebugLevel::Dev,
						true
					);

					reqList.clear();
					std::format_to(std::back_inserter(reqList), "[{}] Runways ", icao);

					for (const auto& [reqType, reqRwy] : aptData->rwyrequests)
					{	
						if (reqRwy.empty())
						{
							std::format_to(std::back_inserter(reqList), "[{}] no requests. ", reqType);

							continue;
						}					

						for (const auto& [rwy, reqInfo] : reqRwy)
						{
							if (reqInfo.empty())
							{
								std::format_to(std::back_inserter(reqList), "[{}][{}] no requests. ", reqType, rwy);

								continue;
							}

							bool first = true;

							std::format_to(std::back_inserter(reqList), "[{}][{}]: ", reqType, rwy);

							for (const auto& [callsign, _] : reqInfo)
							{
								if (!first) reqList += " | ";
								reqList += callsign;
								first = false;
							}

							reqList += " ";
						}
					}

					vsid::Logger::log(LogLevel::Info, reqList);
				}
				else
				{
					vsid::Logger::log(
						LogLevel::Warning,
						std::format("[{}] not in active airports. Cannot check for requests", icao)
					);
				}

				return true;
			}

			if (cmd.params.size() == 2) // reset all req lists for a given apt
			{
				if (!vsid::utils::svEqualCi(cmd.params[1], "reset")) return false;

				std::string_view icao = cmd.params[0];

				if (AirportManager::isActive(icao))
				{
					const auto aptData = AirportManager::getData(icao);

					if (aptData == nullptr)
					{
						vsid::Logger::log(
							LogLevel::Warning,
							std::format(
								"[{}] failed to get airport when resetting request lists", icao)
						);

						return true;
					}

					std::string reqClearList;
					bool first = true;

					std::format_to(std::back_inserter(reqClearList), "[{}] ", icao);

					const bool updatedReq = AirportManager::update(icao, [&reqClearList, &first](vsid::apt::AirportData& data)
						{
							for (auto& [reqType, reqList] : data.requests)
							{
								reqList.clear();

								if (!first) reqClearList += " | ";

								if (reqList.empty())
									std::format_to(std::back_inserter(reqClearList), "[{}][Cleared]", reqType);
								else
									std::format_to(std::back_inserter(reqClearList), "[{}][NOT Cleared]", reqType);

								first = false;
							}
						});

					if (updatedReq) vsid::Logger::log(LogLevel::Info, reqClearList);
					else vsid::Logger::log(LogLevel::Warning, std::format("[{}] couldn't reset all request lists.", icao));

					reqClearList.clear();
					first = true;

					std::format_to(std::back_inserter(reqClearList), "[{}] Runways ", icao);

					const bool updatedRwyReq = AirportManager::update(icao, [&reqClearList, &first](vsid::apt::AirportData& data)
						{
							for (auto& [reqType, rwyReq] : data.rwyrequests)
							{
								rwyReq.clear();

								if (!first) reqClearList += " | ";

								if (rwyReq.empty())
									std::format_to(std::back_inserter(reqClearList), "[{}][Cleared]", reqType);
								else
									std::format_to(std::back_inserter(reqClearList), "[{}][NOT Cleared]", reqType);

								first = false;
							}
						});

					if (updatedRwyReq) vsid::Logger::log(LogLevel::Info, reqClearList);
					else vsid::Logger::log(LogLevel::Warning, std::format("[{}] couldn't reset all rwy request lists.", icao));
				}

				return true;
			}

			if (cmd.params.size() == 3)
			{
				if (!vsid::utils::svEqualCi(cmd.params[1], "reset")) return false;

				std::string_view icao = cmd.params[0];
				std::string reqClearList;

				if (AirportManager::isActive(icao))
				{
					std::string_view req = cmd.params[2];

					std::format_to(std::back_inserter(reqClearList), "[{}] Requests ", icao);

					const bool updatedReq = AirportManager::update(icao, [&req, &reqClearList](vsid::apt::AirportData& data)
						{
							if (auto it = data.requests.find(req); it != data.requests.end())
							{
								it->second.clear();

								if (it->second.empty())
									std::format_to(std::back_inserter(reqClearList), "[{}][Cleared]", req);
								else
									std::format_to(std::back_inserter(reqClearList), "[{}][NOT Cleared]", req);
							}
							else
								std::format_to(std::back_inserter(reqClearList), "[{}] not in request list.", req);
						});

					if (updatedReq) vsid::Logger::log(LogLevel::Info, reqClearList);
					else vsid::Logger::log(LogLevel::Warning, std::format("[{}] couldn't reset given request list.", icao));

					reqClearList.clear();
					std::format_to(std::back_inserter(reqClearList), "[{}] Runways ", icao);

					const bool updatedRwyReq = AirportManager::update(icao, [&req, &reqClearList](vsid::apt::AirportData& data)
						{
							if (auto it = data.rwyrequests.find(req); it != data.rwyrequests.end())
							{
								it->second.clear();

								if (it->second.empty())
									std::format_to(std::back_inserter(reqClearList), "[{}][Cleared]", req);
								else
									std::format_to(std::back_inserter(reqClearList), "[{}][NOT Cleared]", req);
							}
							else
								std::format_to(std::back_inserter(reqClearList), "[{}] not in request list.", req);
						});

					if (updatedRwyReq) vsid::Logger::log(LogLevel::Info, reqClearList);
					else vsid::Logger::log(LogLevel::Warning, std::format("[{}] couldn't reset given rwy request list.", icao));
								
				}

				return true;
			}

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "debug")) 
		{
			if (cmd.params.empty())
			{
				if (!vsid::Logger::getConsoleState())
				{
					vsid::Logger::toggleDebugLevel({ "all" });
					vsid::Logger::enableConsole();
				}
				else				
					vsid::Logger::disableConsole();
			}
			else if (cmd.params[0] == "devonly")
			{
				vsid::Logger::setLogDevOnly(!vsid::Logger::getLogDevOnly());
				vsid::Logger::log(LogLevel::Info, std::format("Development messages logging: [{}]", (vsid::Logger::getLogDevOnly()) ? "ON" : "OFF"));

				return true;
			}
			else if (cmd.params[0] != "status")
			{
				vsid::Logger::toggleDebugLevel(cmd.params);

				vsid::Logger::enableConsole();
			}

			if(vsid::Logger::getConsoleState()) vsid::Logger::log(LogLevel::Info, std::format("DEBUG area active: [{}]", vsid::Logger::getDebugLevelString()));
			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "log"))
		{
			if (!cmd.params.empty())
			{
				if (vsid::utils::svEqualCi(cmd.params[0], "dev"))
					vsid::Logger::setLogDevOnly(!vsid::Logger::getLogDevOnly());

				vsid::Logger::log(LogLevel::Info, std::format("Log development messages [{}]", (vsid::Logger::getLogDevOnly()) ? "ON" : "OFF"));
			}
			else
			{
				vsid::Logger::log(LogLevel::Error, "Missing additional command parameter.");
				return false;
			}

			return true;
		}

		if (vsid::utils::svEqualCi(cmd.command, "reload"))
		{
			if (cmd.params.empty())
			{
				vsid::Logger::log(LogLevel::Info, "Reloading main config...");
				this->configParser.loadMainConfig();

				return true;
			}
			else
			{
				if(vsid::utils::svEqualCi(cmd.params[0], "ese"))
				{
					this->loadEse();

					return true;
				}
			}
			return false;
		}

		if (vsid::utils::svEqualCi(cmd.command, "ghversion"))
		{
			if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
			{
				vsid::Logger::log(LogLevel::Error, "Failed to init curl_global");
				return true;
			}

			vsid::version::checkForUpdates(this->getConfigParser().notifyUpdate, vsid::version::parseSemVer(pluginVersion));

			curl_global_cleanup();

			vsid::Logger::log(LogLevel::Info, "Version check completed. Check log for details if interested "
				"and there is no update notification.");

			return true;
		}

		vsid::Logger::log(LogLevel::Info, std::format("Unknown command [{}].", cmd.command));
		return false;
	}

	return true;
}

void vsid::VSIDPlugin::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	if (!FlightPlan.IsValid()) return;
	if (this->outOfVis(FlightPlan)) return;

	// if we receive updates for flight plans validate entered sid again to sync between controllers
	// updates are received for any flight plan not just that under control

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();
	const std::string adep = fplnData.GetOrigin();

	if (!FplnManager::contains(callsign)) // #dev - debugging msg
	{
		vsid::Logger::log(LogLevel::Debug, DebugLevel::Fpln, true, "[{}] flight plan not processed. Skipping update", callsign);

		return;
	}

	const auto processedFpln = FplnManager::getData(callsign);
	if (processedFpln == nullptr) return;

	vsid::Logger::log(
		LogLevel::Debug,
		std::format("[{}] flight plan updated", callsign),
		vsid::DebugLevel::Fpln,
		true
	);

	std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');

	if (filedRoute.empty()) return;

	auto [blockSid, blockRwy] = vsid::fpln::getAtcBlock(FlightPlan);

	if (!processedFpln->atcRWY && !blockRwy.empty() &&
		(fplnData.IsAmended() || FlightPlan.GetClearenceFlag() ||
		blockSid != adep ||
		(blockSid == adep && vsid::fpln::findRemarks(FlightPlan, "VSID/RWY")))
		)
	{
		(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
			{
				data.atcRWY = true;
			});
	}

	if (vsid::fpln::findRemarks(FlightPlan, "VSID/RWY") &&
		blockSid != adep &&
		ControllerMyself().IsController()
		)
	{
		if (!vsid::fpln::removeRemark(FlightPlan, "VSID/RWY"))
		{
			if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_REMARKRMV))
			{
				vsid::Logger::log(
					LogLevel::Error,
					std::format("[{}] - Failed to remove remarks! Code: {}", callsign, ERROR_FPLN_REMARKRMV)
				);

				messageHandler->addFplnError(callsign, ERROR_FPLN_REMARKRMV);
			}
		}
		else messageHandler->removeFplnError(callsign, ERROR_FPLN_REMARKRMV);

		if (!fplnData.AmendFlightPlan())
		{
			if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
			{
				vsid::Logger::log(
					LogLevel::Error,
					std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND)
				);

				messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
			}
		}
		else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
	}

	if (const auto aptData = AirportManager::getData(adep); aptData != nullptr) // #monitor - major changes
	{
		if (blockSid == adep && !blockRwy.empty())
		{
			// update possible rwy requests

			if (!processedFpln->request.empty())
			{
				std::string fplnRwy = vsid::fpln::getAtcBlock(FlightPlan).second;

				// update rwy requests directly if a rwy request is stored for the flight plan

				if (processedFpln->request.find("rwy") != std::string::npos &&
					!fplnRwy.empty() && vsid::utils::contains(aptData->allRwys, fplnRwy))
				{
					std::string normReq = vsid::utils::split(processedFpln->request, ' ').at(1);

					(void)AirportManager::update(adep, [&normReq, &callsign, &fplnRwy](vsid::apt::AirportData& data)
						{
							if (auto it = data.rwyrequests.find(normReq); it != data.rwyrequests.end())
							{
								bool stop = false;

								for (auto& [rwy, rwyReq] : it->second)
								{
									for (auto jt = rwyReq.begin(); jt != rwyReq.end();)
									{
										if (jt->first != callsign)
										{
											++jt;
											continue;
										}

										if (rwy != fplnRwy)
										{
											data.rwyrequests[normReq][fplnRwy].insert({ callsign, jt->second });
											rwyReq.erase(jt);

											stop = true;
											break;
										}

										++jt;
									}
									if (stop) break;
								}
							}
						});
				}
				// check if a rwy request is available for stored non-rwy request and update the rwy
				else if (!fplnRwy.empty() && vsid::utils::contains(aptData->allRwys, fplnRwy))
				{
					(void)AirportManager::update(adep, [&processedFpln, &callsign, &fplnRwy](vsid::apt::AirportData& data)
						{
							bool stop = false;

							for (auto& [type, rwys] : data.rwyrequests)
							{
								if (processedFpln->request.find(type) == std::string::npos) continue;

								for (auto& [rwy, rwyReq] : rwys)
								{
									for (auto it = rwyReq.begin(); it != rwyReq.end();)
									{
										if (it->first != callsign)
										{
											++it;
											continue;
										}

										if (rwy != fplnRwy)
										{
											data.rwyrequests[type][fplnRwy].insert({ callsign, it->second });
											rwyReq.erase(it);

											stop = true;
											break;
										}

										++it;
									}
									if (stop) break;
								}
								if (stop) break;
							}
						});	
				}
			}

			vsid::Logger::log(
				LogLevel::Debug,
				vsid::DebugLevel::Sid,
				false,
				"[{}] fpln updated, calling processFlightplan with atcRwy : {}",
				callsign,
				blockRwy
			);

			FplnManager::reprocess(callsign);
		}
		else if (blockSid != adep && !blockRwy.empty())
		{
			/*vsid::Sid atcSid;
			for (const vsid::Sid& sid : aptData->sids)
			{
				if (atcBlock.first.find_first_of("0123456789") != std::string::npos)
				{
					if (sid.base != atcBlock.first.substr(0, atcBlock.first.length() - 2)) continue;
					if (sid.designator != std::string(1, atcBlock.first[atcBlock.first.length() - 1])) continue;
				}
				else
				{
					if (sid.base != atcBlock.first) continue;
				}
				atcSid = sid;
			}*/

			vsid::Logger::log(
				LogLevel::Debug,
				vsid::DebugLevel::Sid,
				false,
				"[{}] fpln updated, calling processFlightplan with atcRwy : {} and atcSid : {}",
				callsign,
				blockRwy,
				blockSid
			);

			FplnManager::reprocess(callsign);
			
			//FplnManager::processFlightplan(FlightPlan, true, atcBlock.second, atcSid);
		}
		else
		{
			vsid::Logger::log(
				LogLevel::Debug,
				std::format("[{}] fpln updated, calling processFlightplan without atcRwy", callsign),
				vsid::DebugLevel::Sid);

			FplnManager::reprocess(callsign);
		}
	}
}

void vsid::VSIDPlugin::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType)
{
	if (!FlightPlan.IsValid()) return;
	if (this->outOfVis(FlightPlan)) return;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	std::string ades = FlightPlan.GetFlightPlanData().GetDestination();
	std::string fplnRwy = vsid::fpln::getAtcBlock(FlightPlan).second;

	if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_SCRATCH_PAD_STRING)
	{
		std::string scratchpad = vsid::utils::toupper(cad.GetScratchPadString());

		if (lastScratchCS == callsign && lastScratchMsg == scratchpad) return; // #dev - new scratchpad skipping
		else
		{
			vsid::Logger::log(LogLevel::Debug, std::format("Scratchpad changed since last update. Processing scratchpad. Old CS [{}]. New CS [{}]. Old Msg [{}]. New Msg [{}]",
				lastScratchCS, callsign, lastScratchMsg, scratchpad), vsid::DebugLevel::Dev, true);

			lastScratchCS = callsign;
			lastScratchMsg = scratchpad;
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Scratchpad [{}]", callsign, scratchpad), vsid::DebugLevel::Dev);

		SyncManager::update(FlightPlan);

		if (!scratchpad.empty())
		{
			// set clearance flag - also unprocessed flight plans

			if (scratchpad.find(".VSID_CTL_") != std::string::npos)
			{
				std::string toFind = ".VSID_CTL_";
				size_t pos = scratchpad.find(".VSID_CTL_");

				bool ctl = scratchpad.substr(pos + toFind.size(), scratchpad.size()) == "TRUE" ? true : false;

				if (!FplnManager::getProcessed().contains(callsign)) // #evaluate - setting 'false' could delete from processed if ades is not active (protection against too many entries)
				{
					if (AirportManager::isActive(ades) ||
						AirportManager::isActive(adep) ||
						FlightPlan.GetFlightPlanData().GetPlanType() == std::string("V"))
					{
						FplnManager::add(callsign); // creation of flight plan of arriving traffic
						FplnManager::update(callsign, [ctl](vsid::fpln::FplnData &data)
							{
								data.ctl = ctl;
							});
					}
				}
				else (void)FplnManager::update(callsign, [ctl](vsid::fpln::FplnData& data)
					{
						data.ctl = ctl;
					});
			}

			// work on processed flight plans

			if (const auto* processedFpln = FplnManager::getData(callsign); processedFpln)
			{
				// set intersection

				if (const auto* aptData = AirportManager::getData(adep); aptData && scratchpad.size() <= 4)
				{
					if (size_t pos = scratchpad.find("+"); pos != std::string::npos)
					{
						std::string intsec = scratchpad.substr(pos + 1, scratchpad.size());

						if (aptData->intsec.contains(fplnRwy) && vsid::utils::contains(aptData->intsec.at(fplnRwy), intsec)) // #continue - check for spReleased
						{
							(void)FplnManager::update(callsign, [&intsec](vsid::fpln::FplnData& data)
								{
									data.intsec = { intsec, true };
								});

							FlightPlan.GetControllerAssignedData().SetScratchPadString("");
						}
					}
					else if (size_t pos = scratchpad.find("-"); pos != std::string::npos)
					{
						std::string intsec = scratchpad.substr(pos + 1, scratchpad.size());

						if (aptData->intsec.contains(fplnRwy) && vsid::utils::contains(aptData->intsec.at(fplnRwy), intsec))
						{
							(void)FplnManager::update(callsign, [&intsec](vsid::fpln::FplnData& data)
								{
									data.intsec = { intsec, false };
								});

							FlightPlan.GetControllerAssignedData().SetScratchPadString("");
						}
					}
				}

				// check for multiple auto-mode users

				if (size_t pos = scratchpad.find(".VSID_AUTO_"); pos != std::string::npos)
				{
					std::string toFind = ".VSID_AUTO_";

					if (const auto* aptData = AirportManager::getData(adep); aptData && aptData->settings.at("auto"))
					{
						std::string atc = scratchpad.substr(pos + toFind.size(), scratchpad.size());
						if (ControllerMyself().GetCallsign() != atc)
						{
							vsid::Logger::log(LogLevel::Warning, std::format("[{}] assigned SID [{}] by [{}]", callsign,
								FlightPlan.GetFlightPlanData().GetSidName(), atc));
						}
					}
				}

				// sync release if GRP states are synced - ES is released on gnd state updates

				if (scratchpad.find("NOSTATE") != std::string::npos)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.gndState = "NOSTATE";
						});
				}
					
				if (scratchpad.find("ONFREQ") != std::string::npos)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.gndState = "ONFREQ";
						});
				}

				if (scratchpad.find("DE-ICE") != std::string::npos)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.gndState = "DE-ICE";
						});
				}

				if (scratchpad.find("LINEUP") != std::string::npos)
				{
					(void)FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
						{
							data.gndState = "LINEUP";
						});
				}

				// clearance flag released while sending - (now temp. below GND states here)

				// request entries

				if (scratchpad.find(".VSID_REQ_") != std::string::npos)
				{
					std::string toFind = ".VSID_REQ_";
					size_t pos = scratchpad.find(toFind);

					vsid::Logger::log(LogLevel::Debug, std::format("[{}] found \".vsid_req_\" in scratch [{}]", callsign, scratchpad), vsid::DebugLevel::Req);

					try
					{
						std::vector<std::string> req = vsid::utils::split(scratchpad.substr(pos + toFind.size(), scratchpad.size()), '/');
						std::string reqType = vsid::utils::tolower(req.at(0));
						bool isRwyReq = reqType.find("rwy") != std::string::npos;

						if (isRwyReq)
						{
							try
							{
								reqType = vsid::utils::split(reqType, ' ').at(1);
							}
							catch (std::out_of_range&)
							{
								vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to split req type [{}] in scratch pad update. "
									"Stopping setting request!", callsign, reqType));

								return;
							}
						}
						long long reqTime = std::stoll(req.at(1));

						// clear all possible requests before setting a new one

						if (const auto* aptData = AirportManager::getData(adep); aptData)
						{
							bool reqActive = false; // preserves active req state which would be overwritten if a req list resulting in false comes after
							std::string fplnRequest = "";
							int fplnRequestTime = -1;
							bool updateProcessedFpln = false;

							(void)AirportManager::update(adep, [&](vsid::apt::AirportData& data)
							{
								for (auto it = data.requests.begin(); it != data.requests.end(); ++it)
								{
									auto& requestSet = it->second;

									for (auto jt = requestSet.begin(); jt != requestSet.end();)
									{
										if (jt->first != callsign)
										{
											++jt;
											continue;
										}

										if (!reqActive)
										{
											fplnRequest = "";
											fplnRequestTime = -1;
											updateProcessedFpln = true;
										}

										vsid::Logger::log(
											LogLevel::Debug,
											std::format("[{}] removing from requests in [{}]", callsign, it->first),
											vsid::DebugLevel::Req
										);

										jt = it->second.erase(jt);
									}
									if (it->first == reqType)
									{
										vsid::Logger::log(
											LogLevel::Debug,
											std::format("[{}] (equal reqType) setting in requests in [{}]", callsign, it->first),
											vsid::DebugLevel::Req
										);

										it->second.insert({ callsign, reqTime });

										if (!isRwyReq)
										{
											fplnRequest = reqType;
											fplnRequestTime = reqTime;
											updateProcessedFpln = true;

											reqActive = true;
										}
									}
								}

								for (auto& [type, rwys] : data.rwyrequests)
								{
									for (auto it = rwys.begin(); it != rwys.end(); ++it)
									{
										for (auto jt = it->second.begin(); jt != it->second.end();)
										{
											if (jt->first != callsign)
											{
												++jt;
												continue;
											}

											if (!reqActive)
											{
												fplnRequest = "";
												fplnRequestTime = -1;
												updateProcessedFpln = true;
											}

											vsid::Logger::log(
												LogLevel::Debug,
												std::format(
													"[{}] removing from rwy requests in [{}/{}]",
													callsign,
													type,
													it->first),
												vsid::DebugLevel::Req);

											jt = it->second.erase(jt);
										}
									}
									if (type == reqType && !fplnRwy.empty())
									{
										vsid::Logger::log(LogLevel::Debug, std::format("[{}] setting in rwy requests in [{}/{}]", callsign, type,
											fplnRwy), vsid::DebugLevel::Req);

										rwys[fplnRwy].insert({ callsign, reqTime });

										if (isRwyReq)
										{
											fplnRequest = "rwy " + reqType;
											fplnRequestTime = reqTime;
											updateProcessedFpln = true;

											reqActive = true;
										}
									}
									else if (isRwyReq && type == reqType && fplnRwy.empty())
										vsid::Logger::log(LogLevel::Warning, std::format("[{}] to be set in runway requests, but runway hasn't been set in the flight plan.", callsign));
								}
							});

							if (updateProcessedFpln)
							{
								FplnManager::update(callsign, [fplnRequestTime, fplnRequest](vsid::fpln::FplnData& data)
									{
										data.request = fplnRequest;
										data.reqTime = fplnRequestTime;
									});
							}
							
						}
						else
							vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}] is not an active airport in req setting", callsign,
								adep), vsid::DebugLevel::Dev);

						messageHandler->removeFplnError(callsign, ERROR_FPLN_REQSET);
					}
					catch (std::out_of_range &e)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_REQSET))
						{
							vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to set the request. Code: {}", callsign, ERROR_FPLN_REQSET));

							messageHandler->addFplnError(callsign, ERROR_FPLN_REQSET);
						}
					}
				}

				// intersections

				if (scratchpad.find(".VSID_INT_") != std::string::npos)
				{
					std::string toFind = ".VSID_INT_";
					size_t pos = scratchpad.find(toFind);

					try
					{
						std::vector<std::string> intersection = vsid::utils::split(scratchpad.substr(pos + toFind.size(), scratchpad.size()), '_');

						if (intersection.at(0) == "NONE")
						{
							FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
								{
									data.intsec = { "", false };
								});
						}
						else
						{
							FplnManager::update(callsign, [&intersection](vsid::fpln::FplnData& data)
								{
									data.intsec = { intersection.at(0), ((intersection.at(1) == "TRUE") ? true : false) };
								});
						}

						messageHandler->removeFplnError(callsign, ERROR_FPLN_INTSET);
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_INTSET))
						{
							vsid::Logger::log(LogLevel::Error, std::format("[{}] failed to set the intersection. Code: {}", callsign, ERROR_FPLN_INTSET));

							messageHandler->addFplnError(callsign, ERROR_FPLN_INTSET);
						}
					}
				}

				// handover flag

				if (scratchpad.find(".VSID_HOV_") != std::string::npos)
				{
					std::string toFind = ".VSID_HOV_";
					size_t pos = scratchpad.find(toFind);

					bool hov = scratchpad.substr(pos + toFind.size(), scratchpad.size()) == "TRUE" ? true : false;

					FplnManager::update(callsign, [hov](vsid::fpln::FplnData& data)
						{
							data.hov = hov;
						});
				}
			}
		}		
	}

	if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE) // updating sync release for ES states as they're not always seen in scratch pad
	{
		SyncManager::update(FlightPlan, "GND");
	}

	if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_CLEARENCE_FLAG) //#dev updating sync release for ES clearance flag as it is not seen in scratch pad
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] received clearance flag update", callsign), vsid::DebugLevel::Dev);

		SyncManager::update(FlightPlan, "CLEA");
	}

	if (const auto* aptData = AirportManager::getData(adep); aptData)
	{
		if (const auto* processedFpln = FplnManager::getData(callsign); processedFpln)
		{
			// get ES gnd states

			if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE)
			{
				(void)FplnManager::update(callsign, [&](vsid::fpln::FplnData& data)
					{
						data.gndState = FlightPlan.GetGroundState();

						if (data.gndState == "DEPA") data.intsec = { "", false };
					});
			}

			// remove requests if present - might also trigger without a present scratchpad

			std::string fplnRequest = "";
			int fplnRequestTime = -1;
			bool updateProcessedFpln = false;

			AirportManager::update(adep, [&](vsid::apt::AirportData& data)
				{
					if (processedFpln->request != "")
					{
						if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_CLEARENCE_FLAG)
						{
							if (FlightPlan.GetClearenceFlag())
							{
								for (auto& fp : data.requests["clearance"])
								{
									if (fp.first != callsign) continue;

									data.requests["clearance"].erase(fp);

									fplnRequest = "";
									fplnRequestTime = -1;
									updateProcessedFpln = true;

									break;
								}
							}
						}

						if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE)
						{
							std::string state = FlightPlan.GetGroundState();

							if (state == "STUP")
							{
								for (auto& fp : data.requests["startup"])
								{
									if (fp.first != callsign) continue;

									data.requests["startup"].erase(fp);

									fplnRequest = "";
									fplnRequestTime = -1;
									updateProcessedFpln = true;

									break;
								}

								for (auto& [rwy, rwyReq] : data.rwyrequests["startup"])
								{
									for (auto& fp : rwyReq)
									{
										if (fp.first != callsign) continue;

										data.rwyrequests["startup"][rwy].erase(fp);

										fplnRequest = "";
										fplnRequestTime = -1;
										updateProcessedFpln = true;

										break;
									}

								}
							}
							else if (state == "PUSH")
							{
								for (auto& fp : data.requests["pushback"])
								{
									if (fp.first != callsign) continue;

									data.requests["pushback"].erase(fp);

									fplnRequest = "";
									fplnRequestTime = -1;
									updateProcessedFpln = true;

									break;
								}
							}
							else if (state == "TAXI")
							{
								for (auto& fp : data.requests["taxi"])
								{
									if (fp.first != callsign) continue;

									data.requests["taxi"].erase(fp);

									fplnRequest = "";
									fplnRequestTime = -1;
									updateProcessedFpln = true;

									break;
								}

							}
							else if (state == "DEPA")
							{
								for (auto& fp : data.requests["departure"])
								{
									if (fp.first != callsign) continue;

									data.requests["departure"].erase(fp);

									fplnRequest = "";
									fplnRequestTime = -1;
									updateProcessedFpln = true;

									break;
								}
							}
						}
					}
				});

			if (updateProcessedFpln)
			{
				FplnManager::update(callsign, [&](vsid::fpln::FplnData& data)
					{
						data.request = fplnRequest;
						data.reqTime = fplnRequestTime;
					});
			}
		}
	}
}

void vsid::VSIDPlugin::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	std::string callsign = FlightPlan.GetCallsign();
	std::string icao = FlightPlan.GetFlightPlanData().GetOrigin();

	if (FplnManager::getProcessed().contains(callsign))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] disconnected from the network.", callsign), vsid::DebugLevel::Fpln);

		FplnManager::clearSidData(callsign);
		FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
			{
				data.removalTime = std::chrono::system_clock::now() + std::chrono::minutes{ 1 };
			});

		this->removeFromRequests(callsign, icao);

		messageHandler->removeCallsignFromErrors(callsign);
	}
}

void vsid::VSIDPlugin::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget RadarTarget)
{
	if (!RadarTarget.IsValid()) return;

	std::string callsign = RadarTarget.GetCallsign();
	std::string adep = RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetOrigin();
	std::string ades = RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetDestination();

	if (const auto* processedFpln = FplnManager::getData(callsign); processedFpln)
	{
		if (processedFpln->ldgAlt != 0 && AirportManager::isActive(ades))
		{
			int alt = RadarTarget.GetPosition().GetPressureAltitude();

			FplnManager::update(callsign, [&](vsid::fpln::FplnData& data)
				{
					if (alt <= std::abs(data.ldgAlt - 200))
					{
						data.ldgAlt = alt;
					}
					else if (alt >= data.ldgAlt + 200)
					{
						data.mapp = true; // #continue - send mapp to network if not already set
						data.ctl = false;
					}
				});		
		}

		// trigger when speed is >= 50 knots
		if (AirportManager::isActive(adep) && RadarTarget.GetGS() >= 50)
		{
			// remove requests that might still be present
			this->removeFromRequests(callsign, adep);

			// remove rwy remark if still present
			if (vsid::fpln::findRemarks(RadarTarget.GetCorrelatedFlightPlan(), "VSID/RWY"))
			{
				EuroScopePlugIn::CFlightPlan fpln = RadarTarget.GetCorrelatedFlightPlan();

				vsid::fpln::removeRemark(fpln, "VSID/RWY");

				if (!fpln.GetFlightPlanData().AmendFlightPlan())
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
					{
						vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND));

						messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
					}
				}
				else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
			}

			// remove from intersections that might still be present

			FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
				{
					data.intsec = { "", false };
				});
		}

		// remove arriving tfc

		else if (RadarTarget.GetGS() < 50 && !AirportManager::isActive(adep))
		{
			if (adep != ades && adep != "")
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] arrived. Removing from processed.", callsign), vsid::DebugLevel::Fpln);

				FplnManager::remove(callsign);
			}
		}
	}
}

void vsid::VSIDPlugin::OnControllerPositionUpdate(EuroScopePlugIn::CController Controller)
{
	vsid::apt::AtcData atcData;
	std::string_view atcIcao;

	const std::string atcCallsign = Controller.GetCallsign(); // #continue - transform to string_view
	
	atcData.si = Controller.GetPositionId();
	atcData.facility = Controller.GetFacility();
	atcData.freq = Controller.GetPrimaryFrequency();

	bool invalidFreq = atcData.freq < 0.1 || atcData.freq > 199.0;

	auto failit = this->atcFailCounter.find(atcCallsign);
	bool inFailCounter = (failit != this->atcFailCounter.end());

	if (atcCallsign == ControllerMyself().GetCallsign()) return;
	if (this->activeAtc.contains(atcCallsign) || this->ignoredAtc.contains(atcCallsign)) return;

	if (inFailCounter)
	{
		if (failit->second >= MAX_ATC_FAIL_COUNT)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Adding to ignore list after reaching max_atc_fail_count.", atcCallsign),
				vsid::DebugLevel::Atc);

			this->ignoredAtc.insert({ atcCallsign, atcData });
			return;
		}

		// maximum 3 attempts to try and match the callsign or frequency against ese stored atc stations
		if (failit->second > 2 && failit->second < MAX_ATC_FAIL_COUNT)
		{
			for (const vsid::SectionAtc& sAtc : this->sectionAtc)
			{
				if (vsid::utils::svEqualCi(atcCallsign, sAtc.callsign) || atcFreqMatch(Controller, sAtc))
				{
					atcData.si = sAtc.si;
					atcData.freq = sAtc.freq;
					atcData.facility = sAtc.facility;

					vsid::Logger::log(LogLevel::Debug, std::format("[{}] match found in parsed stations. Setting SI [{}] | FREQ [{}] | FAC [{}]",
						atcCallsign, atcData.si, atcData.freq, atcData.facility), vsid::DebugLevel::Atc);

					this->atcFailCounter.erase(failit);
					inFailCounter = false;

					break;
				}
			}
		}
	}

	try
	{
		atcIcao = vsid::utils::splitSV(atcCallsign, '_').at(0);
		messageHandler->removeGenError(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign);
	}
	catch (const std::out_of_range &e)
	{
		if (!messageHandler->genErrorsContains(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign))
		{
			vsid::Logger::log(LogLevel::Error, std::format("Failed to get ICAO part of controller callsign [{}] in ATC update. Code: {}",
				atcCallsign, ERROR_ATC_ICAOSPLIT));

			messageHandler->addGenError(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign);
		}
	}

	if (atcCallsign.find("ATIS") != std::string::npos)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Adding ATIS to ignore list.", atcCallsign), vsid::DebugLevel::Atc);

		this->ignoredAtc.insert({ atcCallsign, atcData });
		return;
	}

	if (atcCallsign.ends_with("FMP"))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Adding FMP station to ignore list.", atcCallsign), vsid::DebugLevel::Atc);

		this->ignoredAtc.insert({ atcCallsign, atcData });
		return;
	}

	if (atcCallsign.ends_with("SUP"))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] Adding SUP station to ignore list.", atcCallsign), vsid::DebugLevel::Atc);

		this->ignoredAtc.insert({ atcCallsign, atcData });
		return;
	}

	if (atcData.facility < 2)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] has facility below 2 (usually FIS). Adding to ignore list.",
			atcCallsign), vsid::DebugLevel::Atc);

		this->ignoredAtc.insert({ atcCallsign, atcData });
		return;
	}

	if (!Controller.IsController())
	{
		if (inFailCounter)
		{
			++failit->second;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] is not a controller. Increasing fail count [{}/{}]",
				atcCallsign, failit->second, MAX_ATC_FAIL_COUNT), vsid::DebugLevel::Atc);

			return;
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] ATC is not a controller. Adding to fail count.",
			atcCallsign), vsid::DebugLevel::Atc);

		this->atcFailCounter.insert({ atcCallsign, 1 });
	}
	
	if (invalidFreq)
	{
		if (inFailCounter)
		{
			++failit->second;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] has invalid frequency [{}]. Increasing fail count [{}/{}]",
				atcCallsign, atcData.freq, failit->second, MAX_ATC_FAIL_COUNT), vsid::DebugLevel::Atc);

			return;
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] has invalid frequency [{}]. Adding to fail count.",
			atcCallsign, atcData.freq), vsid::DebugLevel::Atc);

		this->atcFailCounter.insert({ atcCallsign, 1 });
	}

	if (atcData.si.empty())
	{
		if (inFailCounter)
		{
			++failit->second;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] is skipped because the SI is empty. Increasing fail count [{}/{}]",
				atcCallsign, failit->second, MAX_ATC_FAIL_COUNT), vsid::DebugLevel::Atc);

			return;
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] is skipped because the SI is empty. Adding to fail count.",
			atcCallsign), vsid::DebugLevel::Atc);

		this->atcFailCounter.insert({ atcCallsign, 1 });

		return;
	}

	if (std::all_of(atcData.si.begin(), atcData.si.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
	{
		if (inFailCounter)
		{
			++failit->second;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] is skipped because the SI is a number [{}]. Increasing fail count [{}/{}]",
				atcCallsign, atcData.si, failit->second, MAX_ATC_FAIL_COUNT), vsid::DebugLevel::Atc);

			return;
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] is skipped because the SI is a number [{}]. Adding to fail count.",
			atcCallsign, atcData.si), vsid::DebugLevel::Atc);

		this->atcFailCounter.insert({ atcCallsign, 1 });
	}
	else if (auto it = this->atcFailCounter.find(atcCallsign); it != this->atcFailCounter.end())
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] is removed from fail counter after "
			"SI [{}] is valid.", atcCallsign, atcData.si), vsid::DebugLevel::Atc);

		this->atcFailCounter.erase(it);
	}

	EuroScopePlugIn::CController atcMyself = ControllerMyself();
	std::set<std::string> atcIcaos;

	if (atcData.facility < 6 && !atcIcao.empty())
		atcIcaos.insert(std::string(atcIcao));

	if (atcData.facility >= 5)
	{
		bool ignore = true;

		for (const auto& [icao, aptInfo] : AirportManager::getAirports())
		{
			if (aptInfo.appSI.contains(atcData.si))
			{
				ignore = false;
				atcIcaos.insert(icao);
			}
			else if (icao == atcIcao)
				ignore = false;
		}

		if (ignore)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] adding to ignore list because SI "
				"[{}] is not mentioned in config and ICAO [{}] does not match airport.",
				atcCallsign, atcData.si, atcIcao), vsid::DebugLevel::Atc);

			this->ignoredAtc.insert({atcCallsign, atcData });
			return;
		}
	}

	for (const std::string& atcIcao : atcIcaos)
	{
		if (!AirportManager::isActive(atcIcao)) continue;

		AirportManager::update(atcIcao, [&](vsid::apt::AirportData& data)
			{
				if (auto jt = data.controllers.find(atcData.si); jt == data.controllers.end())
				{
					atcData.Icaos.insert(atcIcao);
					data.controllers.insert({ atcCallsign, atcData });
					this->activeAtc.insert({ atcCallsign, atcData });

					vsid::Logger::log(LogLevel::Debug, std::format("[{}] adding to active ATC list in [{}].", atcCallsign, atcIcao), vsid::DebugLevel::Atc);
				}

				if (data.settings["auto"] && !data.forceAuto && data.hasLowerAtc(atcMyself))
				{
					vsid::Logger::log(LogLevel::Info, std::format("[{}] Disabling auto mode. [{}] now online.", atcIcao, atcCallsign), vsid::DebugLevel::Atc);

					data.settings["auto"] = false;
				}
			});		
	}
}

void vsid::VSIDPlugin::OnControllerDisconnect(EuroScopePlugIn::CController Controller)
{
	std::string atcCallsign = Controller.GetCallsign();

	if (auto it = this->activeAtc.find(atcCallsign); it != this->activeAtc.end())
	{
		for (const auto& [icao, airport] : AirportManager::getAirports())
		{
			if (!it->second.Icaos.contains(icao)) continue;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] disconnected. Removing from ATC list for [{}].", atcCallsign,
				icao), vsid::DebugLevel::Atc);

			AirportManager::update(icao, [&atcCallsign](vsid::apt::AirportData& data)
				{
					data.controllers.erase(atcCallsign);
				});
		}

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] disconnected. Removing from general active ATC list.", atcCallsign), vsid::DebugLevel::Atc);

		this->activeAtc.erase(it);
	}

	if (this->ignoredAtc.contains(atcCallsign))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] disconnected. Removing from ignore list.", atcCallsign), vsid::DebugLevel::Atc);
		this->ignoredAtc.erase(atcCallsign);
	}

	if (this->atcFailCounter.contains(atcCallsign))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] disconnected. Removing from fail counter list.", atcCallsign), vsid::DebugLevel::Atc);
		this->atcFailCounter.erase(atcCallsign);
	}
}

void vsid::VSIDPlugin::OnAirportRunwayActivityChanged()
{
	this->detectPlugins();

	this->UpdateActiveAirports();

	for (auto& [id, screen] : this->radarScreens)
	{
		screen->OnAirportRunwayActivityChanged();
	}
}

void vsid::VSIDPlugin::UpdateActiveAirports()
{
	using AirportManager = vsid::apt::AirportManager;

	vsid::Logger::log(LogLevel::Info, "Updating airports...", vsid::DebugLevel::Conf);

	this->SelectActiveSectorfile();
	  
	// get active airports
	std::set<std::string> removedAirports;

	for (EuroScopePlugIn::CSectorElement sfe =	this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
												sfe.IsValid();
												sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT)
		)
	{
		const std::string& icao = vsid::utils::trim(sfe.GetName());
		const bool sectorActive = sfe.IsElementActive(true) || sfe.IsElementActive(false); // active for departure or arrival

		if (!sectorActive && vsid::apt::AirportManager::isActive(icao))
		{
			AirportManager::remove(icao);
			removedAirports.insert(icao);
		}
		else if (sectorActive && !vsid::apt::AirportManager::isActive(icao))
		{
			AirportManager::add(icao, { .icao = icao });
		}
	}

	// drop flight plans that are stale (no longer found) or whose origin airport just went inactive -
	// reactivated/new ones are picked up again automatically via OnGetTagItem once it fires for them

	if (!removedAirports.empty())
	{
		std::set<std::string> invalidatedFplns;

		for (const auto& [callsign, fplnInfo] : FplnManager::getProcessed())
		{
			EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());

			if (!FlightPlan.IsValid() || removedAirports.contains(FlightPlan.GetFlightPlanData().GetOrigin()))
			{
				invalidatedFplns.insert(callsign);
			}
		}

		for (const auto& callsign : invalidatedFplns)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] no longer valid or origin airport deactivated. Removing from processed.", callsign), vsid::DebugLevel::Fpln);

			FplnManager::remove(callsign);
		}
	}

	// get active rwys - accumulated per airport across all its runway elements

	std::map<std::string, std::pair<std::set<std::string>, std::set<std::string>>> rwysByAirport; // icao -> {arrRwys, depRwys}

	for (EuroScopePlugIn::CSectorElement sfe =	this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
												sfe.IsValid();
												sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY)
		)
	{
		const std::string& aptName = vsid::utils::trim(sfe.GetAirportName());

		if (!AirportManager::isActive(aptName)) continue;

		auto& [arrRwys, depRwys] = rwysByAirport[aptName];

		if (sfe.IsElementActive(false, 0)) arrRwys.insert(vsid::utils::trim(sfe.GetRunwayName(0)));
		if (sfe.IsElementActive(true, 0)) depRwys.insert(vsid::utils::trim(sfe.GetRunwayName(0)));
		if (sfe.IsElementActive(false, 1)) arrRwys.insert(vsid::utils::trim(sfe.GetRunwayName(1)));
		if (sfe.IsElementActive(true, 1)) depRwys.insert(vsid::utils::trim(sfe.GetRunwayName(1)));
	}

	for (const auto& [aptName, rwys] : rwysByAirport)
	{
		AirportManager::update(aptName, [&rwys](vsid::apt::AirportData& aptData)
			{
				aptData.arrRwys = rwys.first;
				aptData.depRwys = rwys.second;
			});
	}

	// only load configs if at least one airport has been selected
	if (!AirportManager::empty())
	{
		this->configParser.loadAirportConfig();

		vsid::Logger::log(LogLevel::Debug, "Checking .ese file for SID mastering...", vsid::DebugLevel::Conf);

		//************************************
		// temp storage for OID mismatches
		// Parameter:	<std::string, - ICAO
		// Parameter:	, std::map<std::string, bool>> - map of OID to whether it has been matched with a section SID or not
		//************************************
		std::map<std::string, std::map<std::string, bool>> incompOIDs;

		// if there are configured airports check for remaining sid data

		for(auto &sectionSid : this->sectionSids)
		{
			auto& activeAirports = AirportManager::getAirports();
			auto aptIt = activeAirports.find(sectionSid.apt);

			if (aptIt == activeAirports.end()) continue;

			auto& airport = aptIt->second;
			
			std::vector<vsid::Sid> tmpSidVec = airport.sids;

			for (vsid::Sid& sid : tmpSidVec)
			{
				if (sid.base != sectionSid.base)
				{
					// if OID is skipped due to unmatching bases mark it has incompatible to yield warnings

					if (vsid::utils::containsDigit(sid.base) && !incompOIDs[sectionSid.apt].contains(sid.base + sid.number + sid.designator))
					{
						incompOIDs[sectionSid.apt][sid.base + sid.number + sid.designator] = false;
					}

					// skip unmatching first three char comparison (filter)

					if (sid.base.length() > 2 && sectionSid.base.length() > 2)
					{
						if (sid.base[0] != sectionSid.base[0]) continue;
						if (sid.base[1] != sectionSid.base[1]) continue;
						if (sid.base[2] != sectionSid.base[2]) continue;
					}

					if (!sid.collapsedBaseMatch(sectionSid.base))
					{
						continue;
					}

					vsid::Logger::log(LogLevel::Debug, std::format("[{}] sid collapsed matched [{}]", sid.base, sectionSid.base), vsid::DebugLevel::Dev);
				}
				if (sid.designator != (sectionSid.desig ? std::string(1, *sectionSid.desig) : "")) continue;
				if (!vsid::utils::contains(sid.rwys, sectionSid.rwy)) continue;

				if (std::isdigit(sectionSid.number))
				{
					if (sid.number == "")
					{
						sid.number = sectionSid.number;

						vsid::Logger::log(LogLevel::Debug, std::format("[{} (ID: {})] mastered. Master rwy [{}]",
							sid.base + sid.number + sid.designator, sid.id, sectionSid.rwy), vsid::DebugLevel::Conf);
					}
					else if (sid.number != "" && sid.number != std::string(1, sectionSid.number) && sid.allowDiffNumbers)
					{
						if ((!sectionSid.route.empty() && sectionSid.route.find(sid.waypoint) != std::string::npos) || sectionSid.route.empty())
						{
							std::string oldNumber = sid.number; // debugging value
							sid.number = sectionSid.number;

							vsid::Logger::log(LogLevel::Debug, std::format("[{} (ID: {})] overwritten old number [{}] with [{}]. RWYs matched "
								"and diff numbers allowed. Master rwy [{}]",
								sid.base + sid.number + sid.designator, sid.id, oldNumber, sid.number, sectionSid.rwy), vsid::DebugLevel::Conf);
						}
					}
					else if (!sid.number.empty()) // health check for possible errors in .ese config
					{
						if (vsid::utils::containsDigit(sid.base))
						{
							if(!incompOIDs[sectionSid.apt].contains(sid.base + sid.number + sid.designator))
								incompOIDs[sectionSid.apt][sid.base + sid.number + sid.designator] = false;

							if (vsid::utils::trim(sid.base + sid.number + sid.designator) ==
								vsid::utils::trim(sectionSid.base + sectionSid.number + sectionSid.desig.value_or(' ')))
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[MIL SID] [{}] equal [{}]",
									sid.base + sid.number + sid.designator, sectionSid.base + sectionSid.number + sectionSid.desig.value_or(' ')),
									vsid::DebugLevel::Conf);

								incompOIDs[sectionSid.apt][sid.base + sid.number + sid.designator] = true;
							}
							else
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[MIL SID] [{}] NOT equal: [{}]",
									sid.base + sid.number + sid.designator, sectionSid.base + sectionSid.number + sectionSid.desig.value_or(' ')),
									vsid::DebugLevel::Dev, true);
							}

							continue;
						}

						int currNumber = -1;

						try
						{
							currNumber = std::stoi(sid.number); // #dev - removed int -> debugging
						}
						catch (const std::invalid_argument& e)
						{
							vsid::Logger::log(LogLevel::Error, std::format("Collapsing SID [{}] caused an error while collapsing base for section SID [{}]. Error: {}",
								sid.idName(), sectionSid.base, e.what()));
						}
						catch (const std::out_of_range& e)
						{
							vsid::Logger::log(LogLevel::Error, std::format("Collapsing SID [{}] caused an error while collapsing base for section SID [{}]. Error: {}",
								sid.idName(), sectionSid.base, e.what()));
						}
						if (currNumber == -1) continue;
						
						int newNumber = sectionSid.number - '0';

						if (currNumber > newNumber || (currNumber == 1 && newNumber == 9))
						{
							vsid::Logger::log(LogLevel::Warning, std::format("[{}] Check your .ese - file for [{}?{}] SID! Already set number [{} (ID: {})]. "
								"Now found additional number [{} - (Runway: {})]. Skipping additional number (is lower or before restarting count) due to "
								"possible sector file error!", sectionSid.apt, sid.base, sid.designator, currNumber, sid.id, newNumber, sectionSid.rwy));
						}
						else if (currNumber < newNumber || (newNumber == 1 && currNumber == 9))
						{
							vsid::Logger::log(LogLevel::Warning, std::format("[{}] Check your.ese - file for [{}?{}] SID!Already set number [{} (ID: {})]. "
								"Now found additional number [{} - (Runway: {})]. Setting additional number (is higher or after restarting count) due to "
								"possible sector file error!",
								sectionSid.apt, sid.base, sid.designator, currNumber, sid.id, newNumber, sectionSid.rwy));

							sid.number = std::to_string(newNumber);
						}
						else if (currNumber != newNumber)
						{
							vsid::Logger::log(LogLevel::Warning, std::format("[{}] Check your.ese - file for [{}?{}] SID!Already set number [{} (ID: {})]. "
								"Now found additional number [{} - (Runway: {})]. Setting additional number as it couldn't be determined which one is more "
								"likely to be correct!", sectionSid.apt, sid.base, sid.designator, currNumber, sid.id, newNumber, sectionSid.rwy));

							sid.number = std::to_string(newNumber);
						}
					}

					if (!sid.transition.empty() && sectionSid.trans.base != "")
					{
						vsid::Logger::log(LogLevel::Debug, std::format("Checking SID [{}{}{}] with transition [{}{}{}].",
							sectionSid.base, sectionSid.number,
							(sectionSid.desig) ? std::string(1, *sectionSid.desig) : "",
							sectionSid.trans.base,
							(sectionSid.trans.number) ? std::string(1, *sectionSid.trans.number) : "",
							(sectionSid.trans.desig) ? std::string(1, *sectionSid.trans.desig) : ""));

						for (auto& [transBase, trans] : sid.transition)
						{
							if (transBase != sectionSid.trans.base)
							{
								// skip unmatching first two char comparison (filter)
								if (transBase.length() > 2 && sectionSid.trans.base.length() > 2)
								{
									if (transBase[0] != sectionSid.trans.base[0]) continue;
									if (transBase[1] != sectionSid.trans.base[1]) continue;
									if (transBase[2] != sectionSid.trans.base[2]) continue;
								}

								if (!sid.collapsedBaseMatch(sectionSid.trans.base, transBase))
								{
									continue;
								}
								vsid::Logger::log(LogLevel::Debug, std::format("[{}] trans collapsed matched [{}]",
									transBase, sectionSid.trans.base), vsid::DebugLevel::Conf);
							}
							if (trans.designator != (sectionSid.trans.desig ? std::string(1, *sectionSid.trans.desig) : "")) continue;
							if (trans.number != "") // #refactor .number to char
							{
								vsid::Logger::log(LogLevel::Debug, std::format("[{}{}{}] (ID: {}) transition [{}{}{}] already mastered. Skipping current transition number: {}",
									sid.base, sid.number, sid.designator, sid.id, trans.base, trans.number, trans.designator,
									(sectionSid.trans.number ? std::string(1, *sectionSid.trans.number) : "")), vsid::DebugLevel::Conf);

								continue;
							}

							if (sectionSid.trans.number && std::isdigit(*sectionSid.trans.number))
							{
								trans.number = *sectionSid.trans.number;

								vsid::Logger::log(LogLevel::Debug, std::format("[{}{}{}] (ID: {}) mastered transition [{}{}{}]", sid.base,
									sid.number, sid.designator, sid.id, trans.base, trans.number, trans.designator), vsid::DebugLevel::Conf);

								break;
							}
							else if (!sectionSid.trans.number && (trans.designator == "" || trans.designator != "XXX"))
							{
								trans.number = "-1"; // dummy value for wpt transitions

								vsid::Logger::log(LogLevel::Debug, std::format("[{}{}{}] (ID: {}) mastered transition [{}{}{}]", sid.base,
									sid.number, sid.designator, sid.id, trans.base, trans.number, trans.designator), vsid::DebugLevel::Conf);

								break;
							}
						}
					}
				}
			}

			AirportManager::update(sectionSid.apt, [&tmpSidVec](vsid::apt::AirportData& data)
				{
					data.sids = std::move(tmpSidVec);
				});
		}

		for (auto& [apt, oids] : incompOIDs)
		{
			if (oids.empty()) continue;

			std::ostringstream ss;
			ss << "[" << apt << "] Check your config file for the following OIDs that couldn't be mastered: ";
			std::string separator = "";
			int mismatchCount = 0;

			for (auto& [oid, matched] : oids)
			{
				if (!matched)
				{
					ss << separator << oid;
					separator = ", ";
					mismatchCount++;
				}
			}

			if (mismatchCount > 0)
				vsid::Logger::log(LogLevel::Warning, ss.str());

			ss.clear();
		}
	}

	//// DOCUMENTATION
			//if (!sidSection)
			//{
			//	if (std::string(sfe.GetAirportName()) == "EDDF")
			//	{

			//		continue;
			//	}
			//	else
			//	{
			//		sidSection = true;
			//	}
			//}
			//if (std::string(sfe.GetAirportName()) == "EDDF")
			//{
			//	messageHandler->writeMessage("DEBUG", "sfe elem: " + std::string(sfe.GetName()));
			//}

	// health check in case SIDs in config do not match sector file

	//************************************
	// Parameter:	<std::string, - ICAO
	// Parameter:	, std::set<std::string> - incompatible SID names
	//************************************
	std::map<std::string, std::set<std::string>> incompSids;
	//************************************
	// Parameter:	<std::string, - ICAO
	// Parameter:	, std::map<std::string, - SID Name
	// Parameter:	, std::set<std::string> - incompatible transition - full name with ? as number
	//************************************
	std::map<std::string, std::map<std::string, std::set<std::string>>> incompTrans;

	for (auto& [icao, _] : AirportManager::getAirports())
	{
		AirportManager::update(icao, [&incompSids, &incompTrans, &icao](vsid::apt::AirportData& data)
			{
				for (vsid::Sid& sid : data.sids)
				{
					if (sid.designator != "")
					{
						if (std::string("0123456789").find_first_of(sid.number) == std::string::npos)
							incompSids[icao].insert(sid.base + '?' + sid.designator);
						else
						{
							for (auto& [_, trans] : sid.transition)
							{
								if (trans.number == "-1") trans.number = "";
								else if (std::string("0123456789").find_first_of(trans.number) == std::string::npos)
									incompTrans[icao][sid.base + sid.number + sid.designator].insert(trans.base + '?' + trans.designator);
							}
						}
					}
					else if (sid.number != "")
					{
						for (auto& [_, trans] : sid.transition)
						{
							if (trans.number == "-1") trans.number = "";
							else if (std::string("0123456789").find_first_of(trans.number) == std::string::npos)
								incompTrans[icao][sid.base + sid.number + sid.designator].insert(trans.base + '?' + trans.designator);
						}
					}
					else
					{
						if (sid.number != "X") incompSids[icao].insert(sid.base);
					}
				}
			});
	}

	// if incompatible SIDs (not in sector file) have been found remove them

	for (std::pair<const std::string, std::set<std::string>>& incompSidPair : incompSids)
	{
		vsid::Logger::log(
			LogLevel::Warning,
			std::format(
				"Check config for [{}] - Could not master sids with .ese file [{}]",
				incompSidPair.first,
				vsid::utils::join(incompSidPair.second, ", ")
			)
		);

		for (const std::string& incompSid : incompSidPair.second)
		{
			if (!AirportManager::isActive(incompSidPair.first)) continue; // #monitor - if incomp sids get deleted

			AirportManager::update(
				incompSidPair.first,
				[&incompSid, &incompTrans, &incompSidPair](vsid::apt::AirportData& data)
				{
					for (auto it = data.sids.begin(); it != data.sids.end();)
					{
						const auto& sid = *it;

						if (incompTrans.contains(incompSidPair.first) &&
							incompTrans[incompSidPair.first].contains(sid.base + sid.number + sid.designator) && // #refactor - prevent double lookup				
							sid.transition.empty())
						{
							vsid::Logger::log(
								LogLevel::Debug,
								std::format(
									"[{}] [{}{}{}] (ID: {}) lost all transitions and erased",
									incompSidPair.first,
									sid.waypoint,
									sid.number,
									sid.designator,
									sid.id
								),
								vsid::DebugLevel::Conf
							);

							it = data.sids.erase(it);
							continue;
						}

						if (!sid.designator.empty())
						{
							if (sid.waypoint == incompSid.substr(0, incompSid.length() - 2) && sid.designator == std::string(1, incompSid[incompSid.length() - 1]))
							{
								if (std::string("0123456789").find_first_of(sid.number) != std::string::npos)
								{
									vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}{}{}] (ID: {}) has a number. Skipping removal (other SID with same base failed to master)",
										incompSidPair.first, sid.waypoint, sid.number, sid.designator, sid.id), vsid::DebugLevel::Conf);

									++it;
									continue;
								}

								if (!sid.transition.empty())
								{
									vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}{}{}] (ID: {}) incompatible and erased. Transition present, double check them for validity.",
										incompSidPair.first, sid.waypoint, sid.number, sid.designator, sid.id), vsid::DebugLevel::Conf);
								}
								else
								{
									vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}{}{}] (ID: {}) incompatible and erased. No transition present.",
										incompSidPair.first, sid.waypoint, sid.number, sid.designator, sid.id), vsid::DebugLevel::Conf);
								}

								it = data.sids.erase(it);
								continue;
							}
						}
						else if (sid.number.empty() && sid.base == incompSid)
						{
							vsid::Logger::log(LogLevel::Debug, std::format("[{}] [{}] (ID: {}) (base only) incompatible and erased",
								incompSidPair.first, it->base, it->id), vsid::DebugLevel::Conf);

							it = data.sids.erase(it);
							continue;
						}
						++it;
					}
				});
		}
	}

	for (auto& [icao, sidMap] : incompTrans)
	{
		vsid::Logger::log(
			LogLevel::Warning,
			std::format("Check config for [{}] - Could not master following SIDs with transitions: ", icao)
		);

		for (auto& [sidName, transitions] : sidMap)
		{
			vsid::Logger::log(
				LogLevel::Warning,
				std::format("SID [{}] [{}]", sidName, vsid::utils::join(transitions, ", "))
			);
		}
	}

	// remove dummy value for SIDs without designator // #evaluate

	for (const auto& [icao, _] : AirportManager::getAirports())
	{
		AirportManager::update(icao, [](vsid::apt::AirportData& data)
			{
				for (vsid::Sid& sid : data.sids)
				{
					if (sid.number != "X") continue;

					sid.number = "";
				}
			});
	}

	FplnManager::reprocessAll();

	vsid::Logger::log(LogLevel::Info, std::format("Airports updated. [{}] active.", AirportManager::getAirports().size()));
}

void vsid::VSIDPlugin::OnTimer(int Counter)
{

	// #disabled - CCAMS; for further evaluation, CCAMS can't take in fast requests, delaying them can cause menus to be closed as ES "selects" a flightplan when triggered
	//if (this->sqwkQueue.size() > 0)
	//{
	//	messageHandler->writeMessage("DEBUG", "Calling CCAMS to squawk.", vsid::MessageHandler::DebugArea::Dev);
	//	try
	//	{
	//		std::string sqwkCallsign = *this->sqwkQueue.begin();
	//		messageHandler->writeMessage("DEBUG", "Extracted callsign " + sqwkCallsign, vsid::MessageHandler::DebugArea::Dev);
	//		this->callExtFunc(sqwkCallsign.c_str(), "CCAMS", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, sqwkCallsign.c_str(), "CCAMS", 871);
	//		this->sqwkQueue.erase(sqwkCallsign);
	//	}
	//	catch (std::out_of_range) {} // no error reporting, we just do nothing
	//}

	if (this->eseDataRdy_)
	{
		std::lock_guard<std::mutex> lock(this->bufferMtx_);

		if (this->eseBuffer_.has_value())
		{
			this->sectionAtc = std::move(this->eseBuffer_->sectionAtc);
			this->sectionSids = std::move(this->eseBuffer_->sectionSids);

			vsid::Logger::log(LogLevel::Info, "Updated ESE data from async buffer");

			UpdateActiveAirports();

			eseBuffer_.reset();
		}

		this->eseDataRdy_ = false;
	}

	std::vector<std::string> esLogs = vsid::Logger::fetchEsMsgs();

	for (const auto& msg : esLogs)
	{
		this->DisplayUserMessage("vSID", "vSID", msg.c_str(), true, true, true, false, false);
	}

	// get info msgs printed to the chat area of ES

	std::pair<std::string, std::string> msg = messageHandler->getMessage();
	/*auto [sender, msg] = messageHandler->getMessage();*/

	if (msg.first != "" && msg.second != "")
	{
		bool flash = (msg.first != "INFO") ? true : false;
		DisplayUserMessage("vSID", msg.first.c_str(), msg.second.c_str(), true, true, false, flash, false);
	}

	// check if we're still connected and clean up all flight plans if not

	if (this->GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_NO)
	{
		FplnManager::clear();

		for (const auto& [icao, _] : AirportManager::getAirports())
		{
			AirportManager::update(icao, [](vsid::apt::AirportData& data)
				{
					for (auto& [_, reqList] : data.requests)
					{
						reqList.clear();
					}
					for (auto& [_, reqList] : data.rwyrequests)
					{
						reqList.clear();
					}
				});		}

		SyncManager::clear();
	}

	// check squawk queue each second if new squawk can be set

	if (!this->squawkQueue.empty() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastSquawkTP).count() >= 2)
	{
		if (EuroScopePlugIn::CFlightPlan FlightPlan = this->FlightPlanSelectASEL(); FlightPlan.IsValid())
		{
			std::string callsign = this->squawkQueue.front();

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] working on squawk queue", callsign), vsid::DebugLevel::Dev);

			if (this->topskyLoaded && this->getConfigParser().preferTopsky)
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling TS Squawk func", callsign), vsid::DebugLevel::Dev);

				this->callExtFunc(callsign.c_str(), "TopSky plugin", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "TopSky plugin", 667, POINT(), RECT());

				this->lastSquawkTP = std::chrono::steady_clock::now();
			}
			else if (this->ccamsLoaded)
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling CCAMS Squawk func", callsign), vsid::DebugLevel::Dev);

				this->callExtFunc(callsign.c_str(), "CCAMS", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "CCAMS", 871, POINT(), RECT());

				this->lastSquawkTP = std::chrono::steady_clock::now();
			}

			this->SetASELAircraft(FlightPlan);

			this->squawkQueue.pop_front();
		}
	}

	if (Counter % 10 == 0)
	{
		std::unordered_map<std::string, bool> invalidFplns = {};

		for (const auto& [callsign, _] : FplnManager::getProcessed())
		{
			EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());
			std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();

			// mark invalid flight plans for removal

			if (!FlightPlan.IsValid())
			{
				invalidFplns.insert( {callsign, true} );

				continue;
			}

			// remove processed flight plans if outside of base vis range

			if (this->outOfVis(FlightPlan))
			{

				vsid::Logger::log(LogLevel::Debug, std::format("[{}] is further away than my range of NM [{}]",
					callsign, ControllerMyself().GetRange()), vsid::DebugLevel::Fpln);

				this->removeFromRequests(callsign, adep);

				if (vsid::fpln::findRemarks(FlightPlan, "VSID/RWY"))
				{
					vsid::fpln::removeRemark(FlightPlan, "VSID/RWY");

					if (!FlightPlan.GetFlightPlanData().AmendFlightPlan())
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
						{
							vsid::Logger::log(LogLevel::Error, std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND));

							messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
						}
					}
					else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
				}

				invalidFplns.insert({ callsign, false });

				messageHandler->removeCallsignFromErrors(callsign);
			}
		}

		for (auto& [callsign, isInvalid] : invalidFplns)
		{
			if(isInvalid) FplnManager::removeInvalid(callsign);
			else FplnManager::remove(callsign);
		}
	}

	// check internally removed flight plans every 30 seconds if they re-connected

	if (Counter % 30 == 0)
	{
		auto now = std::chrono::system_clock::now();
		std::set<std::string> invalidFplns = {};

		for (auto const& [callsign, data] : FplnManager::getProcessed())
		{
			EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());

			if (data.removalTime && FlightPlan.IsValid() && !this->outOfVis(FlightPlan))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] reconnected.", callsign), vsid::DebugLevel::Fpln);
				
				FplnManager::update(callsign, [](vsid::fpln::FplnData& cbData)
					{
						cbData.removalTime = std::nullopt;
					});

				continue;
			}

			if (data.removalTime && now > *data.removalTime)
			{
				std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();

				this->removeFromRequests(callsign, adep);

				vsid::Logger::log(LogLevel::Debug, std::format("[{}] exceeded disconnection time. Dropping", callsign), vsid::DebugLevel::Fpln);

				invalidFplns.insert(callsign);
			}
		}

		for (auto& callsign : invalidFplns)
		{
			FplnManager::remove(callsign);
		}
	}
}

void vsid::VSIDPlugin::deleteScreen(int id)
{
	vsid::Logger::log(LogLevel::Debug, std::format("(deleteScreen) size: {}", this->radarScreens.size()), vsid::DebugLevel::Menu);
	for (auto [id, screen] : this->radarScreens)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("(deleteScreen) present id [{}] is valid [{}]", id, this->radarScreens.at(id) ? "TRUE" : "FALSE"),
			vsid::DebugLevel::Menu);
	}

	if (this->radarScreens.contains(id))
	{
		vsid::Logger::log(LogLevel::Debug, std::format("(deleteScreen) Removing id [{}] use count [{}]", id, this->radarScreens.at(id).use_count()),
			vsid::DebugLevel::Menu);

		this->radarScreens.erase(id);
	}
	else
	{
		vsid::Logger::log(LogLevel::Debug, std::format("(deleteScreen) id [{}] is unknown.", id), vsid::DebugLevel::Menu);
	}

	vsid::Logger::log(LogLevel::Debug, std::format("(deleteScreen) size after deletion [{}]", this->radarScreens.size()), vsid::DebugLevel::Menu);
}

void vsid::VSIDPlugin::callExtFunc(const char* sCallsign, const char* sItemPlugInName, int ItemCode, const char* sItemString, const char* sFunctionPlugInName,
	int FunctionId, POINT Pt, RECT Area)
{
	if (this->radarScreens.size() > 0)
	{
		// check all avbl screens and use the first valid one

		for (const auto &[id, screen] : this->radarScreens)
		{
			if (screen)
			{
				screen->StartTagFunction(sCallsign, sItemPlugInName, ItemCode, sItemString, sFunctionPlugInName, FunctionId, Pt, Area);
				break;
			}
			else
			{
				vsid::Logger::log(LogLevel::Error, std::format("Couldn't call ext func for [{}] as the screen (id: {}) couldn't be called. Code: {}",
					sCallsign, id, ERROR_FPLN_EXTFUNC), vsid::DebugLevel::Menu);
			}
		}
	}
}

/*
* END ES FUNCTIONS
*/

void vsid::VSIDPlugin::exit()
{
	this->radarScreens.clear();
	if(this->curlInit) curl_global_cleanup();
	vsid::Logger::shutdown();

	this->shared.reset();
}

void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	vsid::crashhandler::initCrashHandler();
	// create the instance

	*ppPlugInInstance = vsidPlugin = new vsid::VSIDPlugin();
}


//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{
	vsid::crashhandler::removeCrashHandler();

	/* no deletion of vsidPlugin needed - ownership taken over by this->shared */
	vsidPlugin->exit();
}
