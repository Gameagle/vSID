#include "pch.h"

#include "vSIDPlugin.h"
#include "flightplan.h"
#include "timeHandler.h"
#include "messageHandler.h"
#include "area.h"
#include "Psapi.h"

#include <set>
#include <algorithm>

#include "display.h"
#include "airport.h"

// DEV
#include <thread>

#include <iostream> // for debugging in detectPlugins()
// END DEV

vsid::VSIDPlugin* vsidPlugin; // pointer needed for ES

vsid::VSIDPlugin::VSIDPlugin() : EuroScopePlugIn::CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, pluginName.c_str(), pluginVersion.c_str(), pluginAuthor.c_str(), pluginCopyright.c_str()) {
	this->detectPlugins();
	this->configParser.loadMainConfig();
	this->configParser.loadGrpConfig();
	this->configParser.loadRnavList();
	this->gsList = "STUP,PUSH,TAXI,DEPA";

	/* takes over pointer control of vsidPlugin - no deletion needed for unloading*/
	this->shared = std::shared_ptr<vsid::VSIDPlugin>(this);

	messageHandler->setLevel("INFO");
	
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

	this->loadEse(); // load and parse ese file

	UpdateActiveAirports(); // preload rwy settings

	DisplayUserMessage("Message", "vSID", std::string("Version " + pluginVersion + " loaded").c_str(), true, true, false, false, false);	
}

vsid::VSIDPlugin::~VSIDPlugin()
{
	messageHandler->writeMessage("DEBUG", "VSIDPlugin destroyed.", vsid::MessageHandler::DebugArea::Menu);
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
				std::string modname = szModName;

				//if (modname == "CCAMS.dll")
				if (modname.find("CCAMS.dll") != std::string::npos)
				{
					this->ccamsLoaded = true;
				}
				//if (modname == "TopSky.dll")
				if (modname.find("TopSky.dll") != std::string::npos)
				{
					this->topskyLoaded = true;
				}
			}
		}
	}
	CloseHandle(hprocess);
}

std::string vsid::VSIDPlugin::findSidWpt(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	std::string filedSid = FlightPlan.GetFlightPlanData().GetSidName();
	std::vector<std::string> filedRoute = vsid::utils::split(FlightPlan.GetFlightPlanData().GetRoute(), ' ');

	if (filedRoute.size() == 0) return "";
	if (!this->activeAirports.contains(adep)) return "";

	if (filedSid != "")
	{
		std::string esWpt = filedSid.substr(0, filedSid.length() - 2);
		for (const vsid::Sid& sid : this->activeAirports[adep].sids)
		{
			if (esWpt == sid.waypoint && std::any_of(filedRoute.begin(), filedRoute.end(), [&](std::string wpt)
				{
					try
					{
						if (esWpt == vsid::utils::split(wpt, '/').at(0)) return true;
						else return false;

						messageHandler->removeFplnError(callsign, ERROR_FPLN_SIDWPT);
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SIDWPT))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign +
								"] Failed to split waypoint during SID waypoint checking. Waypoint was: " + wpt +
								". Code: " + ERROR_FPLN_SIDWPT);

							messageHandler->addFplnError(callsign, ERROR_FPLN_SIDWPT);
						}
						return false;
					}
				})) return esWpt;
		}
	}

	// continue checks if esWpt hasn't been returned

	std::set<std::string> sidWpts = {};

	for (const vsid::Sid &sid : this->activeAirports[adep].sids)
	{
		if(sid.waypoint != "XXX") sidWpts.insert(sid.waypoint);

		for (auto& [base, _] : sid.transition)
		{
			sidWpts.insert(base);
		}
	}

	for (std::string &wpt : filedRoute)
	{
		if (wpt.find("/") != std::string::npos)
		{
			try
			{
				wpt = vsid::utils::split(wpt, '/').at(0);

				messageHandler->removeFplnError(callsign, ERROR_FPLN_SIDWPT);
			}
			catch (std::out_of_range)
			{
				if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SIDWPT))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to get the waypoint of a waypoint"
						" and speed/level group. Waypoint is: " + wpt + ". Code: " + ERROR_FPLN_SIDWPT);

					messageHandler->addFplnError(callsign, ERROR_FPLN_SIDWPT);
				}	
			}
		}
		if (std::any_of(sidWpts.begin(), sidWpts.end(), [&](std::string sidWpt)
			{
				return sidWpt == wpt;
			}))
		{
			return wpt;
		}
	}
	return "";
}

vsid::Sid vsid::VSIDPlugin::processSid(EuroScopePlugIn::CFlightPlan& FlightPlan, std::string atcRwy)
{
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string icao = fplnData.GetOrigin();
	std::string dest = fplnData.GetDestination();
	std::string callsign = FlightPlan.GetCallsign();
	std::vector<std::string> filedRoute = vsid::utils::split(std::string(fplnData.GetRoute()), ' ');
	std::string sidWpt = vsid::VSIDPlugin::findSidWpt(FlightPlan);
	vsid::Sid setSid = {};
	int prio = 99;
	bool customRuleActive = false;
	std::set<std::string> wptRules = {};
	std::set<std::string> actTSid = {};
	bool validEquip = true;

	if (!this->activeAirports.contains(icao))
	{
		messageHandler->writeMessage("DEBUG", "[" + icao + "] is not an active airport. Skipping all SID checks", vsid::MessageHandler::DebugArea::Sid);
		return vsid::Sid();
	}

	std::set<std::string> depRwys = this->activeAirports[icao].depRwys; // dep rwys to merge with arr rwys if needed

	// determine if a rule is active

	if (this->activeAirports[icao].customRules.size() > 0)
	{
		customRuleActive = std::any_of(
			this->activeAirports[icao].customRules.begin(),
			this->activeAirports[icao].customRules.end(),
			[](auto item)
			{
				return item.second;
			}
		);
	}

	if (customRuleActive) // #evaluate - arrrwy check for areas below ~ 325
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] [" + icao +
			"] Custom rules active. Checking for avbl areas and possible arr as dep rwys.",
			vsid::MessageHandler::DebugArea::Sid);

		std::map<std::string, bool> customRules = this->activeAirports[icao].customRules;

		for (auto it = this->activeAirports[icao].sids.begin(); it != this->activeAirports[icao].sids.end();)
		{
			if (it->customRule == "" ||
				(this->activeAirports[icao].customRules.contains(it->customRule) &&
					!this->activeAirports[icao].customRules[it->customRule]))
			{
				++it; continue;
			}

			if (it->area != "")
			{
				for (std::string area : vsid::utils::split(it->area, ','))
				{
					if (this->activeAirports[icao].areas.contains(area) &&
						this->activeAirports[icao].areas[area].isActive &&
						this->activeAirports[icao].areas[area].arrAsDep)
					{
						std::set<std::string> arrRwys = this->activeAirports[icao].arrRwys;
						depRwys.merge(arrRwys);
					}
				}
			}
			++it;
		}
	}

	// determining "night" SIDs (any SID with a set time frame)

	if (this->activeAirports[icao].settings["time"] &&
		this->activeAirports[icao].timeSids.size() > 0 &&
		std::any_of(this->activeAirports[icao].timeSids.begin(),
			this->activeAirports[icao].timeSids.end(),
			[&](auto item)
			{
				return sidWpt == item.waypoint;
			}
		))
	{
		for (const vsid::Sid& sid : this->activeAirports[icao].timeSids)
		{
			if (sid.waypoint != sidWpt) continue;
			if (vsid::time::isActive(this->activeAirports[icao].timezone, sid.timeFrom, sid.timeTo))
			{
				actTSid.insert(sid.waypoint);
			}
		}
	}

	// include atc set rwy if present as dep rwy

	std::string actAtcRwy = "";

	if (atcRwy != "" && this->processed.contains(callsign) && this->processed[callsign].atcRWY) actAtcRwy = atcRwy;
	else if (atcRwy != "" && !this->processed.contains(callsign) &&
		this->activeAirports[icao].settings["auto"] &&
		(vsid::fplnhelper::findRemarks(FlightPlan, "VSID/RWY") || fplnData.IsAmended())) actAtcRwy = atcRwy;

	for (vsid::Sid& currSid : this->activeAirports[icao].sids)
	{
		if (actAtcRwy != "") depRwys.insert(actAtcRwy);

		// skip if current SID does not match found SID wpt
		if (currSid.transition.empty() && currSid.waypoint != sidWpt && currSid.waypoint != "XXX") continue;
		else if (!currSid.transition.empty() && !currSid.transition.contains(sidWpt)) continue;

		bool rwyMatch = false;
		bool restriction = false;
		validEquip = true;


		// checking areas for arrAsDep - actual area evaluation down below

		if (currSid.area != "")
		{
			std::vector<std::string> sidAreas = vsid::utils::split(currSid.area, ',');

			if (std::any_of(sidAreas.begin(), sidAreas.end(), [&](auto sidArea) // #evaluate - wrong areas might result in an arr rwy becoming dep rwy
				{
					if (this->activeAirports[icao].areas.contains(sidArea) &&
						this->activeAirports[icao].areas[sidArea].isActive &&
						this->activeAirports[icao].areas[sidArea].inside(FlightPlan.GetFPTrackPosition().GetPosition()))
					{
						return true;
					}
					else return false;
				}))
			{
				for (std::string& area : sidAreas)
				{
					if (this->activeAirports[icao].areas.contains(area) && this->activeAirports[icao].areas[area].arrAsDep)
					{
						std::set<std::string> arrRwys = this->activeAirports[icao].arrRwys;
						depRwys.merge(arrRwys);
					}
				}
			}
		}

		messageHandler->writeMessage("DEBUG", "[" + callsign + "] Checking SID \"" + currSid.idName() + "\"", vsid::MessageHandler::DebugArea::Sid);

		// skip if SID needs active arr rwys

		if (!currSid.actArrRwy.empty())
		{
			if (currSid.actArrRwy.contains("allow"))
			{
				if (currSid.actArrRwy["allow"]["all"] != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["allow"]["all"], ',');
					if (!std::all_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (this->activeAirports[icao].arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
							"\" because not all of the required arr Rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
				else if (currSid.actArrRwy["allow"]["any"] != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["allow"]["any"], ',');
					if (std::none_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (this->activeAirports[icao].arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because none of the required arr rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
			}

			if (currSid.actArrRwy.contains("deny"))
			{
				if (currSid.actArrRwy["deny"]["all"] != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["deny"]["all"], ',');
					if (std::all_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (this->activeAirports[icao].arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
							"\" because all of the forbidden arr Rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
				else if (currSid.actArrRwy["deny"]["any"] != "")
				{
					std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["deny"]["any"], ',');
					if (std::any_of(actArrRwy.begin(), actArrRwy.end(), [&](std::string rwy)
						{
							if (this->activeAirports[icao].arrRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because at least one of the forbidden arr rwys is active",
							vsid::MessageHandler::DebugArea::Sid
						);
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
				if (currSid.actDepRwy["allow"]["all"] != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["allow"]["all"], ',');
					if (!std::all_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because not all of the required dep Rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
				else if (currSid.actDepRwy["allow"]["any"] != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["allow"]["any"], ',');
					if (std::none_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because none of the required dep rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
			}

			if (currSid.actDepRwy.contains("deny"))
			{
				if (currSid.actDepRwy["deny"]["all"] != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["deny"]["all"], ',');
					if (std::all_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because all of the forbidden dep Rwys are active",
							vsid::MessageHandler::DebugArea::Sid
						);
						continue;
					}
				}
				else if (currSid.actDepRwy["deny"]["any"] != "")
				{
					std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["deny"]["any"], ',');
					if (std::any_of(actDepRwy.begin(), actDepRwy.end(), [&](std::string rwy)
						{
							if (depRwys.contains(rwy)) return true;
							else return false;
						}))
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
							currSid.idName() + "\" because at least one of the forbidden dep rwys is active",
							vsid::MessageHandler::DebugArea::Sid
						);
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
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] [" + icao + "] available rwys (merged if arrAsDep in active area or rwy set by atc): " +
			vsid::utils::join(depRwys),
			vsid::MessageHandler::DebugArea::Sid
		);
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] [" + icao + "] Skipped ATC RWYs : \"" +
			vsid::utils::join(skipAtcRWY) + "\" / skipped SID RWY: \"" +
			vsid::utils::join(skipSidRWY) + "\"", vsid::MessageHandler::DebugArea::Sid
		);

		// skip if custom rules are active but the current sid has no rule or has a rule but this is not active

		if (customRuleActive && currSid.customRule != "" &&
			this->activeAirports[icao].customRules.contains(currSid.customRule) &&
			!this->activeAirports[icao].customRules[currSid.customRule])
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
				currSid.idName() + "\" because the SID custom rule is not active.",

				vsid::MessageHandler::DebugArea::Sid);
			continue;
		}

		// skip if aircraft type does not match (heli, fixed wing) - unknown aircraft is never skipped

		if (currSid.wingType.find(fplnData.GetAircraftType()) == std::string::npos && fplnData.GetAircraftType() != '?')
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because wing type \"" + fplnData.GetAircraftType() + "\" doesn't match.",
				vsid::MessageHandler::DebugArea::Sid);

			continue;
		}

		// skip if equipment does not match

		if (this->activeAirports[icao].equipCheck)
		{
			std::string equip = vsid::fplnhelper::getEquip(FlightPlan, this->configParser.rnavList);
			std::string pbn = vsid::fplnhelper::getPbn(FlightPlan);

			if (currSid.equip.contains("RNAV"))
			{
				if (equip.size() > 1 && equip.find_first_of("ABGRI") == std::string::npos && pbn == "")
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
						"\" because RNAV ('A', 'B', 'G', 'R' or 'I') is required, but not found in equipment \"" +
						equip + "\" and PBN is empty", vsid::MessageHandler::DebugArea::Sid);
					validEquip = false;
					continue;
				}
			}
			else if (currSid.equip.contains("NON-RNAV"))
			{
				if (equip.size() < 2 && pbn == "")
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
						"\" because only NON-RNAV is allowed, but equipment \"" + equip +
						"\" has less than 2 entry and PBN is empty", vsid::MessageHandler::DebugArea::Sid);
					validEquip = false;
					continue;
				}

				/*if (equip.find_first_of("GRI") != std::string::npos || equip == "" || pbn != "")*/
				if (equip.find_first_of("GRI") != std::string::npos || pbn != "")
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
						"\" because only NON-RNAV is allowed, but RNAV ('G', 'R' or 'I') was found in equipment \"" + equip +
						"\" or equipment was empty and PBN is not empty", vsid::MessageHandler::DebugArea::Sid);
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
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
							"\" because equipment \"" + sidEquip.first + "\" is mandatory but was not found in equipment \"" + equip + "\"",
							vsid::MessageHandler::DebugArea::Sid);
						validEquip = false;
						continue;
					}
					if (!sidEquip.second && equip.find(sidEquip.first) != std::string::npos)
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
							"\" because equipment \"" + sidEquip.first + "\" is forbidden but was found in equipment \"" + equip + "\"",
							vsid::MessageHandler::DebugArea::Sid);
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
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName()
					+ "\" because none of the transitions found in the route",
					vsid::MessageHandler::DebugArea::Dev);
			}
		}

		// skip if custom rules are inactive but a rule exists in sid

		if (!customRuleActive && currSid.customRule != "")
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because no rule is active and the SID has a rule configured",
				vsid::MessageHandler::DebugArea::Sid
			);
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
					if (this->activeAirports[icao].areas.contains(sidArea))
					{
						if (!this->activeAirports[icao].areas[sidArea].isActive) return true;
						else return false;
					}
					else return false; // warning for wrong config in next area check to prevent doubling
				}))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
					"\" because all areas are inactive but one is set in the SID",
					vsid::MessageHandler::DebugArea::Sid
				);
				continue;
			}

			// skip if area is active + fpln outside
			if (std::none_of(sidAreas.begin(),
				sidAreas.end(),
				[&](auto sidArea)
				{
					if (this->activeAirports[icao].areas.contains(sidArea))
					{
						if (!this->activeAirports[icao].areas[sidArea].isActive ||
							(this->activeAirports[icao].areas[sidArea].isActive &&
								!this->activeAirports[icao].areas[sidArea].inside(FlightPlan.GetFPTrackPosition().GetPosition()))
							) return false;
						else return true;
					}
					else
					{
						messageHandler->writeMessage("WARNING", "Area \"" + sidArea + "\" not in config for " + icao + ". Check your config.");
						return false;
					} // fallback
				}))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
					"\" because the plane is not in one of the active areas",
					vsid::MessageHandler::DebugArea::Sid
				);
				continue;
			}
		}

		// skip if lvp ops are active but SID is not configured for lvp ops and lvp is not disabled for SID
		if (this->activeAirports[icao].settings["lvp"] && !currSid.lvp && currSid.lvp != -1)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because LVP is active and SID is not configured for LVP or LVP check is not disabled for SID",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}

		// skip if lvp ops are inactive but SID is configured for lvp ops

		if (!this->activeAirports[icao].settings["lvp"] && currSid.lvp && currSid.lvp != -1)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because LVP is inactive and SID is configured for LVP or LVP check is not disabled for SID",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}

		// skip if no matching rwy was found in SID;

		if (!rwyMatch)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" due to a RWY mismatch between SID rwy and active DEP rwy",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}


		// skip if engine type doesn't match

		if (currSid.engineType != "" && currSid.engineType.find(fplnData.GetEngineType()) == std::string::npos)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because of a mismatch in engineType",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}
		else if (currSid.engineType != "")
		{
			restriction = true;
		}

		// skip if an aircraft type is set in sid but is set to false

		if ((currSid.acftType.contains(fplnData.GetAircraftFPType()) &&
			!currSid.acftType[fplnData.GetAircraftFPType()]) ||
			std::any_of(currSid.acftType.begin(), currSid.acftType.end(), [&](auto type)
				{
					return type.second && !currSid.acftType.contains(fplnData.GetAircraftFPType());
				}))
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because of a mismatch in aircraft type (type is set to false" +
				" or type is set to true but plane is not of the type)",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}
		else if (!currSid.acftType.empty())
		{
			restriction = true;
		}

		// skip if SID has engineNumber requirement and acft doesn't match
		if (!vsid::utils::containsDigit(currSid.engineCount, fplnData.GetEngineNumber()))
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because of a mismatch in engine number",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}
		else if (currSid.engineCount > 0)
		{
			restriction = true;
		}

		// skip if SID has WTC requirement and acft doesn't match
		if (currSid.wtc != "" && currSid.wtc.find(fplnData.GetAircraftWtc()) == std::string::npos)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because of mismatch in WTC",
				vsid::MessageHandler::DebugArea::Sid
			);
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
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because of MTOW",
					vsid::MessageHandler::DebugArea::Sid
				);
				continue;
			}
			else restriction = true;
		}

		// skip if destination restrictions present and flight plan doesn't match

		if (!currSid.dest.empty())
		{
			if (currSid.dest.contains(dest) && !currSid.dest[dest])
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because destination \"" +
					dest + "\" is not allowed", vsid::MessageHandler::DebugArea::Sid
				);
				continue;
			}
			else if (!currSid.dest.contains(dest) && std::any_of(currSid.dest.begin(), currSid.dest.end(), [](const std::pair<std::string, bool>& sidDest)
				{
					return sidDest.second;
				}))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because destination \"" +
					dest + "\" was not found for this SID and the SID has mandatory destinations",
					vsid::MessageHandler::DebugArea::Sid);
				continue;
			}

		}

		// skip if route restrictions present and flight plan doesn't match

		if (!currSid.route.empty())
		{
			if (currSid.route.contains("allow"))
			{
				bool validRoute = false;
				for (const auto& [_, route] : currSid.route["allow"])
				{
					std::vector<std::string>::iterator startPos;

					try
					{
						startPos = std::find(filedRoute.begin(), filedRoute.end(), route.at(0));

						if (startPos == filedRoute.end()) messageHandler->writeMessage("DEBUG", "[" + callsign + "] skipping SID \"" +
							currSid.idName() + "\" route checking for " + vsid::utils::join(route, ' ') +
							" because first mandatory wpt " + route.at(0) +
							" was not found in route", vsid::MessageHandler::DebugArea::Sid);

						messageHandler->removeFplnError(callsign, ERROR_FPLN_ALLOWROUTE);
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_ALLOWROUTE))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to get first position for allowed route in SID " +
								currSid.idName() + " checking SID route \"" + vsid::utils::join(route, ' ') + "\". Code: " + ERROR_FPLN_ALLOWROUTE);
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

							messageHandler->writeMessage("DEBUG", "[" + callsign + "] checking mand. wpt " + wpt +
								" vs. " + *it, vsid::MessageHandler::DebugArea::Sid);
							if (wpt == *it)
							{
								allowSkip = false;
								lastMatch = true;
								startPos++;
								break;
							}
							else if (allowSkip)
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] skipping wpt " + *it +
									" as skipping is allowed", vsid::MessageHandler::DebugArea::Sid);
								lastMatch = false;
								it++;
								startPos++;
								continue;
							}
							else
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] mismatch between mand. wpt " +
									wpt + " vs. " + *it + ". Aborting", vsid::MessageHandler::DebugArea::Sid);
								lastMatch = false;
								stopRoute = true;
								break;
							}
						}
					}

					if (lastMatch)
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] mand. route in " + currSid.idName() +
							" was found. Accepting.", vsid::MessageHandler::DebugArea::Sid);
						validRoute = true;
						break;
					}
				}
				if (!validRoute)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] skipping SID \"" +
						currSid.idName() + "\" because none of the allowed routes matched", vsid::MessageHandler::DebugArea::Sid);

					continue;
				}
			}

			if (currSid.route.contains("deny"))
			{
				bool invalidRoute = false;

				for (const auto& [_, route] : currSid.route["deny"])
				{
					std::vector<std::string>::iterator startPos;

					try
					{
						startPos = std::find(filedRoute.begin(), filedRoute.end(), route.at(0));

						if (startPos == filedRoute.end())
						{
							messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepting SID " +
								currSid.idName() + " because first waypoint of denied route wasn't found", vsid::MessageHandler::DebugArea::Sid);

							messageHandler->removeFplnError(callsign, ERROR_FPLN_DENYROUTE);

							break;
						}
					}
					catch (std::out_of_range)
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_DENYROUTE))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to get first position for denied route in SID " +
								currSid.idName() + " checking SID route \"" + vsid::utils::join(route, ' ') + "\". Code: " + ERROR_FPLN_DENYROUTE);
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

							messageHandler->writeMessage("DEBUG", "[" + callsign + "] checking forb. wpt " + wpt +
								" vs. " + *it, vsid::MessageHandler::DebugArea::Sid);
							if (wpt == *it)
							{
								allowSkip = false;
								lastMatch = true;
								startPos++;
								break;
							}
							else if (allowSkip)
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] skipping wpt " + *it +
									" as it is allowed", vsid::MessageHandler::DebugArea::Sid);

								lastMatch = false;
								it++;
								startPos++;
								continue;
							}
							else
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] mismatch between forb. wpt " +
									wpt + " vs. " + *it + ". Aborting", vsid::MessageHandler::DebugArea::Sid);
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
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] skipping SID \"" +
						currSid.idName() + "\" because a denied route matched", vsid::MessageHandler::DebugArea::Sid);

					continue;
				}
				else messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepting SID \"" +
					currSid.idName() + "\" because no denied route matched", vsid::MessageHandler::DebugArea::Sid);
			}
		}

		// skip if sid has night times set but they're not active
		if (!actTSid.contains(currSid.waypoint) &&
			(currSid.timeFrom != -1 || currSid.timeTo != -1)
			)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because time is set and not active",
				vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}

		// if a SID has the special prio "0" return an empty SID for forced manual selection
		if (currSid.prio == 0)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] special prio value '0' detected. Returning empty SID for forced manual mode",
				vsid::MessageHandler::DebugArea::Sid);

			if (this->processed.contains(callsign)) this->processed[callsign].validEquip = true;
			return vsid::Sid();
		}

		// if a SID is accepted when filed by a pilot set the SID
		if (currSid.pilotfiled && currSid.name() == fplnData.GetSidName() && currSid.prio < prio)
		{
			setSid = currSid;
			prio = currSid.prio;
		}
		else if (currSid.pilotfiled) messageHandler->writeMessage("DEBUG", "[" + callsign + "] Ignoring SID \"" + currSid.idName() +
			"\" because it is only accepted as pilot filed and the prio is higher or the filed SID was another",
			vsid::MessageHandler::DebugArea::Sid
		);
		if (currSid.prio < prio && (setSid.pilotfiled == currSid.pilotfiled || restriction))
		{
			setSid = currSid;
			prio = currSid.prio;
		}
		else if (currSid.prio == 99)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
				"\" because prio is 99 (manual only SID)",
				vsid::MessageHandler::DebugArea::Sid);
		}
		else messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
			"\" because prio is higher",
			vsid::MessageHandler::DebugArea::Sid);
	}
	messageHandler->writeMessage("DEBUG", "[" + callsign + "] Setting SID \"" + setSid.idName() + "\"", vsid::MessageHandler::DebugArea::Sid);

	// if the last valid SID fails due to equipment return a special "EQUIP" sid to also handle yet unprocessed fplns
	// reset in processFlightplan()
	if (!validEquip && setSid.empty())
	{
		setSid.base = "EQUIP";
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] Re-Setting special SID base 'EQUIP' as the last possible SID failed due to equipment checks",
			vsid::MessageHandler::DebugArea::Sid);
	}

	return(setSid);
}

void vsid::VSIDPlugin::processFlightplan(EuroScopePlugIn::CFlightPlan& FlightPlan, bool checkOnly, std::string atcRwy, vsid::Sid manualSid)
{
	if (!FlightPlan.IsValid()) return;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string icao = fplnData.GetOrigin();
	std::string filedSidWpt = this->findSidWpt(FlightPlan);
	std::vector<std::string> filedRoute = vsid::fplnhelper::clean(FlightPlan, filedSidWpt);
	vsid::Sid sidSuggestion = {};
	vsid::Sid sidCustomSuggestion = {};
	std::string setRwy = "";
	vsid::Fpln fpln = {};
	bool resetIC = false;

	if (!this->activeAirports.contains(icao))
	{
		if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_ADEPINACTIVE))
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] ADEP [" + icao +
				"] is not an active airport. Aborting processing. Code: " + ERROR_FPLN_ADEPINACTIVE);
			messageHandler->addFplnError(callsign, ERROR_FPLN_ADEPINACTIVE);
		}

		return;
	}
	else messageHandler->removeFplnError(callsign, ERROR_FPLN_ADEPINACTIVE);

	// restore infos after an airport update was called but only for first processing

	if (!this->processed.contains(callsign) &&
		vsid::fplnhelper::restoreFplnInfo(callsign, this->processed, this->savedFplnInfo))
	{
		fpln = this->processed[callsign];
		resetIC = true;

		messageHandler->writeMessage("DEBUG", "[" + callsign + "] re-syncing req and states for reconnected flight plan.",
			vsid::MessageHandler::DebugArea::Fpln);

		// sync requests - sync function requires fpln to be known in one of the lists

		if (fpln.request != "" && fpln.reqTime != -1)
		{
			std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(fpln.reqTime);

			messageHandler->writeMessage("DEBUG", "[" + callsign + "] syncing " + fpln.request +
				" with scratch. New: \"" + newScratch + "\" | Old: \"" + FlightPlan.GetControllerAssignedData().GetScratchPadString() + "\"", vsid::MessageHandler::DebugArea::Req
			);
			this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
		}

		this->syncStates(FlightPlan);

		std::string ades = fplnData.GetDestination();

		if (this->activeAirports.contains(ades) && fpln.ctl)
		{
			std::string newScratch = ".VSID_CTL_TRUE";
			this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
		}
	}
	else if(this->processed.contains(callsign))
	{
		fpln = this->processed[callsign];

		fpln.sid = {};
		fpln.customSid = {};
		fpln.sidWpt = "";
		fpln.transition = "";
		fpln.validEquip = true;
	}

	// save the SID waypoint with each processing for later evaluation (e.g. SID tagItem)

	fpln.sidWpt = filedSidWpt;

	/* if a sid has been set manually choose this */

	if (std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V")
	{
		if (!manualSid.empty())
		{
			sidCustomSuggestion = manualSid;
		}
	}
	else if (!manualSid.empty())
	{
		sidSuggestion = this->processSid(FlightPlan);
		sidCustomSuggestion = manualSid;

		if (atcRwy != "" && vsid::utils::contains(sidSuggestion.rwys, atcRwy) && sidSuggestion == sidCustomSuggestion)
		{
			sidCustomSuggestion = {};
		}
	}
	/* if a rwy is given by atc check for a sid for this rwy and for a normal sid
	* to be then able to compare those two
	*/
	else if (atcRwy != "")
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] processing SID without atcRWY (atcRWY present, will be next check)", vsid::MessageHandler::DebugArea::Sid);
		sidSuggestion = this->processSid(FlightPlan);
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] processing SID with atcRWY (for customSuggestion): " + atcRwy, vsid::MessageHandler::DebugArea::Sid);
		sidCustomSuggestion = this->processSid(FlightPlan, atcRwy);
	}
	/* default state */
	else
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] processing SID without atcRWY", vsid::MessageHandler::DebugArea::Sid);
		sidSuggestion = this->processSid(FlightPlan);
	}

	// reset special 'EQUIP' SID back to empty

	if (sidSuggestion.base == "EQUIP")
	{
		fpln.validEquip = false;
		sidSuggestion.base = "";
	}

	if (sidCustomSuggestion.base == "EQUIP")
	{
		fpln.validEquip = false;
		sidCustomSuggestion.base = "";
	}

	// if a custom sid has already been detected but evaluation fails (e.g. old airac entry)
	// preserve data previously pulled from the flight plan (suggestion value for other checks below)

	if (this->processed.contains(callsign))
	{
		if (!this->processed[callsign].customSid.empty() && sidSuggestion.empty() && sidCustomSuggestion.empty() &&
			std::string(fplnData.GetRoute()).find(this->processed[callsign].customSid.name()) != std::string::npos)
		{
			sidCustomSuggestion = this->processed[callsign].customSid;
		}
	}

	// determine dep rwy based on suggested SIDs

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		try
		{
			std::string rwy;
			if (atcRwy != "" && vsid::utils::contains(sidSuggestion.rwys, atcRwy)) rwy = atcRwy;
			else
			{
				bool arrAsDep = false;
				std::string& area = sidSuggestion.area;

				if (area != "" && this->activeAirports[icao].areas.contains(area) && this->activeAirports[icao].areas[area].isActive &&
					this->activeAirports[icao].areas[area].inside(FlightPlan.GetFPTrackPosition().GetPosition()))
				{
					arrAsDep = this->activeAirports[icao].areas[area].arrAsDep;
				}

				for (const std::string& sidRwy : sidSuggestion.rwys)
				{
					if (this->activeAirports[icao].isDepRwy(sidRwy, arrAsDep))
					{
						rwy = sidRwy;
						break;
					}
				}
			}

			if (rwy != "") setRwy = rwy;
			else
			{
				messageHandler->writeMessage("DEBUG", "Fall back to ES dep rwy for [" + callsign + "] in fpln processing for sidSuggestion",
											vsid::MessageHandler::DebugArea::Sid);
				setRwy = fplnData.GetDepartureRwy();
			}
		}
		catch (std::out_of_range) // old remains - might be removed #checkforremoval
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to check RWY in sidSuggestion. Check config " +
										icao + " for SID \"" + sidSuggestion.idName() + "\". RWY value is: " + vsid::utils::join(sidSuggestion.rwys));
		}
	}
	else if (sidCustomSuggestion.base != "")
	{
		try
		{
			std::string rwy;

			if (atcRwy != "" && vsid::utils::contains(sidCustomSuggestion.rwys, atcRwy)) rwy = atcRwy;
			else
			{

				bool arrAsDep = false;
				std::string& area = sidCustomSuggestion.area;

				if (area != "" && this->activeAirports[icao].areas.contains(area) && this->activeAirports[icao].areas[area].isActive &&
					this->activeAirports[icao].areas[area].inside(FlightPlan.GetFPTrackPosition().GetPosition()))
				{
					arrAsDep = this->activeAirports[icao].areas[area].arrAsDep;
				}

				for (const std::string& sidRwy : sidSuggestion.rwys)
				{
					if (this->activeAirports[icao].isDepRwy(sidRwy, arrAsDep))
					{
						rwy = sidRwy;
						break;
					}
				}
			}

			if (rwy != "") setRwy = rwy;
			else
			{
				messageHandler->writeMessage("DEBUG", "Fall back to ES dep rwy for [" + callsign + "] in fpln processing for sidCustomSuggestion",
					vsid::MessageHandler::DebugArea::Sid);
				setRwy = fplnData.GetDepartureRwy();
			}
		}
		catch (std::out_of_range) // old remains - might be removed #checkforremoval
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to check RWY in sidCustomSuggestion. Check config " +
										icao + " for SID \"" +	sidCustomSuggestion.idName() + "\". RWY value is: " + vsid::utils::join(sidCustomSuggestion.rwys));
		}
	}
	
	// building a new route with the selected sid

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		std::ostringstream ss;
		ss << sidSuggestion.name();
		if (std::string transition = vsid::fplnhelper::getTransition(FlightPlan, sidSuggestion.transition, filedSidWpt); transition != "")
		{
			ss << "x" << transition;
		}
		ss << "/" << setRwy;
		filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
	}
	else if (sidCustomSuggestion.base != "")
	{
		std::ostringstream ss;
		ss << sidCustomSuggestion.name();
		if (std::string transition = vsid::fplnhelper::getTransition(FlightPlan, sidCustomSuggestion.transition, filedSidWpt); transition != "")
		{
			ss << "x" << transition;
		}
		ss << "/" << setRwy;
		filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
	}

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		fpln.sid = sidSuggestion;
		fpln.transition = vsid::fplnhelper::getTransition(FlightPlan, sidSuggestion.transition, filedSidWpt);
	}
	else if (sidCustomSuggestion.base != "")
	{
		fpln.sid = sidSuggestion;
		fpln.customSid = sidCustomSuggestion;
		fpln.transition = vsid::fplnhelper::getTransition(FlightPlan, sidCustomSuggestion.transition, filedSidWpt);
	}

	// if the fpln was already processed update values to prevent overwriting
	if (this->processed.contains(callsign))
	{
		fpln.atcRWY = this->processed[callsign].atcRWY;
		fpln.request = this->processed[callsign].request;
		fpln.reqTime = this->processed[callsign].reqTime;
		fpln.noFplnUpdate = this->processed[callsign].noFplnUpdate;
	}

	this->processed[callsign] = std::move(fpln);

	// if an IFR fpln has no matching sid but the route should be set inverse - otherwise rwy changes would be overwritten
	if (!checkOnly && std::string(fplnData.GetPlanType()) == "I" &&
		sidSuggestion.empty() && sidCustomSuggestion.empty()) checkOnly = true;

	if (!checkOnly && this->processed.contains(callsign))
	{	
		this->processed[callsign].noFplnUpdate = true;
		if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to change flight plan! Code: " + ERROR_FPLN_SETROUTE);
			this->processed[callsign].noFplnUpdate = false;
		}
		//else this->processed[callsign].noFplnUpdate = true;

		if (!fplnData.AmendFlightPlan())
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! Code: " + ERROR_FPLN_AMEND);
			this->processed[callsign].noFplnUpdate = false;
		}
		else
		{
			if (this->activeAirports[fplnData.GetOrigin()].settings["auto"])
			{
				std::string newScratch = ".vsid_auto_" + std::string(ControllerMyself().GetCallsign());
				this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
			}
			this->processed[callsign].atcRWY = true;
		}

		if (sidSuggestion.base != "" && sidCustomSuggestion.base == "" && sidSuggestion.initialClimb) // #evaluate - custom kept if same as std - might remove one branch
		{
			int initialClimb = (sidSuggestion.initialClimb > fplnData.GetFinalAltitude()) ? fplnData.GetFinalAltitude() : sidSuggestion.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude. Code: " + ERROR_FPLN_SETALT);
			}
		}
		else if (sidCustomSuggestion.base != "" && sidCustomSuggestion.initialClimb)
		{
			int initialClimb = (sidCustomSuggestion.initialClimb > fplnData.GetFinalAltitude()) ? fplnData.GetFinalAltitude() : sidCustomSuggestion.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude. Code: " + ERROR_FPLN_SETALT);
			}
		}

		std::string squawk = FlightPlan.GetControllerAssignedData().GetSquawk();
		if ((squawk == "" || squawk == "0000" || squawk == "1234") && !this->activeAirports[icao].settings["auto"])
		{
			if (this->ccamsLoaded && !this->getConfigParser().preferTopsky)
			{
				// DEV - see OnTime for further usage
				// this->sqwkQueue.insert(callsign);
				// END DEV
				
				this->callExtFunc(callsign.c_str(), "CCAMS", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "CCAMS", 871, POINT(), RECT());

				messageHandler->writeMessage("DEBUG", "[" + callsign + "] calling CCAMS squawk", vsid::MessageHandler::DebugArea::Dev);
			}
			else if (this->topskyLoaded)
			{
				this->callExtFunc(callsign.c_str(), "TopSky plugin", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "TopSky plugin", 667, POINT(), RECT());

				messageHandler->writeMessage("DEBUG", "[" + callsign + "] calling TopSky squawk", vsid::MessageHandler::DebugArea::Dev);
			}
		}
	}

	// reset IC if it doesn't match

	if (resetIC && this->processed.contains(callsign)) vsid::fplnhelper::restoreIC(this->processed[callsign], FlightPlan, ControllerMyself());
}

void vsid::VSIDPlugin::removeFromRequests(const std::string& callsign, const std::string& icao)
{
	if (this->activeAirports.contains(icao))
	{
		for (auto it = this->activeAirports[icao].requests.begin(); it != this->activeAirports[icao].requests.end(); ++it)
		{
			for (std::set<std::pair<std::string, long long>>::iterator jt = it->second.begin(); jt != it->second.end();)
			{
				if (jt->first != callsign)
				{
					++jt;
					continue;
				}
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] erasing from request \"" + it->first +
					"\" at [" + icao + "]", vsid::MessageHandler::Req);

				if (this->processed.contains(callsign))
				{
					this->processed[callsign].request = "";
					this->processed[callsign].reqTime = -1;
				}
				
				jt = it->second.erase(jt);
			}
		}

		for (auto& [type, rwys] : this->activeAirports[icao].rwyrequests)
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
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] erasing from rwy request \"" + type + "\" at [" + icao + "] in rwy \"" +
						it->first + "\"", vsid::MessageHandler::Req);

					if (this->processed.contains(callsign))
					{
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
					}
					
					jt = it->second.erase(jt);
				}
			}
		}
	}
}

void vsid::VSIDPlugin::syncReq(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();

	messageHandler->writeMessage("DEBUG", "[" + callsign + "] calling request sync.", vsid::MessageHandler::DebugArea::Req);

	if (!this->processed.contains(callsign) || !this->activeAirports.contains(adep)) return;

	vsid::Fpln& fpln = this->processed[callsign];

	if (fpln.request != "")
	{
		// sync rwy requests - parallel req lists are already managed on scratchpad updates
		if (this->activeAirports[adep].rwyrequests.contains(fpln.request))
		{
			bool stop = false;
			for (auto& [rwy, reqRwy] : this->activeAirports[adep].rwyrequests[fpln.request])
			{
				for (auto& [reqCallsign, reqTime] : reqRwy)
				{
					if (reqCallsign != callsign) continue;

					std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(reqTime);

					this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());

					stop = true;
					break;
				}
				if (stop) break;
			}
		}
		// sync normal requests
		else if (this->activeAirports[adep].requests.contains(fpln.request))
		{
			for (auto& [reqCallsign, reqTime] : this->activeAirports[adep].requests[fpln.request])
			{
				if (reqCallsign != callsign) continue;

				std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(reqTime);

				this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());

				break;
			}
		}
	}
}

void vsid::VSIDPlugin::syncStates(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	std::string callsign = FlightPlan.GetCallsign();

	if (this->processed.contains(callsign))
	{
		this->addSyncQueue(callsign, (FlightPlan.GetClearenceFlag() ? "CLEA" : "NOTC"), FlightPlan.GetControllerAssignedData().GetScratchPadString());
		if (this->processed[callsign].gndState != "")
		{
			this->addSyncQueue(callsign, this->processed[callsign].gndState, FlightPlan.GetControllerAssignedData().GetScratchPadString());
		}
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
				if (myCallsign == atc.callsign || myFreq == atc.freq)
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

void vsid::VSIDPlugin::processSPQueue()
{
	if (queueInProcess || this->syncQueue.empty()) return;
	messageHandler->writeMessage("DEBUG", "Started sync processing queue...", vsid::MessageHandler::DebugArea::Dev);

	queueInProcess = true;

	for (std::unordered_map<std::string, std::deque<std::pair<std::string, std::string>>>::iterator it = this->syncQueue.begin(); it != this->syncQueue.end();)
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
			std::pair<std::string, std::string> scratchPair = std::move(it->second.front());
			it->second.pop_front();

			// #evaluate - removing double entries - + 8 lines below

			size_t pos = 0;
			for (auto jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				if (jt->first == scratchPair.first && jt->second == scratchPair.second) ++pos;
			}

			if (pos != 0 && pos <= it->second.size())
			{
				it->second.erase(it->second.begin(), it->second.begin() + pos);
			}

			messageHandler->writeMessage("DEBUG", "[" + callsign + "] #1 (Queue) sync released... " + ((this->spReleased[callsign]) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Dev);

			this->spReleased[callsign] = false;
			spWorkerActive = true; // reset on received update

			messageHandler->writeMessage("DEBUG", "[" + callsign + "] (Queue) scratch pad sending scratch. New: \"" +
				scratchPair.first + "\" | Old: " + scratchPair.second + "\"", vsid::MessageHandler::DebugArea::Dev);

			FlightPlan.GetControllerAssignedData().SetScratchPadString(vsid::utils::trim(scratchPair.first).c_str());
			FlightPlan.GetControllerAssignedData().SetScratchPadString(vsid::utils::trim(scratchPair.second).c_str());

			// pre-release sync lock as mentioned ES msgs are never received
			
			if (scratchPair.first == "CLEA" || scratchPair.first == "NOTC") this->updateSPSyncRelease(callsign);
		}
		else if (this->spReleased.contains(callsign) && !this->spReleased[callsign]) // #dev - sync - remove (only debugging)
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
			this->spReleased.erase(callsign);
			continue;
		}

		++it;
	}
	queueInProcess = false;
	messageHandler->writeMessage("DEBUG", "Finished sync processing queue...", vsid::MessageHandler::DebugArea::Dev);
}

void vsid::VSIDPlugin::loadEse()
{
	json& vSidConfig = this->configParser.getMainConfig();

	if (vSidConfig.is_null())
	{
		messageHandler->writeMessage("ERROR", "Failed to parse main config. (Critical!)");
		return;
	}
		
	if (!vSidConfig.contains("esePath"))
	{
		messageHandler->writeMessage("ERROR", "Config value esePath is missing. (Critical!).");
		return;
	}

	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;
	std::string esePath = vSidConfig.at("esePath");
	std::filesystem::path fullEsePath;

	basePath.append(esePath).make_preferred();
	
	try
	{
		for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
		{
			if (!std::filesystem::is_directory(entry) && entry.extension() == ".ese")
			{
				fullEsePath = entry;
				fullEsePath = std::filesystem::canonical(fullEsePath.make_preferred());
			}
		}

		if (fullEsePath.empty())
		{
			messageHandler->writeMessage("ERROR", "Couldn't find .ese file. Checked in: " + basePath.lexically_normal().string());
			return;
		}
	}
	catch (std::filesystem::filesystem_error& e)
	{
		messageHandler->writeMessage("ERROR", "Failed to validate ese path: " + std::string(e.what()));
	}
	

	vsid::EseParser eseParser(this->sectionAtc, this->sectionSids);
	eseParser.parseEse(fullEsePath);
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

	messageHandler->writeMessage("DEBUG", "Screen created with id: " + std::to_string(this->screenId), vsid::MessageHandler::DebugArea::Menu);
	for (auto [id, screen] : this->radarScreens)
	{
		messageHandler->writeMessage("DEBUG", "radarScreens id: " + std::to_string(id) + " screen valid: " + 
			((this->radarScreens.at(id)) ? "true" : "false"), vsid::MessageHandler::DebugArea::Menu);
	}

	try
	{
		if (this->radarScreens.at(this->screenId)) return this->radarScreens.at(this->screenId)->getRadarScreen();
		else return nullptr;
	}
	catch (std::out_of_range)
	{
		messageHandler->writeMessage("ERROR", "Failed to return Radar Screen with id: " + std::to_string(this->screenId) +
			". Code: " + ERROR_DSP_SCREENCREATE);
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
		messageHandler->writeMessage("ERROR", "Couldn't process flight plan as it was reported invalid (technical invalid). Code: " +
			ERROR_FPLN_INVALID);
		return;
	}

	EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
	const std::string ades = fplnData.GetDestination();
	const std::string& adep = fplnData.GetOrigin();


	/* DOCUMENTATION 
		//for (int i = 0; i < 8; i++) // test on how to get annotations
		//{
		//	std::string annotation = flightPlanASEL.GetControllerAssignedData().GetFlightStripAnnotation(i);
		//	vsid::messagehandler::LogMessage("Debug", "Annotation: " + annotation);
		//}

		delete FlightPlan;
	}*/

	// dynamically fill sid selection list with valid sids

	if (this->processed.contains(callsign))
	{
		if (FunctionId == TAG_FUNC_VSID_SIDS_MAN)
		{
			std::string filedSidWpt = this->findSidWpt(fpln);
			std::map<std::string, vsid::Sid> validDepartures;
			std::string depRWY = vsid::fplnhelper::getAtcBlock(fpln).second;
			const std::string& icao = fplnData.GetOrigin();

			if (!this->activeAirports.contains(icao)) return;

			// deprwy is set and known

			if (depRWY != "" && this->processed.contains(callsign) && this->processed[callsign].atcRWY)
			{
				for (vsid::Sid& sid : this->activeAirports[fplnData.GetOrigin()].sids)
				{
					if ((sid.waypoint == filedSidWpt || sid.waypoint == "XXX" ||
						std::any_of(sid.transition.begin(), sid.transition.end(), [&](std::pair<std::string, vsid::Transition> trans)
							{
								return trans.first == filedSidWpt;
							})) && vsid::utils::contains(sid.rwys, depRWY))
					{
						validDepartures[sid.base + sid.number + sid.designator] = sid;
						if (this->activeAirports[icao].enableRVSids)
						{
							validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", { depRWY });
						}
					}
					else if (filedSidWpt == "" && vsid::utils::contains(sid.rwys, depRWY))
					{
						validDepartures[sid.base + sid.number + sid.designator] = sid;
						if (this->activeAirports[icao].enableRVSids)
						{
							validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", { depRWY });
						}
					}
				}
			}
			// deprwy is not set
			else if (depRWY == "")
			{
				for (vsid::Sid& sid : this->activeAirports[fplnData.GetOrigin()].sids)
				{
					if (sid.waypoint == filedSidWpt || sid.waypoint == "XXX" ||
						std::any_of(sid.transition.begin(), sid.transition.end(), [&](std::pair<std::string, vsid::Transition> trans)
							{
								return trans.first == filedSidWpt;
							}))
					{
						for (const std::string& sidRwy : sid.rwys)
						{
							if (this->activeAirports[icao].depRwys.contains(sidRwy)) validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							else if (sid.area != "" && this->activeAirports[icao].areas.contains(sid.area) && this->activeAirports[icao].areas[sid.area].arrAsDep &&
								this->activeAirports[icao].areas[sid.area].isActive &&
								this->activeAirports[icao].areas[sid.area].inside(fpln.GetFPTrackPosition().GetPosition()))
							{
								validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							}
						}
					}
					else if (filedSidWpt == "")
					{
						for (const std::string& sidRwy : sid.rwys)
						{
							if (this->activeAirports[icao].depRwys.contains(sidRwy)) validDepartures[sid.base + sid.number + sid.designator + " - " + sidRwy] = sid;
							else if (sid.area != "" && this->activeAirports[icao].areas.contains(sid.area) && this->activeAirports[icao].areas[sid.area].arrAsDep &&
								this->activeAirports[icao].areas[sid.area].isActive &&
								this->activeAirports[icao].areas[sid.area].inside(fpln.GetFPTrackPosition().GetPosition()))
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
				std::vector<std::string> filedRoute = vsid::fplnhelper::clean(fpln, filedSidWpt);

				if (depRWY != "")
				{
					std::ostringstream ss;
					ss << fplnData.GetOrigin() << "/" << depRWY;
					filedRoute.insert(filedRoute.begin(), ss.str());
				}
				this->processed[callsign].noFplnUpdate = true;
				if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to change flight plan! Code: " + ERROR_FPLN_SETROUTE);
					this->processed[callsign].noFplnUpdate = false;
				}
				if (!fplnData.AmendFlightPlan())
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! Code: " + ERROR_FPLN_AMEND);
					this->processed[callsign].noFplnUpdate = false;
				}
				else
				{
					//this->processed[callsign].noFplnUpdate = true;
					this->processed[callsign].atcRWY = true;
				}
			}
			else if (strlen(sItemString) != 0 && std::string(sItemString) != "NO SID")
			{
				if (depRWY == "")
				{
					if (std::string(sItemString).find("-") != std::string::npos)
					{
						try
						{
							depRWY = vsid::utils::split(sItemString, '-').at(1);
						}
						catch (std::out_of_range)
						{
							messageHandler->writeMessage("DEBUG", "[" + callsign + "] failed to retrieve rwy from selected SID: " +
								sItemString, vsid::MessageHandler::DebugArea::Sid);
						};
					}
					else
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] couldn't retrieve rwy from selected SID (no rwy present in SID): " +
							sItemString, vsid::MessageHandler::DebugArea::Sid);
					}
				}

				if (depRWY != "")
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] Calling manual SID with rwy : " + depRWY, vsid::MessageHandler::DebugArea::Sid);
					this->processFlightplan(fpln, false, depRWY, validDepartures[sItemString]);
				}
				else
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] didn't receive runway for selected SID " +
						sItemString + " and didn't process. Code: " + ERROR_FPLN_SIDMAN_RWY);
				}
			}
			// FlightPlan->flightPlan.GetControllerAssignedData().SetFlightStripAnnotation(0, sItemString) // test on how to set annotations (-> exclusive per plugin)
		}

		if (FunctionId == TAG_FUNC_VSID_SIDS_AUTO)
		{
			if (!this->activeAirports.contains(adep)) return;

			std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
			std::pair<std::string, std::string> atcBlock = vsid::fplnhelper::getAtcBlock(fpln);

			if (std::string(fplnData.GetPlanType()) == "I")
			{
				// if a non standard SID is detected reset the SID to the standard SID
				if (atcBlock.first != "" && atcBlock.first != std::string(fplnData.GetOrigin()))
				{
					if (std::find(atcBlock.first.begin(), atcBlock.first.end(), 'x') != atcBlock.first.end() || // #refactor - remove checks for xX
						std::find(atcBlock.first.begin(), atcBlock.first.end(), 'X') != atcBlock.first.end())
					{
						atcBlock.first = vsid::fplnhelper::splitTransition(atcBlock.first).first;
					}

					if (atcBlock.first != this->processed[callsign].sid.name()) this->processFlightplan(fpln, false);
					else vsid::fplnhelper::restoreIC(this->processed[callsign], fpln, ControllerMyself());
				}
				// if only a rwy is detected set the SID based on that RWY
				else if (this->processed[callsign].atcRWY && atcBlock.second != "")
				{
					this->processFlightplan(fpln, false, atcBlock.second);
				}
				// if nothing is detected set the default SID
				else this->processFlightplan(fpln, false);
			}
		}

		if (FunctionId == TAG_FUNC_VSID_TRANS)
		{
			std::map<std::string, vsid::Transition> validDepartures;
			const std::string filedSidWpt = this->findSidWpt(fpln);
			auto [blockSid, depRwy] = vsid::fplnhelper::getAtcBlock(fpln);

			if (!this->activeAirports.contains(adep)) return;

			if (this->processed.contains(callsign) && !this->processed[callsign].sid.empty() && // #checkforremoval - processed.contains check above FunctionId Block
				!this->processed[callsign].sid.transition.empty() && this->processed[callsign].customSid.empty())
			{
				for (auto& [base, trans] : this->processed[callsign].sid.transition)
				{
					if (filedSidWpt != "" && filedSidWpt != base) continue;

					validDepartures[trans.base + trans.number + trans.designator] = trans;
				}
			}
			else if (this->processed.contains(callsign) && !this->processed[callsign].customSid.empty() && // #checkforremoval - processed.contains check above FunctionId Block
				!this->processed[callsign].customSid.transition.empty())
			{
				for (auto& [base, trans] : this->processed[callsign].customSid.transition)
				{
					if (filedSidWpt != "" && filedSidWpt != base) continue;

					validDepartures[trans.base + trans.number + trans.designator] = trans;
				}
			}
			else if (this->processed.contains(callsign) && blockSid != "" && depRwy != "") // #checkforremoval - processed.contains check above FunctionId Block
			{
				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fplnhelper::splitTransition(blockSid).first;
				}

				for (vsid::Sid& sid : this->activeAirports[adep].sids)
				{
					if (blockSid == sid.name())
					{
						for (auto& [base, trans] : sid.transition)
						{
							if (filedSidWpt != "" && filedSidWpt != base) continue;

							validDepartures[trans.base + trans.number + trans.designator] = trans;
						}
					}
				}

				/*if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || #checkforremoval - doubled code
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fpln::splitTransition(blockSid);

					for (vsid::Sid& sid : this->activeAirports[adep].sids)
					{
						if (blockSid == sid.name())
						{
							for (auto& [base, trans] : sid.transition)
							{
								if (filedSidWpt != "" && filedSidWpt != base) continue;

								validDepartures[trans.base + trans.number + trans.designator] = trans;
							}
						}
					}
				}
				else
				{
					for (vsid::Sid& sid : this->activeAirports[adep].sids)
					{
						if (blockSid == sid.name())
						{
							for (auto& [base, trans] : sid.transition)
							{
								if (filedSidWpt != "" && filedSidWpt != base) continue;

								validDepartures[trans.base + trans.number + trans.designator] = trans;
							}
						}
					}
				}*/
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Select Trans", 1);

				if (blockSid == adep || blockSid == "")
				{
					this->AddPopupListElement("SELECT SID", "SELECT SID", TAG_FUNC_VSID_TRANS, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				}
				else if (validDepartures.size() == 0)
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
				std::vector<std::string> filedRoute = vsid::fplnhelper::clean(fpln, filedSidWpt);
				std::string sid;

				if (this->processed.contains(callsign) && !this->processed[callsign].sid.empty() && this->processed[callsign].customSid.empty()) // #checkforremoval - processed.contains check above FunctionId Block
					sid = this->processed[callsign].sid.name();
				else if (this->processed.contains(callsign) && !this->processed[callsign].customSid.empty()) // #checkforremoval - processed.contains check above FunctionId Block
					sid = this->processed[callsign].customSid.name();

				if (sid != "")
				{
					std::ostringstream ss;
					ss << sid << "x" << sItemString << "/" << depRwy;
					filedRoute.insert(filedRoute.begin(), ss.str());

					this->processed[callsign].noFplnUpdate = true;


					if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
					{
						messageHandler->writeMessage("ERROR", "[" + std::string(fpln.GetCallsign()) + "] - Failed to change flight plan! Code: " +
							ERROR_FPLN_SETROUTE);
						this->processed[callsign].noFplnUpdate = false;
					}
					if (!fplnData.AmendFlightPlan())
					{
						messageHandler->writeMessage("ERROR", "[" + std::string(fpln.GetCallsign()) + "] - Failed to amend flight plan! Code: " +
							ERROR_FPLN_AMEND);
						this->processed[callsign].noFplnUpdate = false;
					}
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_CLMBMENU)
		{
			if (!this->activeAirports.contains(adep)) return;

			std::map<std::string, int> alt;

			// code order important! the pop up list may only be generated when no sItemString is present (if a button has been clicked)
			// if the list gets set up again while clicking a button wrong values might occur

			for (int i = this->activeAirports[adep].maxInitialClimb; i >= vsid::utils::getMinClimb(this->activeAirports[adep].elevation); i -= 500)
			{
				std::string menuElem = (i > this->activeAirports[adep].transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
				alt[menuElem] = i;
			}

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Select Climb", 1);
				for (int i = this->activeAirports[adep].maxInitialClimb; i >= vsid::utils::getMinClimb(this->activeAirports[adep].elevation); i -= 500)
				{
					std::string clmbElem = (i > this->activeAirports[adep].transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
					this->AddPopupListElement(clmbElem.c_str(), clmbElem.c_str(), TAG_FUNC_VSID_CLMBMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				}

				if (alt.size() == 0)
					this->AddPopupListElement("NO MAX CLIMB", "NO MAX CLIMB", EuroScopePlugIn::TAG_ITEM_FUNCTION_NO, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
			}
			if (strlen(sItemString) != 0)
			{
				if (!fpln.GetControllerAssignedData().SetClearedAltitude(alt[sItemString]))
				{
					messageHandler->writeMessage("ERROR", "Failed to set cleared altitude. Code: " + ERROR_FPLN_SETALT);
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_RWYMENU)
		{
			if (!this->activeAirports.contains(adep)) return;

			std::vector<std::string> allRwys = this->activeAirports[adep].allRwys;
			std::string rwy;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "RWY", 1);
				for (std::vector<std::string>::iterator it = allRwys.begin(); it != allRwys.end();)
				{
					rwy = *it;
					if (this->activeAirports[adep].depRwys.contains(rwy) ||
						this->activeAirports[adep].arrRwys.contains(rwy)
						)
					{
						this->AddPopupListElement(rwy.c_str(), rwy.c_str(), TAG_FUNC_VSID_RWYMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
						it = allRwys.erase(it);
					}
					else ++it;
				}
				if (allRwys.size() > 0)
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
				std::vector<std::string> filedRoute = vsid::fplnhelper::clean(fpln);
				std::ostringstream ss;
				ss << fplnData.GetOrigin() << "/" << sItemString;
				filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));

				if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to change flight plan! Code: " + ERROR_FPLN_SETROUTE);
				}

				if (!vsid::fplnhelper::findRemarks(fpln, "VSID/RWY"))
				{
					if (!vsid::fplnhelper::addRemark(fpln, "VSID/RWY"))
					{
						messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to set remarks! Code: " + ERROR_FPLN_REMARKSET);
					}
				}

				if (!fplnData.AmendFlightPlan())
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! Code: " + ERROR_FPLN_AMEND);
				}
				else if (this->activeAirports[adep].settings["auto"] && this->processed.contains(callsign)) // #checkforremoval - .contains check now above FunctionID block
				{
					if (std::string(fpln.GetFlightPlanData().GetPlanType()) == "I" &&
						(!this->processed[callsign].sid.empty() || !this->processed[callsign].customSid.empty())
						)
					{
						this->processed[callsign].noFplnUpdate = true;

						vsid::fplnhelper::saveFplnInfo(callsign, this->processed[callsign], this->savedFplnInfo);

						this->processed.erase(callsign);
					}
				}
				else
				{
					this->processed[callsign].atcRWY = true;
				}
			}
		}

		if (FunctionId == TAG_FUNC_VSID_REQMENU)
		{
			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "REQ", 1);

				this->AddPopupListElement("No Req", "No Req", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);

				if (this->activeAirports.contains(adep))
				{
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
			}
			else if (strlen(sItemString) != 0)
			{
				std::string req = vsid::utils::tolower(sItemString);
				std::string newScratch = "";
				long long now = std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()).time_since_epoch().count();

				if (this->processed[callsign].request == req)
				{
					if (this->activeAirports[adep].requests.contains(req)) return;

					if (this->activeAirports[adep].rwyrequests.contains(req))
					{
						for (auto& [rwy, rwyReq] : this->activeAirports[adep].rwyrequests[req])
						{
							if (rwy != fplnData.GetDepartureRwy()) continue;

							for (auto& [reqCallsign, reqTime] : rwyReq)
							{
								if (reqCallsign == callsign) return;
							}
						}
					}
				}
				else if (req.find(this->processed[callsign].request) != std::string::npos)
				{
					if (req.find("rwy") != std::string::npos)
					{
						try
						{
							std::string normReq = vsid::utils::split(req, ' ').at(1);

							if (this->activeAirports[adep].requests.contains(normReq))
							{
								for (auto& [reqCallign, reqTime] : this->activeAirports[adep].requests[normReq])
								{
									if (reqCallign != callsign) continue;
									now = reqTime;
									break;
								}
							}
						}
						catch (std::out_of_range) {};
					}
				}
				else if (this->processed[callsign].request.find(req) != std::string::npos)
				{
					try
					{
						bool stop = false;
						for (auto& [rwy, rwyReq] : this->activeAirports[adep].rwyrequests.at(this->processed[callsign].request))
						{
							if (rwy != fplnData.GetDepartureRwy()) continue;

							for (auto& [reqCallsign, reqTime] : rwyReq)
							{
								if (reqCallsign != callsign) continue;

								now = reqTime;
								stop = true;

								break;
							}
							if (stop) break;
						}
					}
					catch (std::out_of_range) {};
				}

				newScratch = ".vsid_req_" + std::string(sItemString) + "/" + std::to_string(now); // #refactor - now check for empty string needed

				if (newScratch != "") this->addSyncQueue(callsign, newScratch, fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_CLR_SID)
		{
			if (!this->activeAirports.contains(adep)) return;

			auto [atcSid, atcRwy] = vsid::fplnhelper::getAtcBlock(fpln);

			this->callExtFunc(callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
				callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_SET_CLEARED_FLAG, POINT(), RECT());

			if (this->processed.contains(callsign) && std::string(fpln.GetFlightPlanData().GetPlanType()) != "V" && // #checkforremoval - .contains check now above FunctionId block
				(atcSid == "" || atcSid == adep))
				this->processFlightplan(fpln, false, atcRwy);
		}

		if (FunctionId == TAG_FUNC_VSID_CLR_SID_SU)
		{
			if (!this->activeAirports.contains(adep)) return;

			auto [atcSid, atcRwy] = vsid::fplnhelper::getAtcBlock(fpln);

			if (!fpln.GetClearenceFlag())
			{
				this->callExtFunc(callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
					callsign.c_str(), nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_SET_CLEARED_FLAG, POINT(), RECT());
			}

			if (this->processed.contains(callsign) && std::string(fpln.GetFlightPlanData().GetPlanType()) != "V" && // #checkforremoval - .contains check now above FunctionId block
				(atcSid == "" || atcSid == adep))
				this->processFlightplan(fpln, false, atcRwy);

			if (std::string(fpln.GetGroundState()) == "") this->addSyncQueue(callsign, "STUP", fpln.GetControllerAssignedData().GetScratchPadString()); // # dev - sync
		}

		if (FunctionId == TAG_FUNC_VSID_INTS_SET)
		{
			if (!this->activeAirports.contains(adep)) return;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Set Int", 1);

				std::string depRwy = vsid::fplnhelper::getAtcBlock(fpln).second;
				if (depRwy == "") this->AddPopupListElement("NO RWY", "NO RWY", TAG_FUNC_VSID_INTS_SET, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				else if (this->activeAirports[adep].intsec.contains(depRwy))
				{
					for (std::string& intsec : this->activeAirports[adep].intsec[depRwy])
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
				else if (std::string(sItemString) == "Clear") this->addSyncQueue(callsign, ".vsid_int_none_false", fpln.GetControllerAssignedData().GetScratchPadString());
				else if (std::string(sItemString) == "NO RWY") return;
				else if (std::string(sItemString) == "NO INTS") return;
				else this->addSyncQueue(callsign, ".vsid_int_" + std::string(sItemString).substr(0, 3) + "_true", fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_INTS_ABLE)
		{
			if (!this->activeAirports.contains(adep)) return;

			if (strlen(sItemString) == 0)
			{
				this->OpenPopupList(Area, "Able Int", 1);

				std::string depRwy = vsid::fplnhelper::getAtcBlock(fpln).second;
				if (depRwy == "") this->AddPopupListElement("NO RWY", "NO RWY", TAG_FUNC_VSID_INTS_ABLE, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, true, false);
				else if (this->activeAirports[adep].intsec.contains(depRwy))
				{
					for (std::string& intsec : this->activeAirports[adep].intsec[depRwy])
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
				else if (std::string(sItemString) == "Clear") this->addSyncQueue(callsign, ".VSID_INT_NONE_FALSE", fpln.GetControllerAssignedData().GetScratchPadString());
				else if (std::string(sItemString) == "NO RWY") return;
				else if (std::string(sItemString) == "NO INTS") return;
				else this->addSyncQueue(callsign, ".VSID_INT_" + std::string(sItemString).substr(0, 3) + "_FALSE", fpln.GetControllerAssignedData().GetScratchPadString());
			}
		}

		if (FunctionId == TAG_FUNC_VSID_TSSQUAWK)
		{
			if (this->topskyLoaded)
			{
				this->callExtFunc(callsign.c_str(), "TopSky plugin", EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, callsign.c_str(), "TopSky plugin", 667, POINT(), RECT());
			}
			else messageHandler->writeMessage("ERROR", "TopSky auto-assign squawk called, but TopSky was not detected.");
		}
	}

	if (FunctionId == TAG_FUNC_VSID_CTL)
	{
		this->processed[callsign].ctl = !this->processed[callsign].ctl;

		std::string ctl = (this->processed[callsign].ctl) ? "TRUE" : "FALSE";
		this->addSyncQueue(callsign, ".VSID_CTL_" + ctl, fpln.GetControllerAssignedData().GetScratchPadString());
	}

	if (FunctionId == TAG_FUNC_VSID_CTL_LOCAL)
	{
		this->processed[callsign].ctlLocal = !this->processed[callsign].ctlLocal;
	}
}

void vsid::VSIDPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	if (!FlightPlan.IsValid()) return;

	this->processSPQueue(); // process sync queue on each tagItem update

	if (this->outOfVis(FlightPlan)) return;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = fplnData.GetOrigin(); // #continue - replace GetOrigin with adep below
	std::string ades = fplnData.GetDestination();

	if (this->activeAirports.contains(adep))
	{
		if (ItemCode == TAG_ITEM_VSID_SIDS)
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
			auto [blockSid, blockRwy] = vsid::fplnhelper::getAtcBlock(FlightPlan);

			if (this->processed.contains(callsign))
			{
				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					blockSid = vsid::fplnhelper::splitTransition(blockSid).first;
				}

				std::string sidName = this->processed[callsign].sid.name();
				std::string customSidName = this->processed[callsign].customSid.name();		

				// set sid item color

				if (((blockSid == fplnData.GetOrigin() &&
					this->processed[callsign].atcRWY &&
					this->processed[callsign].customSid.empty()) ||
					(blockSid == "" &&
						this->processed[callsign].sid.empty() &&
						this->processed[callsign].customSid.empty())) &&
					std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "I"
					)
				{
					*pRGB = this->configParser.getColor("noSid");
				}
				else if ((blockSid == "" ||
					blockSid == fplnData.GetOrigin()) &&
					((this->processed[callsign].customSid.empty() ||
						this->processed[callsign].sid == this->processed[callsign].customSid) ||
						((std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V" &&
							std::string(sItemString) == "VFR")
							))
					)
				{
					*pRGB = this->configParser.getColor("sidSuggestion");
				}
				else if (blockSid != "" &&
					blockSid == fplnData.GetOrigin() &&
					!this->processed[callsign].customSid.empty() &&
					this->processed[callsign].sid != this->processed[callsign].customSid
					)
				{
					*pRGB = this->configParser.getColor("customSidSuggestion");
				}
				else if (blockSid != "" && ((blockSid == sidName && this->processed[callsign].sid.sidHighlight) ||
					(blockSid == customSidName && this->processed[callsign].customSid.sidHighlight)))
				{
					*pRGB = this->configParser.getColor("sidHighlight");
				}
				else if ((blockSid != "" &&
					blockSid != fplnData.GetOrigin() &&
					blockSid == customSidName &&
					this->processed[callsign].sid != this->processed[callsign].customSid) ||
					blockSid != sidName
					)
				{
					*pRGB = this->configParser.getColor("customSidSet");
				}
				else if (blockSid != "" && blockSid == sidName)
				{
					*pRGB = this->configParser.getColor("suggestedSidSet");
				}
				else *pRGB = RGB(140, 140, 60);

				// set sid item text

				if (blockSid != "" && blockSid != fplnData.GetOrigin())
				{
					strcpy_s(sItemString, 16, blockSid.c_str());
				}
				else if ((blockSid == "" ||
					blockSid == fplnData.GetOrigin()) &&
					std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V"
					)
				{
					strcpy_s(sItemString, 16, "VFR");
				}
				else if ((blockSid != "" &&
					blockSid == fplnData.GetOrigin() &&
					this->processed[callsign].atcRWY &&
					!vsid::utils::contains(this->processed[callsign].customSid.rwys, blockRwy)) ||
					(this->processed[callsign].sid.empty() &&
						this->processed[callsign].customSid.empty())
					)
				{
					if (this->processed[callsign].validEquip && this->processed[callsign].sidWpt == "") strcpy_s(sItemString, 16, "MANUAL");
					else if (this->processed[callsign].validEquip && this->processed[callsign].sidWpt != "") strcpy_s(sItemString, 16, this->processed[callsign].sidWpt.c_str());
					else if (!this->processed[callsign].validEquip) strcpy_s(sItemString, 16, "EQUIP");
				}
				else if (sidName != "" && customSidName == "")
				{
					strcpy_s(sItemString, 16, sidName.c_str());
				}
				else if (customSidName != "")
				{
					strcpy_s(sItemString, 16, customSidName.c_str());
				}
			}
			else if (this->activeAirports.contains(fplnData.GetOrigin()) && RadarTarget.GetGS() <= 50)
			{
				bool checkOnly = !this->activeAirports[fplnData.GetOrigin()].settings["auto"];

				if (!checkOnly)
				{
					if (FlightPlan.GetClearenceFlag() ||
						std::string(fplnData.GetPlanType()) == "V")/* ||
						(atcBlock.first != "" && atcBlock.first != fplnData.GetOrigin())*/
					{
						checkOnly = true;
					}
					else if (!FlightPlan.GetClearenceFlag() && blockSid != fplnData.GetOrigin() && fplnData.IsAmended())
					{
						// prevent automode to use rwys set before
						blockRwy = "";
					}
				}
				if (blockRwy != "" &&
					(vsid::fplnhelper::findRemarks(FlightPlan, "VSID/RWY") ||
						blockSid != fplnData.GetOrigin() ||
						fplnData.IsAmended())
					)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] not yet processed, calling processFlightplan with atcRwy: " +
						blockRwy + ((!checkOnly) ? " and setting the fpln." : " and only checking the fpln"),
						vsid::MessageHandler::DebugArea::Sid);
					this->processFlightplan(FlightPlan, checkOnly, blockRwy);
				}
				else
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] not yet processed, calling processFlightplan without atcRwy" +
						((!checkOnly) ? " and setting the fpln." : " and only checking the fpln"),
						vsid::MessageHandler::DebugArea::Sid);
					this->processFlightplan(FlightPlan, checkOnly);
				}

			}
			// if the airborne aircraft has no SID set display the first waypoint of the route

			if (std::string(fplnData.GetPlanType()) != "V" && RadarTarget.GetGS() > 50 && this->activeAirports.contains(fplnData.GetOrigin()) &&
				RadarTarget.GetPosition().GetPressureAltitude() >= this->activeAirports[fplnData.GetOrigin()].elevation + 100)
			{
				std::vector<std::string> route = vsid::utils::split(fplnData.GetRoute(), ' ');

				if ((blockSid != "" && blockSid == fplnData.GetOrigin()) || blockSid == "")
				{
					*pRGB = this->configParser.getColor("customSidSuggestion");
					bool validWpt = false;

					for (const std::string& wpt : route)
					{
						if (this->activeAirports[fplnData.GetOrigin()].isSidWpt(wpt))
						{
							strcpy_s(sItemString, 16, wpt.c_str());
							validWpt = true;
							break;
						}
					}
					if (!validWpt) strcpy_s(sItemString, 16, "");
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] airborne and atc.first is no SID: \"" + blockSid + "\"", vsid::MessageHandler::DebugArea::Sid);
				}
				// processed flight plans are managed above - this is for already airborne flight plans after connecting

				else if (!this->processed.contains(callsign) && blockSid != "" && blockSid != fplnData.GetOrigin())
				{
					*pRGB = this->configParser.getColor("customSidSuggestion");
					strcpy_s(sItemString, 16, blockSid.c_str());
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] airborne and unknown. SID: \"" + blockSid + "\"", vsid::MessageHandler::DebugArea::Sid);
				}
			}
		}

		if (ItemCode == TAG_ITEM_VSID_TRANS)
		{
			if (this->processed.contains(callsign))
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

				std::string adep = fplnData.GetOrigin();
				std::string sidName = this->processed[callsign].sid.name();
				std::string customSidName = this->processed[callsign].customSid.name();
				std::string transition = "";
				auto [blockSid, blockRwy] = vsid::fplnhelper::getAtcBlock(FlightPlan);

				if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
					std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
				{
					transition = blockSid;
					blockSid = vsid::fplnhelper::splitTransition(blockSid).first;
					transition.erase(transition.find(blockSid), blockSid.length());

					if (transition != "" && (transition.at(0) == 'X' || transition.at(0) == 'x')) transition.erase(0, 1);
				}

				if (blockSid == "")
				{
					*pRGB = this->configParser.getColor("sidSuggestion");

					if (this->processed[callsign].transition != "")
						strcpy_s(sItemString, 16, this->processed[callsign].transition.c_str());
					else strcpy_s(sItemString, 16, "---");
				}
				else
				{
					if(blockSid == adep && this->processed[callsign].transition != "" &&
						!this->processed[callsign].customSid.empty() &&
						this->processed[callsign].sid != this->processed[callsign].customSid)
							*pRGB = this->configParser.getColor("customSidSuggestion");
					else if (blockSid != "" && transition != "" && this->processed[callsign].transition == transition &&
						((blockSid == sidName && this->processed[callsign].sid.sidHighlight) ||
						(blockSid == customSidName && this->processed[callsign].customSid.sidHighlight)))
					{
						*pRGB = this->configParser.getColor("sidHighlight");
					}
					else if (transition != "" && this->processed[callsign].transition == transition)
						*pRGB = this->configParser.getColor("suggestedSidSet");
					else if (transition != "" && this->processed[callsign].transition != transition)
						*pRGB = this->configParser.getColor("customSidSet");
					/*else if (blockSid != adep && transition == "" && this->processed[callsign].transition != "")
						*pRGB = this->configParser.getColor("noSid");*/
					else *pRGB = this->configParser.getColor("sidSuggestion");

					if (transition != "") strcpy_s(sItemString, 16, transition.c_str());
					else if (transition == "" && this->processed[callsign].transition != "" &&
						((blockSid != adep && !this->processed[callsign].sid.empty()) ||
							(blockSid == adep && !this->processed[callsign].customSid.empty())))
					{
						strcpy_s(sItemString, 16, this->processed[callsign].transition.c_str());
					}
							
					else strcpy_s(sItemString, 16, "---");
				}
			}
		}

		if (ItemCode == TAG_ITEM_VSID_CLIMB)
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
			EuroScopePlugIn::CFlightPlan fpln = FlightPlan;

			int transAlt = this->activeAirports[fplnData.GetOrigin()].transAlt;
			int tempAlt = 0;
			bool climbVia = false;

			if (this->processed.contains(callsign))
			{
				// determine if a found non-standard sid exists in valid sids and set it for ic comparison

				std::string sidName = this->processed[callsign].sid.name();
				std::string customSidName = this->processed[callsign].customSid.name();
				std::string atcSid = vsid::fplnhelper::getAtcBlock(FlightPlan).first;

				if (std::find(atcSid.begin(), atcSid.end(), 'x') != atcSid.end() || // #refactor - remove checks for xX
					std::find(atcSid.begin(), atcSid.end(), 'X') != atcSid.end())
				{
					atcSid = vsid::fplnhelper::splitTransition(atcSid).first;
				}

				
				// if an unknown Sid is set (non-standard or non-custom) try to find matching Sid in config

				if (atcSid != "" && atcSid != fplnData.GetOrigin() && atcSid != sidName && atcSid != customSidName)
				{
					for (vsid::Sid& sid : this->activeAirports[fplnData.GetOrigin()].sids)
					{
						if (atcSid == sid.name() && this->processed[callsign].sid != sid)
						{
							this->processed[callsign].customSid = sid;
							break;
						}
					}
				}

				// determine if climb via is needed depending on customSid

				if ((atcSid == "" || atcSid == sidName || atcSid == fplnData.GetOrigin()) && sidName != customSidName)
				{
					climbVia = this->processed[callsign].sid.climbvia;
				}
				else if (atcSid == "" || atcSid == customSidName || atcSid == fplnData.GetOrigin())
				{
					climbVia = this->processed[callsign].customSid.climbvia;
				}

				// determine initial climb depending on customSid

				bool rflBelowInitial = false; // additional check for suggestion coloring

				if (this->processed[callsign].sid.initialClimb != 0 &&
					this->processed[callsign].customSid.empty() &&
					(atcSid == sidName || atcSid == "" || atcSid == fplnData.GetOrigin())
					)
				{
					if (fpln.GetFinalAltitude() < this->processed[callsign].sid.initialClimb)
					{
						tempAlt = fpln.GetFinalAltitude();
						rflBelowInitial = true;
					}
					else tempAlt = this->processed[callsign].sid.initialClimb;
				}
				else if (this->processed[callsign].customSid.initialClimb != 0 &&
					(atcSid == customSidName || atcSid == "" || atcSid == fplnData.GetOrigin())
					)
				{
					if (fpln.GetFinalAltitude() < this->processed[callsign].customSid.initialClimb)
					{
						tempAlt = fpln.GetFinalAltitude();
						rflBelowInitial = true;
					}
					else tempAlt = this->processed[callsign].customSid.initialClimb;
				}

				if (fpln.GetClearedAltitude() == fpln.GetFinalAltitude() && !rflBelowInitial && tempAlt != fpln.GetFinalAltitude())
				{
					*pRGB = this->configParser.getColor("suggestedClmb"); // white
				}
				else if ((fpln.GetClearedAltitude() != fpln.GetFinalAltitude() || tempAlt == this->processed[callsign].sid.initialClimb ||
					tempAlt == this->processed[callsign].customSid.initialClimb) &&
					fpln.GetClearedAltitude() == tempAlt
					)
				{
					if ((tempAlt == this->processed[callsign].sid.initialClimb && this->processed[callsign].sid.clmbHighlight) ||
						(tempAlt == this->processed[callsign].customSid.initialClimb && this->processed[callsign].customSid.clmbHighlight))
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

				if (fpln.GetClearedAltitude() == fpln.GetFinalAltitude())
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
					if (fpln.GetClearedAltitude() == 0)
					{
						strcpy_s(sItemString, 16, std::string("---").c_str());
					}
					else if (fpln.GetClearedAltitude() <= transAlt)
					{
						strcpy_s(sItemString, 16, std::string("A").append(std::to_string(fpln.GetClearedAltitude() / 100)).c_str());
					}
					else
					{
						if (fpln.GetClearedAltitude() / 100 >= 100)
						{
							strcpy_s(sItemString, 16, std::to_string(fpln.GetClearedAltitude() / 100).c_str());
						}
						else
						{
							strcpy_s(sItemString, 16, std::string("0").append(std::to_string(fpln.GetClearedAltitude() / 100)).c_str());
						}
					}
				}
			}
		}

		if (ItemCode == TAG_ITEM_VSID_RWY)
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;

			if (this->processed.contains(callsign))
			{
				std::pair<std::string, std::string> atcBlock = vsid::fplnhelper::getAtcBlock(FlightPlan);

				if (atcBlock.first == fplnData.GetOrigin() &&
					!this->processed[callsign].atcRWY &&
					!this->processed[callsign].remarkChecked
					)
				{
					this->processed[callsign].atcRWY = vsid::fplnhelper::findRemarks(FlightPlan, "VSID/RWY");
					this->processed[callsign].remarkChecked = true;
					if (this->processed[callsign].atcRWY)
					{
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepted RWY because remarks are found.",
							vsid::MessageHandler::DebugArea::Rwy);
					}
				}
				else if (atcBlock.first == fplnData.GetOrigin() &&
					!this->processed[callsign].atcRWY &&
					fplnData.IsAmended()
					)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepted RWY because FPLN is amended and ICAO is found",
						vsid::MessageHandler::DebugArea::Rwy);
					this->processed[callsign].atcRWY = true;
				}
				else if (!this->processed[callsign].atcRWY &&
					atcBlock.first != "" &&
					(atcBlock.first == this->processed[callsign].sid.name() ||
						atcBlock.first == this->processed[callsign].customSid.name())
					)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepted RWY because SID/RWY is found \"" + atcBlock.first + "/" + atcBlock.second +
						"\" SID: \"" + this->processed[callsign].sid.name() + "\" Custom SID: \"" + this->processed[callsign].customSid.name() + "\"",
						vsid::MessageHandler::DebugArea::Rwy);
					this->processed[callsign].atcRWY = true;
				}
				else if (!this->processed[callsign].atcRWY &&
					atcBlock.first != "" &&
					atcBlock.first != fplnData.GetOrigin() &&
					(fplnData.IsAmended() || FlightPlan.GetClearenceFlag())
					)
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] accepted RWY because no ICAO is found and other than configured SID is found "
						" and " + ((fplnData.IsAmended()) ? " fpln is amended" : "") +
						((FlightPlan.GetClearenceFlag() ? " clearance flag set" : "")), vsid::MessageHandler::DebugArea::Rwy);
					this->processed[callsign].atcRWY = true;
				}

				if (atcBlock.second != "" &&
					this->activeAirports[fplnData.GetOrigin()].depRwys.contains(atcBlock.second) &&
					this->processed[callsign].atcRWY
					)
				{
					*pRGB = this->configParser.getColor("rwySet");
				}
				else if (atcBlock.second != "" &&
					!this->activeAirports[fplnData.GetOrigin()].depRwys.contains(atcBlock.second) &&
					this->processed[callsign].atcRWY
					)
				{
					*pRGB = this->configParser.getColor("notDepRwySet");
				}
				else *pRGB = this->configParser.getColor("rwyNotSet");

				if (atcBlock.second != "" && this->processed[callsign].atcRWY)
				{
					strcpy_s(sItemString, 16, atcBlock.second.c_str());
				}
				else
				{
					std::string sidRwy;

					if (!this->processed[callsign].sid.empty())
					{
						try // #checkforremoval
						{
							bool arrAsDep = false;
							std::string& sidArea = this->processed[callsign].sid.area;

							if (sidArea != "" && this->activeAirports.contains(adep) && this->activeAirports[adep].areas.contains(sidArea) &&
								this->activeAirports[adep].areas[sidArea].isActive &&
								this->activeAirports[adep].areas[sidArea].inside(FlightPlan.GetFPTrackPosition().GetPosition()))
							{
								arrAsDep = this->activeAirports[adep].areas[sidArea].arrAsDep;
							}

							for (const std::string& rwy : this->processed[callsign].sid.rwys)
							{
								if (this->activeAirports[adep].isDepRwy(rwy, arrAsDep))
								{
									sidRwy = rwy;
									break;
								}
							}

							messageHandler->removeFplnError(callsign, ERROR_CONF_RWYMENU);
						}
						catch (std::out_of_range)
						{
							if (!messageHandler->getFplnErrors(callsign).contains(ERROR_CONF_RWYMENU))
							{
								messageHandler->writeMessage("ERROR", "Failed to get RWY in the RWY menu. Check config " +
									this->activeAirports[fplnData.GetOrigin()].icao + " for SID \"" +
									this->processed[callsign].sid.idName() + "\". RWY value is: " +
									vsid::utils::join(this->processed[callsign].sid.rwys));
								messageHandler->addFplnError(callsign, ERROR_CONF_RWYMENU);
							}
						}
					}

					if (sidRwy == "") sidRwy = "---";
					strcpy_s(sItemString, 16, sidRwy.c_str());
				}
			}
			else
			{
				*pRGB = this->configParser.getColor("rwyNotSet");
				strcpy_s(sItemString, 16, fplnData.GetDepartureRwy());
			}
		}

		if (ItemCode == TAG_ITEM_VSID_SQW)
		{
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

		if (ItemCode == TAG_ITEM_VSID_REQ)
		{
			if (this->processed.contains(callsign) && this->processed[callsign].request != "")
			{
				std::string& request = this->processed[callsign].request;

				// check rwy requests first

				if(this->activeAirports[adep].rwyrequests.contains(request))
				{
					for (auto& [rwy, rwyreq] : this->activeAirports[adep].rwyrequests[request])
					{
						for (std::set<std::pair<std::string, long long>>::iterator it = rwyreq.begin(); it != rwyreq.end(); ++it)
						{
							if (it->first == callsign)
							{
								int pos = std::distance(it, rwyreq.end());
								std::string req = vsid::utils::toupper(request).at(0) + std::to_string(pos);
								strcpy_s(sItemString, 16, req.c_str());
								break;
							}
						}
					}
				}
				// check normal requests
				else if (this->activeAirports[adep].requests.contains(request))
				{
					for (std::set<std::pair<std::string, long long>>::iterator it = this->activeAirports[adep].requests[request].begin();
						it != this->activeAirports[adep].requests[request].end(); ++it)
					{
						if (it->first == callsign)
						{
							int pos = std::distance(it, this->activeAirports[adep].requests[request].end());
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
			if (this->processed.contains(callsign) && this->processed[callsign].request != "")
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				long long now = std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()).time_since_epoch().count();

				std::string& request = this->processed[callsign].request;

				// determine rwy request timer
				if (this->activeAirports[adep].rwyrequests.contains(request))
				{
					for (auto& [rwy, rwyReq] : this->activeAirports[adep].rwyrequests[request])
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
				else if (this->activeAirports[adep].requests.contains(request))
				{
					for (auto& [reqCallsign, reqTime] : this->activeAirports[adep].requests[request])
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
			
			if(FlightPlan.GetClearenceFlag()) strcpy_s(sItemString, 16, "\xA4");
			else strcpy_s(sItemString, 16, "\xAC");
		}

		if (ItemCode == TAG_ITEM_VSID_INTS)
		{
			if (this->processed.contains(callsign))
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				
				if(this->processed[callsign].intsec.second) *pRGB = this->configParser.getColor("intsecSet");
				else *pRGB = this->configParser.getColor("intsecAble");

				strcpy_s(sItemString, 16, this->processed[callsign].intsec.first.c_str());
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

		if (!this->processed.contains(callsign))
		{
			if (std::string(fplnData.GetPlanType()) == "V" || !this->activeAirports.contains(ades)) return;

			if (dtg <= clrf.distWarning && alt <= this->activeAirports[ades].elevation + clrf.altWarning)
			{
				*pRGB = this->configParser.getColor("clrfWarning");
				strcpy_s(sItemString, 16, "CLR!");
			}
			else if (dtg <= clrf.distCaution && alt <= this->activeAirports[ades].elevation + clrf.altCaution)
			{
				// creates an entry
				this->processed[callsign].ldgAlt = alt;
				*pRGB = this->configParser.getColor("clrfCaution");
				strcpy_s(sItemString, 16, "CLR");
			}
		}
		else
		{
			if (this->processed[callsign].mapp && ((this->activeAirports.contains(ades) &&
				alt > this->activeAirports[ades].elevation + clrf.altCaution + 200) || alt > clrf.altCaution + 200))
			{
				this->processed.erase(callsign);
				return;
			}

			if (this->processed[callsign].ctl)
			{
				if (this->processed[callsign].ldgAlt == 0) this->processed[callsign].ldgAlt = alt;

				*pRGB = this->configParser.getColor("clrfSet");
				strcpy_s(sItemString, 16, "CTL");
			}
			else
			{
				if (std::string(fplnData.GetPlanType()) == "V" || !this->activeAirports.contains(ades)) return;
				if (this->processed[callsign].mapp) return;

				if (dtg <= clrf.distWarning && alt <= this->activeAirports[ades].elevation + clrf.altWarning)
				{
					*pRGB = this->configParser.getColor("clrfWarning");
					strcpy_s(sItemString, 16, "CLR!");
				}
				else if (dtg <= clrf.distCaution && alt <= this->activeAirports[ades].elevation + clrf.altCaution)
				{
					if (this->processed[callsign].ldgAlt == 0) this->processed[callsign].ldgAlt = alt;

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
		 
		if (!this->processed.contains(callsign) &&
			(std::string(fplnData.GetPlanType()) == "V" || !this->activeAirports.contains(ades)))
				return;
		else if (this->processed.contains(callsign))
		{
			// alt & clrf only for mapp calculation
			int alt = RadarTarget.GetPosition().GetPressureAltitude();
			vsid::Clrf& clrf = this->configParser.getClrfMinimums();

			if (this->processed[callsign].mapp && ((this->activeAirports.contains(ades) &&
				alt > this->activeAirports[ades].elevation + clrf.altCaution + 200) || alt > clrf.altCaution + 200))
			{
				this->processed.erase(callsign);
				return;
			}

			if (this->processed[callsign].ctlLocal)
			{
				if (this->processed[callsign].ldgAlt == 0) this->processed[callsign].ldgAlt = alt;

				*pRGB = this->configParser.getColor("clrfSet");
				strcpy_s(sItemString, 16, "CTL");
			}
		}
	}
}

bool vsid::VSIDPlugin::OnCompileCommand(const char* sCommandLine)
{
	std::vector<std::string> command = vsid::utils::split(sCommandLine, ' ');

	if (command[0] == ".vsid")
	{
		if (command.size() == 1)
		{
			messageHandler->writeMessage("INFO", "Available commands: "
				"version / "
				"auto [icao] - activate automode for icao - sets force mode if lower atc online /"
				"area [icao] [areaname] - toggle area for icao /"
				"rule [icao] [rulename] - toggle rule for icao / "
				"night [icao] - toggle night mode for icao /"
				"lvp [icao] - toggle lvp ops for icao / "
				"req icao - lists request list entries / "
				"req icao reset [listname] - resets all request lists or specified list / "
				"reload [ese] - reloads the main config or the ese file / "
				"Debug - toggle debug mode");
			return true;
		}
		if (vsid::utils::tolower(command[1]) == "version")
		{
			messageHandler->writeMessage("INFO", "vSID Version " + pluginVersion +
				" loaded. Using nlohmann json (" + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." +
				std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "." + std::to_string(NLOHMANN_JSON_VERSION_PATCH) +
				"). Using libcurl (" + curl_version() + ")");
			return true;
		}
		// debugging only
		else if (vsid::utils::tolower(command[1]) == "removed")
		{
			for (auto &elem : this->removeProcessed)
			{
				messageHandler->writeMessage("DEBUG", "[" + elem.first + "] being removed at: " + vsid::time::toFullString(elem.second.first) +
											" and is disconnected " + ((elem.second.second) ? "YES" : "NO"), vsid::MessageHandler::DebugArea::Dev);
			}
			if (this->removeProcessed.size() == 0)
			{
				messageHandler->writeMessage("DEBUG", "Removed list empty", vsid::MessageHandler::DebugArea::Dev);
			}
			return true;
		}
		// end debugging only
		else if (vsid::utils::tolower(command[1]) == "rule")
		{
			if (command.size() == 2)
			{
				for (std::pair<const std::string, vsid::Airport>& airport : this->activeAirports)
				{
					if (!airport.second.customRules.empty())
					{
						std::ostringstream ss;
						for (std::pair<const std::string, bool>& rule : airport.second.customRules)
						{
							std::string status = (rule.second) ? "ON" : "OFF";
							ss << rule.first << ": " << status << " ";
						}
						messageHandler->writeMessage(airport.first + " Rules", ss.str());
					}
					else messageHandler->writeMessage(airport.first + " Rules", "No rules configured");
				}
			}
			else if (command.size() >= 3 && this->activeAirports.contains(vsid::utils::toupper(command[2])))
			{
				std::string icao = vsid::utils::toupper(command[2]);
				if (command.size() == 3)
				{
					if (!this->activeAirports[icao].customRules.empty())
					{
						std::ostringstream ss;
						for (std::pair<const std::string, bool>& rule : this->activeAirports[icao].customRules)
						{
							std::string status = (rule.second) ? "ON " : "OFF ";
							ss << rule.first << ": " << status;
						}
						messageHandler->writeMessage(icao + " Rules", ss.str());
					}
					else messageHandler->writeMessage(icao + " Rules", "No rules configured");
				}
				else if (command.size() == 4)
				{
					std::string rule = vsid::utils::toupper(command[3]);
					if (this->activeAirports[icao].customRules.contains(rule))
					{
						this->activeAirports[icao].customRules[rule] = !this->activeAirports[icao].customRules[rule];
						messageHandler->writeMessage(icao + " Rule", rule + " " + (this->activeAirports[icao].customRules[rule] ? "ON" : "OFF"));

						std::erase_if(this->processed, [&](const std::pair<std::string, vsid::Fpln>& pFpln)
							{
								auto& [callsign, fplnInfo] = pFpln;

								EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(callsign.c_str());
								EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();

								if (fpln.IsValid() &&
									!fpln.GetClearenceFlag() &&
									this->activeAirports.contains(fplnData.GetOrigin()) &&
									this->activeAirports[fplnData.GetOrigin()].settings["auto"]
									)
								{
									vsid::fplnhelper::saveFplnInfo(callsign, fplnInfo, this->savedFplnInfo);

									return true;
								}
								else return false;
							}
						);

						for (std::pair<const std::string, vsid::Fpln> fpln : this->processed) // #refactor type deduction
						{
							EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(fpln.first.c_str());
							auto atcBlock = vsid::fplnhelper::getAtcBlock(FlightPlan);

							messageHandler->writeMessage("DEBUG", "[" + fpln.first + "] rechecking due to rule change.", vsid::MessageHandler::DebugArea::Sid);

							if (atcBlock.second != "") this->processFlightplan(FlightPlan, true, atcBlock.second);
							else this->processFlightplan(FlightPlan, true);
						}
					}
					else messageHandler->writeMessage(icao + " " + command[3], "Rule is unknown");
				}
			}
			else messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " not in active airports");
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "lvp")
		{
			if (command.size() == 2)
			{
				std::ostringstream ss;
				for (auto it = this->activeAirports.begin(); it != this->activeAirports.end();)
				{
					std::string status = (it->second.settings["lvp"]) ? "ON" : "OFF";

					ss << it->first << " LVP: " << status;
					it++;
					if (it != this->activeAirports.end()) ss << " / ";
				}
				messageHandler->writeMessage("INFO", ss.str());
			}
			else if (command.size() == 3)
			{
				if (this->activeAirports.contains(vsid::utils::toupper(command[2])))
				{
					if (this->activeAirports[vsid::utils::toupper(command[2])].settings["lvp"])
					{
						this->activeAirports[vsid::utils::toupper(command[2])].settings["lvp"] = 0;
						messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " LVP: " +
							((this->activeAirports[vsid::utils::toupper(command[2])].settings["lvp"]) ? "ON" : "OFF")
						);
					}
					else
					{
						this->activeAirports[vsid::utils::toupper(command[2])].settings["lvp"] = 1;
						messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " LVP: " +
							((this->activeAirports[vsid::utils::toupper(command[2])].settings["lvp"]) ? "ON" : "OFF")
						);
					}
					this->UpdateActiveAirports();
				}
				else messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " is not in active airports");
			}
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "night" || vsid::utils::tolower(command[1]) == "time")
		{
			if (command.size() == 2)
			{
				std::ostringstream ss;
				for (auto it = this->activeAirports.begin(); it != this->activeAirports.end();)
				{
					std::string status = (it->second.settings["time"]) ? "ON" : "OFF";

					ss << it->first << " Time Mode: " << status;
					it++;
					if (it != this->activeAirports.end()) ss << " / ";
				}
				messageHandler->writeMessage("INFO", ss.str());
			}
			else if (command.size() == 3)
			{
				if (this->activeAirports.contains(vsid::utils::toupper(command[2])))
				{
					if (this->activeAirports[vsid::utils::toupper(command[2])].settings["time"])
					{
						this->activeAirports[vsid::utils::toupper(command[2])].settings["time"] = 0;
					}
					else
					{
						this->activeAirports[vsid::utils::toupper(command[2])].settings["time"] = 1;
					}
					messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " Time Mode: " +
						((this->activeAirports[vsid::utils::toupper(command[2])].settings["time"]) ? "ON" : "OFF")
					);
					this->UpdateActiveAirports();
				}
				else messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " is not in active airports");
			}
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "auto")
		{	
			std::string atcSI = ControllerMyself().GetPositionId();
			std::string atcIcao;

			// DEV
			for (EuroScopePlugIn::CController ctr = this->ControllerSelectFirst(); ctr.IsValid(); ctr = this->ControllerSelectNext(ctr))
			{
				messageHandler->writeMessage("DEBUG", "[ControllerSelect] Callsign: " + std::string(ctr.GetCallsign()) +
					"; SI: " + std::string(ctr.GetPositionId()), vsid::MessageHandler::DebugArea::Atc);
			}
			// END DEV
			
			try
			{
				atcIcao = vsid::utils::split(ControllerMyself().GetCallsign(), '_').at(0);
			}
			catch (std::out_of_range)
			{
				messageHandler->writeMessage("ERROR", "Failed to get own ATC ICAO for automode. Code: " + ERROR_CMD_ATCICAO);
			}

			if (!ControllerMyself().IsController())
			{
				messageHandler->writeMessage("ERROR", "Automode not available for observer");
				return true;
			}
			if (command.size() == 2)
			{
				int counter = 0;
				std::ostringstream ss;
				ss << "Automode ON for: ";
				for (auto it = this->activeAirports.begin(); it != this->activeAirports.end(); ++it)
				{
					if (ControllerMyself().GetFacility() >= 2 && ControllerMyself().GetFacility() <= 4 && atcIcao != it->second.icao)
					{
						messageHandler->writeMessage("DEBUG", "[" + it->second.icao + "] Skipping auto mode because own ATC ICAO does not match", vsid::MessageHandler::DebugArea::Atc);
						continue;
					}
					else if (ControllerMyself().GetFacility() > 4 && !it->second.appSI.contains(atcSI) && atcIcao != it->second.icao)
					{
						messageHandler->writeMessage("DEBUG", "[" + it->second.icao + "] Skipping auto mode because own SI is not in apt appSI or own ATC ICAO does not match",
													vsid::MessageHandler::DebugArea::Atc
						);
						continue;
					}
					if (!it->second.settings["auto"] && it->second.controllers.size() == 0)
					{
						it->second.settings["auto"] = true;
						ss << it->first << " ";
						counter++;
					}
					else if (!it->second.settings["auto"] &&
							it->second.controllers.size() > 0 &&
							!it->second.hasLowerAtc(ControllerMyself(), true))
					{
						it->second.settings["auto"] = true;
						ss << it->first << " ";
						counter++;
					}
					else if(!it->second.settings["auto"])
					{
						messageHandler->writeMessage("INFO", "[" + it->first + "] Cannot activate automode. Lower or same level controller online.");
						continue;
					}
				}

				if (counter > 0)
				{
					messageHandler->writeMessage("INFO", ss.str());

					// remove processed flight plans if they're not cleared or if the set rwy is not part of depRwys anymore

					std::erase_if(this->processed, [&](const std::pair<std::string, vsid::Fpln>& pFpln)
						{
							auto& [callsign, fplnInfo] = pFpln;

							EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(callsign.c_str());
							EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
							std::string adep = fplnData.GetOrigin();

							messageHandler->writeMessage("DEBUG", "[" + std::string(fpln.GetCallsign()) + "] for erase on auto mode activation", vsid::MessageHandler::DebugArea::Dev);
							
							if (this->activeAirports.contains(adep) &&
								this->activeAirports[adep].settings["auto"] &&
								!fpln.GetClearenceFlag() && !fplnInfo.atcRWY
								)
							{
								messageHandler->writeMessage("DEBUG", "[" + std::string(fpln.GetCallsign()) + "] erased on auto mode activation", vsid::MessageHandler::DebugArea::Dev);

								vsid::fplnhelper::saveFplnInfo(callsign, fplnInfo, this->savedFplnInfo);

								return true;
							}
							else return false;
						}
					);
				}
				else messageHandler->writeMessage("INFO", "No new automode. Check .vsid auto status for active ones.");
			}
			else if (command.size() > 2 && vsid::utils::tolower(command[2]) == "status")
			{
				std::ostringstream ss;
				ss << "Automode active for: ";
				int counter = 0;
				for (auto it = this->activeAirports.begin(); it != this->activeAirports.end(); ++it)
				{
					if (it->second.settings["auto"])
					{
						ss << it->first << " ";
						counter++;
					}
				}
				if (counter > 0) messageHandler->writeMessage("INFO", ss.str());
				else messageHandler->writeMessage("INFO", "No active automode.");
			}
			else if (command.size() > 2 && vsid::utils::tolower(command[2]) == "off")
			{
				for (auto it = this->activeAirports.begin(); it != this->activeAirports.end(); ++it)
				{
					it->second.settings["auto"] = false;
				}
				messageHandler->writeMessage("INFO", "Automode OFF for all airports.");
			}
			else if (command.size() > 2)
			{
				for (std::vector<std::string>::iterator it = command.begin() + 2; it != command.end(); ++it)
				{
					*it = vsid::utils::toupper(*it);
					if (this->activeAirports.contains(*it))
					{
						if (ControllerMyself().GetFacility() >= 2 && ControllerMyself().GetFacility() <= 4 && atcIcao != *it)
						{
							messageHandler->writeMessage("DEBUG", "[" + *it + "] Skipping force auto mode because own ATC ICAO does not match",
														vsid::MessageHandler::DebugArea::Atc
							);
							continue;
						}
						else if (ControllerMyself().GetFacility() > 4 && !this->activeAirports[*it].appSI.contains(atcSI) && atcIcao != *it)
						{
							messageHandler->writeMessage("DEBUG", "[" + *it + "] Skipping force auto mode because own SI is not in apt appSI or own ATC ICAO does not match",
														vsid::MessageHandler::DebugArea::Atc
														);
							continue;
						}

						this->activeAirports[*it].settings["auto"] = !this->activeAirports[*it].settings["auto"];
						messageHandler->writeMessage("INFO", *it + " automode is: " + ((this->activeAirports[*it].settings["auto"]) ? "ON" : "OFF"));

						if (this->activeAirports[*it].settings["auto"])
						{
							// remove processed flight plans if they're not cleared or if the set rwy is not part of depRwys anymore

							std::erase_if(this->processed, [&](const std::pair<std::string, vsid::Fpln>& pFpln)
								{
									auto& [callsign, fplnInfo] = pFpln;
									EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(callsign.c_str());
									EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
									std::string adep = fplnData.GetOrigin();
									
									if (*it == adep && !fpln.GetClearenceFlag() && !fplnInfo.atcRWY)
									{
										vsid::fplnhelper::saveFplnInfo(callsign, fplnInfo, this->savedFplnInfo);

										return true;
									}
									else return false;
								}
							);
						}
						if (this->activeAirports[*it].settings["auto"] && this->activeAirports[*it].hasLowerAtc(ControllerMyself()))
						{
							this->activeAirports[*it].forceAuto = true;
						}
						else if (!this->activeAirports[*it].settings["auto"])
						{
							this->activeAirports[*it].forceAuto = false;
						}
					}
					else messageHandler->writeMessage("INFO", "[" + *it + "] not in active airports. Cannot set automode");
				}
			}
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "area")
		{
			if (command.size() == 2)
			{
				for (std::pair<const std::string, vsid::Airport>& apt : this->activeAirports)
				{
					if (!apt.second.areas.empty())
					{
						std::ostringstream ss;
						for (std::pair<const std::string, vsid::Area>& area : apt.second.areas)
						{
							std::string status = (area.second.isActive) ? "ON" : "OFF";
							ss << area.first << ": " << status << " ";
						}
						messageHandler->writeMessage(apt.first + " Areas", ss.str());
					}
					else messageHandler->writeMessage(apt.first + " Areas", "No area settings configured");
				}
			}
			else if (command.size() >= 3 && this->activeAirports.contains(vsid::utils::toupper(command[2])))
			{
				std::string icao = vsid::utils::toupper(command[2]);
				if (command.size() == 3)
				{
					if (!this->activeAirports[icao].areas.empty())
					{
						std::ostringstream ss;
						for (std::pair<const std::string, vsid::Area>& area : this->activeAirports[icao].areas)
						{
							std::string status = (area.second.isActive) ? "ON " : "OFF ";
							ss << area.first << ": " << status;
						}
						messageHandler->writeMessage(icao + " Areas", ss.str());
					}
					else messageHandler->writeMessage(icao + " Areas", "No area settings configured");
				}
				else if (command.size() == 4)
				{
					std::string area = vsid::utils::toupper(command[3]);
					if (!this->activeAirports[icao].areas.empty() && area == "ALL")
					{
						std::ostringstream ss;
						for (std::pair<const std::string, vsid::Area> &el : this->activeAirports[icao].areas)
						{
							el.second.isActive = true;
							ss << el.first << ": " << ((el.second.isActive) ? "ON " : "OFF ");
						}
						messageHandler->writeMessage(icao + " Areas", ss.str());
					}
					else if (!this->activeAirports[icao].areas.empty() && area == "OFF")
					{
						std::ostringstream ss;
						for (std::pair<const std::string, vsid::Area>& el : this->activeAirports[icao].areas)
						{
							el.second.isActive = false;
							ss << el.first << ": " << ((el.second.isActive) ? "ON " : "OFF ");
						}
						messageHandler->writeMessage(icao + " Areas", ss.str());
					}
					else if (this->activeAirports[icao].areas.contains(area))
					{
						this->activeAirports[icao].areas[area].isActive = !this->activeAirports[icao].areas[area].isActive;
						messageHandler->writeMessage(icao + " Area", area + " " +
													(this->activeAirports[icao].areas[area].isActive ? "ON" : "OFF")
													);
					}
					else messageHandler->writeMessage(icao + " " + command[3], "Area is unknown");
					
					std::erase_if(this->processed, [&](const std::pair<std::string, vsid::Fpln>& pFpln)
						{
							auto& [callsign, fplnInfo] = pFpln;
							EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(callsign.c_str());
							EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();

							if (fpln.IsValid() &&
								!fpln.GetClearenceFlag() &&
								this->activeAirports.contains(fplnData.GetOrigin()) &&
								this->activeAirports[fplnData.GetOrigin()].settings["auto"]
								)
							{
								vsid::fplnhelper::saveFplnInfo(callsign, fplnInfo, this->savedFplnInfo);

								return true;
							}
							else return false;
						}
					);

					for (auto &[callsign, fpln] : this->processed)
					{
						EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());
						auto atcBlock = vsid::fplnhelper::getAtcBlock(FlightPlan);
						messageHandler->writeMessage("DEBUG", "[" + callsign + "] Rechecking due to area change.", vsid::MessageHandler::DebugArea::Sid);

						if (atcBlock.second != "" && FlightPlan.IsValid())
						{
							this->processFlightplan(FlightPlan, true, atcBlock.second);
						}
						else if(FlightPlan.IsValid()) this->processFlightplan(FlightPlan, true);
					}
				}
			}
			else messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " not in active airports");
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "sync")
		{
			messageHandler->writeMessage("DEBUG", "Syncinc all requests.", vsid::MessageHandler::DebugArea::Req);

			for (auto& [callsign, fpln] : this->processed)
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] sync processing...", vsid::MessageHandler::DebugArea::Dev);
				EuroScopePlugIn::CFlightPlan FlightPlan = FlightPlanSelect(callsign.c_str());
				
				if (!FlightPlan.IsValid()) continue;

				std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
				std::string ades = FlightPlan.GetFlightPlanData().GetDestination();

				// sync requests

				this->syncReq(FlightPlan);
				
				// sync states

				if (this->activeAirports.contains(adep))
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] calling sync state.", vsid::MessageHandler::DebugArea::Dev);
					this->syncStates(FlightPlan);
				}

				// sync cleared to land flag

				if (this->activeAirports.contains(ades))
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] syncing ctlf.", vsid::MessageHandler::DebugArea::Dev);

					std::string newScratch = ".VSID_CTL_" + std::string((fpln.ctl) ? "TRUE" : "FALSE");

					this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
				}

				// sync intersections

				if (this->activeAirports.contains(adep))
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] syncing intersection.", vsid::MessageHandler::DebugArea::Dev);

					if (std::string intersection = fpln.intsec.first; intersection != "")
					{
						std::string newScratch = ".VSID_INT_" + intersection + "_" + ((fpln.intsec.second) ? "TRUE" : "FALSE");

						this->addSyncQueue(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
					}
				}
			}
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "req")
		{
			if (command.size() >= 3)
			{
				std::string icao = vsid::utils::toupper(command[2]);

				if (!this->activeAirports.contains(icao))
				{
					messageHandler->writeMessage("WARNING", icao + " not in active airports. Cannot check for requests");
					return true;
				}

				std::ostringstream ss;

				if (command.size() == 3)
				{
					for (auto& req : this->activeAirports[icao].requests)
					{
						for (std::set<std::pair<std::string, long long>>::iterator it = req.second.begin(); it != req.second.end();)
						{
							ss << it->first;
							++it;
							if (it != req.second.end()) ss << ", ";
						}
						if (ss.str().size() == 0) ss << "No requests.";
						messageHandler->writeMessage("INFO", "[" + icao + "] " + req.first + " requests: " + ss.str());
						ss.str("");
						ss.clear();
					}

					for (auto& req : this->activeAirports[icao].rwyrequests)
					{
						for (auto& [rwy, rwyReq] : req.second)
						{
							for (std::set<std::pair<std::string, long long>>::iterator it = rwyReq.begin(); it != rwyReq.end();)
							{
								ss << it->first;
								++it;
								if (it != rwyReq.end()) ss << ", ";
							}
							if (ss.str().size() == 0) ss << "No requests.";
							messageHandler->writeMessage("INFO", "[" + icao + "] " + req.first + " (" + rwy + ")" + " requests: " + ss.str());
							ss.str("");
							ss.clear();
						}					
					}
				}
				else if (command.size() == 4 && vsid::utils::tolower(command[3]) == "reset")
				{
					bool failedReset = false;
					for (auto& [_, reqList] : this->activeAirports[icao].requests)
					{
						reqList.clear();

						if (reqList.size() != 0) failedReset = true;
					}

					if (!failedReset) messageHandler->writeMessage("INFO", icao +
						" all requests have been cleared.");
					else messageHandler->writeMessage("INFO", icao + " failed to reset requests.");
				}
				else if (command.size() == 5 && vsid::utils::tolower(command[3]) == "reset")
				{
					std::string req = vsid::utils::tolower(command[4]);

					if (!this->activeAirports[icao].requests.contains(req))
					{
						messageHandler->writeMessage("INFO", "Unknown request queue \"" + req + "\"");
						return true;
					}
					else this->activeAirports[icao].requests[req].clear();

					if (this->activeAirports[icao].requests[req].size() == 0) messageHandler->writeMessage("INFO", icao +
						" request queue \"" + req + "\" reset");
					else messageHandler->writeMessage("INFO", icao + " request queue \"" + req + "\" failed to reset");
				}
			}
			else messageHandler->writeMessage("INFO", "Missing parameters for request command");

			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "debug")
		{
			if (command.size() == 2)
			{
				messageHandler->setDebugArea("ALL");
				if (messageHandler->getLevel() != vsid::MessageHandler::Level::Debug)
				{
					messageHandler->setLevel("DEBUG");
					messageHandler->writeMessage("INFO", "DEBUG MODE: ON");
				}
				else
				{
					messageHandler->setLevel("INFO");
					messageHandler->writeMessage("INFO", "DEBUG MODE: OFF");
				}
				return true;
			}
			else if (command.size() == 3)
			{
				if (messageHandler->getLevel() != vsid::MessageHandler::Level::Debug && vsid::utils::toupper(command[2]) != "STATUS")
				{
					messageHandler->setLevel("DEBUG");
					messageHandler->setDebugArea(vsid::utils::toupper(command[2]));
					messageHandler->writeMessage("INFO", "DEBUG MODE: ON");
				}

				else if (vsid::utils::toupper(command[2]) == "STATUS")
				{
					std::ostringstream ss;
					ss << "DEBUG Mode: " << ((messageHandler->getLevel() == vsid::MessageHandler::Level::Debug) ? "ON" : "OFF");
					std::string area;
					if (messageHandler->getDebugArea() == vsid::MessageHandler::DebugArea::All) area = "ALL";
					else if (messageHandler->getDebugArea() == vsid::MessageHandler::DebugArea::Atc) area = "ATC";
					else if (messageHandler->getDebugArea() == vsid::MessageHandler::DebugArea::Sid) area = "SID";
					else if (messageHandler->getDebugArea() == vsid::MessageHandler::DebugArea::Rwy) area = "RWY";
					else if (messageHandler->getDebugArea() == vsid::MessageHandler::DebugArea::Dev) area = "DEV";

					ss << " - Area is: " << area;
					messageHandler->writeMessage("INFO", ss.str());
				}
				else if (messageHandler->setDebugArea(vsid::utils::toupper(command[2])))
				{
					messageHandler->writeMessage("INFO", "DEBUG AREA: " + vsid::utils::toupper(command[2]));
				}
				else messageHandler->writeMessage("INFO", "Unknown Debug Level");
				return true;
			}			
		}
		else if (vsid::utils::tolower(command[1]) == "reload")
		{
			if (command.size() == 2)
			{
				messageHandler->writeMessage("INFO", "Reloading main config...");
				this->configParser.loadMainConfig();
			}
			else if (command.size() == 3 && vsid::utils::tolower(command[2]) == "ese")
			{
				this->loadEse();
			}
			
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "ghversion")
		{
			if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
			{
				messageHandler->writeMessage("ERROR", "Failed to init curl_global");
				return true;
			}		

			vsid::version::checkForUpdates(this->getConfigParser().notifyUpdate, vsid::version::parseSemVer(pluginVersion));

			curl_global_cleanup();
			
			return true;
		}
		else
		{
			messageHandler->writeMessage("INFO", "Unknown command");
			return true;
		}
	}
	return false;
}

void vsid::VSIDPlugin::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	// if we receive updates for flight plans validate entered sid again to sync between controllers
	// updates are received for any flight plan not just that under control

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();
	const std::string adep = fplnData.GetOrigin();

	
	if (this->processed.contains(callsign))
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] flight plan updated", vsid::MessageHandler::DebugArea::Dev);

		//// check for the last updates to auto disable auto mode if needed
		//if (this->activeAirports[fplnData.GetOrigin()].settings["auto"])
		//{
		//	auto updateNow = vsid::time::getUtcNow();

		//	auto test = this->processed[callsign].lastUpdate - updateNow;

		//	/*messageHandler->writeMessage("DEBUG", callsign + " updateNow: " + vsid::time::toString(updateNow));
		//	messageHandler->writeMessage("DEBUG", callsign + " lastUpdate: " + vsid::time::toString(this->processed[callsign].lastUpdate));
		//	messageHandler->writeMessage("DEBUG", callsign + " difference in seconds: " + std::string(std::format("{:%H:%M:%S}", test)));*/

		//	/*if (test >= 3 &&
		//		this->processed[callsign].updateCounter > 3)
		//	{
		//		this->activeAirports[fplnData.GetOrigin()].settings["auto"] = false;
		//		messageHandler->writeMessage("WARNING", "Automode disabled for " +
		//									std::string(fplnData.GetOrigin()) + 
		//									" due to more than 3 flight plan changes in the last 3 seconds");
		//		this->processed[callsign].updateCounter = 1;
		//	}
		//	else if()*/

		//	this->processed[callsign].lastUpdate = updateNow;
		//}

		std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');

		if (filedRoute.size() > 0)
		{
			std::pair<std::string, std::string> atcBlock = vsid::fplnhelper::getAtcBlock(FlightPlan);
			if (this->activeAirports.contains(adep) && this->activeAirports[adep].settings["auto"] &&
				atcBlock.first == "")
			{
				vsid::fplnhelper::saveFplnInfo(callsign, this->processed[callsign], this->savedFplnInfo);
				this->processed.erase(callsign);
				return;
			}

			if (!this->processed[callsign].atcRWY && atcBlock.second != "" &&
				(fplnData.IsAmended() || FlightPlan.GetClearenceFlag() ||
				atcBlock.first != adep ||
				(atcBlock.first == adep && vsid::fplnhelper::findRemarks(FlightPlan, "VSID/RWY")))
				)
			{
				this->processed[callsign].atcRWY = true;
			}

			if (vsid::fplnhelper::findRemarks(FlightPlan, "VSID/RWY") &&
				atcBlock.first != adep &&
				ControllerMyself().IsController()
				)
			{
				this->processed[callsign].noFplnUpdate = true;

				if (!vsid::fplnhelper::removeRemark(FlightPlan, "VSID/RWY"))
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_REMARKRMV))
					{
						messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to remove remarks! Code: " + ERROR_FPLN_REMARKRMV);
						messageHandler->addFplnError(callsign, ERROR_FPLN_REMARKRMV);
					}

					this->processed[callsign].noFplnUpdate = false;
				}
				else messageHandler->removeFplnError(callsign, ERROR_FPLN_REMARKRMV);
				//else this->processed[callsign].noFplnUpdate = false;

				if (!fplnData.AmendFlightPlan())
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
					{
						messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! - #FFDU");
						messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
					}

					this->processed[callsign].noFplnUpdate = false;
				}
				else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
				//else this->processed[callsign].noFplnUpdate = false;
			}

			// DEV
			// if we want to suppress an update that happened and skip sid checking  // #evaluate needs further checking - this the right position? now several flightplanupdates again
			if (this->processed[callsign].noFplnUpdate)
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] nofplnUpdate true. Disabling.", vsid::MessageHandler::DebugArea::Dev);
				this->processed[callsign].noFplnUpdate = false;
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] nofplnUpdate after disabling " + ((this->processed[callsign].noFplnUpdate) ? "TRUE" : "FALSE"),
											vsid::MessageHandler::DebugArea::Dev);
				return;
			}

			// END DEV

			if (atcBlock.first == adep && atcBlock.second != "" && this->activeAirports.contains(adep))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] fpln updated, calling processFlightplan with atcRwy : " + atcBlock.second,
											vsid::MessageHandler::DebugArea::Sid
				);
				this->processFlightplan(FlightPlan, true, atcBlock.second);

				// update possible rwy requests

				if (this->processed[callsign].request != "")
				{
					std::string fplnRwy = fplnData.GetDepartureRwy();

					// update rwy requests directly if a rwy request is stored for the flight plan

					if (this->processed[callsign].request.find("rwy") != std::string::npos && fplnRwy != "")
					{
						try
						{
							bool stop = false;

							for (auto& [rwy, rwyReq] : this->activeAirports[adep].rwyrequests.at(this->processed[callsign].request))
							{
								for (std::set<std::pair<std::string, long long>>::iterator it = rwyReq.begin(); it != rwyReq.end();)
								{
									if (it->first != callsign)
									{
										++it;
										continue;
									}

									if (rwy != fplnRwy)
									{
										this->activeAirports[adep].rwyrequests.at(this->processed[callsign].request)[fplnRwy].insert({ callsign, it->second });
										rwyReq.erase(it);
										stop = true;
										break;
									}
								}
								if (stop) break;
							}
						}
						catch (std::out_of_range) {};
					}
					// check if a rwy request is available for stored non-rwy request and update the rwy
					else if(fplnRwy != "")
					{
						bool stop = false;
						for (auto& [type, rwys] : this->activeAirports[adep].rwyrequests)
						{
							if (type.find(this->processed[callsign].request) == std::string::npos) continue;

							for (auto& [rwy, rwyReq] : rwys)
							{
								for (std::set<std::pair<std::string, long long>>::iterator it = rwyReq.begin(); it != rwyReq.end();)
								{
									if (it->first != callsign)
									{
										++it;
										continue;
									}

									if (rwy != fplnRwy)
									{
										this->activeAirports[adep].rwyrequests.at(type)[fplnRwy].insert({ callsign, it->second });
										rwyReq.erase(it);
										stop = true;
										break;
									}
								}
								if (stop) break;
							}
							if (stop) break;
						}
					}
				}
			}
			else if (atcBlock.first != adep && atcBlock.second != "" && this->activeAirports.contains(adep))
			{
				vsid::Sid atcSid;
				for (vsid::Sid& sid : this->activeAirports[adep].sids)
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
				}
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] fpln updated, calling processFlightplan with atcRwy : " +
											atcBlock.second + " and atcSid : " + atcSid.name(),
											vsid::MessageHandler::DebugArea::Sid);
				this->processFlightplan(FlightPlan, true, atcBlock.second, atcSid);
			}
			else if(this->activeAirports.contains(adep))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] fpln updated, calling processFlightplan without atcRwy",
											vsid::MessageHandler::DebugArea::Sid
				);
				this->processFlightplan(FlightPlan, true);
			}
		}
	}
}

void vsid::VSIDPlugin::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType)
{
	if (!FlightPlan.IsValid()) return;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	std::string ades = FlightPlan.GetFlightPlanData().GetDestination();
	std::string fplnRwy = vsid::fplnhelper::getAtcBlock(FlightPlan).second;

	if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_SCRATCH_PAD_STRING)
	{
		if (spWorkerActive)
		{
			messageHandler->writeMessage("DEBUG", "SP sync worker active. Skipping current scratch evaluation", vsid::MessageHandler::DebugArea::Dev);
			spWorkerActive = false;
			return;
		}

		std::string scratchpad = vsid::utils::toupper(cad.GetScratchPadString());

		messageHandler->writeMessage("DEBUG", "[" + callsign + "] Scratchpad entry : " + scratchpad, vsid::MessageHandler::DebugArea::Dev);

		// set clearance flag

		if (scratchpad.find(".VSID_CTL_") != std::string::npos)
		{
			std::string toFind = ".VSID_CTL_";
			size_t pos = scratchpad.find(".VSID_CTL_");

			bool ctl = scratchpad.substr(pos + toFind.size(), scratchpad.size()) == "TRUE" ? true : false;

			this->processed[callsign].ctl = ctl; // #evaluate - setting 'false' could delete from processed if ades is not active (protection against too many entries)

			this->updateSPSyncRelease(callsign);
		}

		// set intersection

		if (this->processed.contains(callsign) && scratchpad.size() > 0)
		{
			if (scratchpad.size() <= 4 && this->activeAirports.contains(adep))
			{
				if (size_t pos = scratchpad.find("+"); pos != std::string::npos)
				{
					std::string intsec = scratchpad.substr(pos + 1, scratchpad.size());

					if (this->activeAirports[adep].intsec.contains(fplnRwy) && vsid::utils::contains(this->activeAirports[adep].intsec[fplnRwy], intsec)) // #continue - check for spReleased
					{
						this->processed[callsign].intsec = { intsec, true };
						FlightPlan.GetControllerAssignedData().SetScratchPadString("");
					}
				}
				else if (size_t pos = scratchpad.find("-"); pos != std::string::npos)
				{
					std::string intsec = scratchpad.substr(pos + 1, scratchpad.size());

					if (this->activeAirports[adep].intsec.contains(fplnRwy) && vsid::utils::contains(this->activeAirports[adep].intsec[fplnRwy], intsec))
					{
						this->processed[callsign].intsec = { intsec, false };
						FlightPlan.GetControllerAssignedData().SetScratchPadString("");
					}
				}
			}

			// check for multiple auto-mode users

			if (scratchpad.find(".VSID_AUTO_") != std::string::npos)
			{
				std::string toFind = ".VSID_AUTO_";
				size_t pos = scratchpad.find(toFind);

				if (this->activeAirports.contains(adep) && this->activeAirports[adep].settings["auto"] && pos != std::string::npos)
				{
					std::string atc = scratchpad.substr(pos + toFind.size(), scratchpad.size());
					if (ControllerMyself().GetCallsign() != atc)
					{
						messageHandler->writeMessage("WARNING", "[" + callsign + "] assigned SID \"" + FlightPlan.GetFlightPlanData().GetSidName() + "\" by " + atc);
					}
				}

				this->updateSPSyncRelease(callsign);
			}

			// sync release if GRP states are synced - ES is released on gnd state updates

			if (scratchpad.find("NOSTATE") != std::string::npos)
			{
				this->processed[callsign].gndState = "NOSTATE";
				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}
			if (scratchpad.find("ONFREQ") != std::string::npos)
			{
				this->processed[callsign].gndState = "ONFREQ";
				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}
			if (scratchpad.find("DE-ICE") != std::string::npos)
			{
				this->processed[callsign].gndState = "DE-ICE";
				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}
			if (scratchpad.find("LINEUP") != std::string::npos)
			{
				this->processed[callsign].gndState = "LINEUP";
				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}

			// clearance flag released while sending

			// request entries

			if (scratchpad.find(".VSID_REQ_") != std::string::npos)
			{
				std::string toFind = ".VSID_REQ_";
				size_t pos = scratchpad.find(toFind);

				messageHandler->writeMessage("DEBUG", "[" + callsign + "] found \".vsid_req_\" in scratch: " + scratchpad, vsid::MessageHandler::DebugArea::Req);

				try
				{
					std::vector<std::string> req = vsid::utils::split(scratchpad.substr(pos + toFind.size(), scratchpad.size()), '/');
					std::string reqType = vsid::utils::tolower(req.at(0));
					long long reqTime = std::stoll(req.at(1));

					// clear all possible requests before setting a new one

					if (this->activeAirports.contains(adep))
					{
						bool reqActive = false; // preserves active req state which would be overwritten if a req list resulting in false comes after

						for (auto it = this->activeAirports[adep].requests.begin(); it != this->activeAirports[adep].requests.end(); ++it)
						{
							for (std::set<std::pair<std::string, long long>>::iterator jt = it->second.begin(); jt != it->second.end();)
							{
								if (jt->first != callsign)
								{
									++jt;
									continue;
								}

								if (!reqActive)
								{
									this->processed[callsign].request = "";
									this->processed[callsign].reqTime = -1;
								}

								messageHandler->writeMessage("DEBUG", "[" + callsign + "] removing from requests in: " +
									it->first, vsid::MessageHandler::DebugArea::Req);

								jt = it->second.erase(jt);
							}
							if (it->first == reqType)
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] setting in requests in: " +
									it->first, vsid::MessageHandler::DebugArea::Req);

								it->second.insert({ callsign, reqTime });
								this->processed[callsign].request = reqType;
								this->processed[callsign].reqTime = reqTime;
								reqActive = true;
							}
							else if (reqType.find(it->first) != std::string::npos)
							{
								it->second.insert({ callsign, reqTime });
							}
						}

						for (auto& [type, rwys] : this->activeAirports[adep].rwyrequests)
						{
							messageHandler->writeMessage("DEBUG", "[" + callsign + "] rwy req type : " + type, vsid::MessageHandler::DebugArea::Dev);
							for (auto it = rwys.begin(); it != rwys.end(); ++it)
							{
								for (std::set<std::pair<std::string, long long>>::iterator jt = it->second.begin(); jt != it->second.end();)
								{
									if (jt->first != callsign)
									{
										++jt;
										continue;
									}

									if (!reqActive)
									{
										this->processed[callsign].request = "";
										this->processed[callsign].reqTime = -1;
									}

									messageHandler->writeMessage("DEBUG", "[" + callsign + "] removing from rwy requests in: " +
										type + "/" + it->first, vsid::MessageHandler::DebugArea::Req);

									jt = it->second.erase(jt);
								}
							}
							if (type == reqType && fplnRwy != "")
							{
								messageHandler->writeMessage("DEBUG", "[" + callsign + "] setting in rwy requests in: " +
									type + "/" + std::string(FlightPlan.GetFlightPlanData().GetDepartureRwy()), vsid::MessageHandler::DebugArea::Req);

								rwys[fplnRwy].insert({ callsign, reqTime });
								this->processed[callsign].request = reqType;
								this->processed[callsign].reqTime = reqTime;
								reqActive = true;
							}
							else if (type.find(reqType) != std::string::npos && fplnRwy != "") // #evaluate - if still needed
							{
								rwys[fplnRwy].insert({ callsign, reqTime });
							}
							else if (type == reqType && fplnRwy == "")
								messageHandler->writeMessage("WARNING", "[" + callsign + "] to be set in runway requests, but runway hasn't been set in the flight plan.");
						}
					}
					else messageHandler->writeMessage("DEBUG", "[" + callsign + "] apt " + adep + " is not an active airport in req setting", vsid::MessageHandler::DebugArea::Dev);

					messageHandler->removeFplnError(callsign, ERROR_FPLN_REQSET);
				}
				catch (std::out_of_range)
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_REQSET))
					{
						messageHandler->writeMessage("ERROR", "[" + callsign + "] failed to set the request");
						messageHandler->addFplnError(callsign, ERROR_FPLN_REQSET);
					}
				}

				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}

			// intersections

			if (scratchpad.find(".VSID_INT_") != std::string::npos)
			{
				std::string toFind = ".VSID_INT_";
				size_t pos = scratchpad.find(toFind);

				try
				{
					std::vector<std::string> intersection = vsid::utils::split(scratchpad.substr(pos + toFind.size(), scratchpad.size()), '_');

					if (intersection.at(0) == "NONE") this->processed[callsign].intsec = { "", false };
					else this->processed[callsign].intsec = { intersection.at(0), ((intersection.at(1) == "TRUE") ? true : false) };

					messageHandler->removeFplnError(callsign, ERROR_FPLN_INTSET);
				}
				catch (std::out_of_range)
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_INTSET))
					{
						messageHandler->writeMessage("ERROR", "[" + callsign + "] failed to set the intersection. Code: " + ERROR_FPLN_INTSET);
						messageHandler->addFplnError(callsign, ERROR_FPLN_INTSET);
					}
				}

				if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
			}
		}
	}

	if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE) // updating sync release for ES states as they're not always seen in scratch pad
	{
		if (this->spReleased.contains(callsign)) this->updateSPSyncRelease(callsign);
	}

	if (this->activeAirports.contains(adep))
	{
		// get ES gnd states

		if (this->processed.contains(callsign) && DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE)
		{
			this->processed[callsign].gndState = FlightPlan.GetGroundState();

			if (this->processed[callsign].gndState == "DEPA") this->processed[callsign].intsec = { "", false };
		}

		// remove requests if present - might also trigger without a present scratchpad

		if (this->processed.contains(callsign) && this->processed[callsign].request != "")
		{
			if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_CLEARENCE_FLAG)
			{
				if (FlightPlan.GetClearenceFlag())
				{
					for (auto& fp : this->activeAirports[adep].requests["clearance"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["clearance"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}
				}
			}
			if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE)
			{
				std::string state = FlightPlan.GetGroundState();

				if (state == "STUP")
				{
					for (auto& fp : this->activeAirports[adep].requests["startup"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["startup"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}

					for (auto& fp : this->activeAirports[adep].requests["rwy startup"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["rwy startup"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}
				}
				else if (state == "PUSH")
				{
					for (auto& fp : this->activeAirports[adep].requests["pushback"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["pushback"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}
				}
				else if (state == "TAXI")
				{
					for (auto& fp : this->activeAirports[adep].requests["taxi"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["taxi"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}

				}
				else if (state == "DEPA")
				{
					for (auto& fp : this->activeAirports[adep].requests["departure"])
					{
						if (fp.first != callsign) continue;

						this->activeAirports[adep].requests["departure"].erase(fp);
						this->processed[callsign].request = "";
						this->processed[callsign].reqTime = -1;
						break;
					}
				}
			}
		}
	}
}

void vsid::VSIDPlugin::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	std::string callsign = FlightPlan.GetCallsign();
	std::string icao = FlightPlan.GetFlightPlanData().GetOrigin();

	if (this->processed.contains(callsign))
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] disconnected from the network.", vsid::MessageHandler::DebugArea::Fpln);

		vsid::fplnhelper::saveFplnInfo(callsign, this->processed[callsign], this->savedFplnInfo);

		this->processed.erase(callsign);

		this->removeFromRequests(callsign, icao);

		this->removeProcessed[callsign] = { std::chrono::utc_clock::now() + std::chrono::minutes{1}, true };

		messageHandler->removeCallsignFromErrors(callsign);
	}
}

void vsid::VSIDPlugin::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget RadarTarget)
{
	if (!RadarTarget.IsValid()) return;

	std::string callsign = RadarTarget.GetCallsign();
	std::string adep = RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetOrigin();
	std::string ades = RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetDestination();
	
	if (this->processed.contains(callsign) && this->processed[callsign].ldgAlt != 0 && this->activeAirports.contains(ades))
	{
		int alt = RadarTarget.GetPosition().GetPressureAltitude();

		if (alt <= std::abs(this->processed[callsign].ldgAlt - 200)) this->processed[callsign].ldgAlt = alt;
		else if (alt >= this->processed[callsign].ldgAlt + 200)
		{
			this->processed[callsign].mapp = true; // #continue - send mapp to network if not already set
			this->processed[callsign].ctl = false;
		}
	}

	// trigger when speed is >= 50 knots
	if (this->processed.contains(callsign) && this->activeAirports.contains(adep) &&
		RadarTarget.GetGS() >= 50)
	{
		// remove requests that might still be present
		this->removeFromRequests(callsign, adep);

		// remove rwy remark if still present
		if (vsid::fplnhelper::findRemarks(RadarTarget.GetCorrelatedFlightPlan(), "VSID/RWY"))
		{
			EuroScopePlugIn::CFlightPlan fpln = RadarTarget.GetCorrelatedFlightPlan();
			vsid::fplnhelper::removeRemark(fpln, "VSID/RWY");

			if (!fpln.GetFlightPlanData().AmendFlightPlan())
			{
				if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! - #FFDU");
					messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
				}
			}
			else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
		}

		// remove from intersections that might still be present

		this->processed[callsign].intsec = { "", false };
	}

	// remove arriving tfc

	else if (this->processed.contains(callsign) && RadarTarget.GetGS() < 50 && !this->activeAirports.contains(adep))
	{
		if (adep != ades && adep != "")
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] arrived. Removing from processed.", vsid::MessageHandler::DebugArea::Dev);

			if (this->savedFplnInfo.contains(callsign)) this->savedFplnInfo.erase(callsign);
			this->processed.erase(callsign);
		}
	}
}

void vsid::VSIDPlugin::OnControllerPositionUpdate(EuroScopePlugIn::CController Controller)
{
	std::string atcCallsign = Controller.GetCallsign();
	std::string atcSI = Controller.GetPositionId();
	int atcFac = Controller.GetFacility();
	double atcFreq = Controller.GetPrimaryFrequency();
	std::string atcIcao;

	try
	{
		atcIcao = vsid::utils::split(Controller.GetCallsign(), '_').at(0);
		messageHandler->removeGenError(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign);
	}
	catch (std::out_of_range)
	{
		if (!messageHandler->genErrorsContains(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign))
		{
			messageHandler->writeMessage("ERROR", "Failed to get ICAO part of controller callsign \"" + atcCallsign +
				"\" in ATC update. Code: " + ERROR_ATC_ICAOSPLIT);
			messageHandler->addGenError(ERROR_ATC_ICAOSPLIT + "_" + atcCallsign);
		}
	}

	if (atcCallsign == ControllerMyself().GetCallsign()) return;
	if (this->actAtc.contains(atcSI) || this->ignoreAtc.contains(atcSI)) return;

	// maximum 3 attempts to try and match the callsign or frequency against ese stored atc stations

	if (this->atcSiFailCounter.contains(atcCallsign) && this->atcSiFailCounter[atcCallsign] > 2 && this->atcSiFailCounter[atcCallsign] < 6)
	{
		for (const vsid::SectionAtc &sAtc : this->sectionAtc)
		{
			if (atcCallsign == sAtc.callsign || atcFreq == sAtc.freq)
			{
				atcSI = sAtc.si;

				messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] match found in parsed stations. Setting SI: " + atcSI,
					vsid::MessageHandler::DebugArea::Atc);

				break;
			}
		}
	}
	
	if (!Controller.IsController())
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because it is not a controller.",
			vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}
	
	if (atcCallsign.find("ATIS") != std::string::npos)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Adding ATIS to ignore list.",
			vsid::MessageHandler::DebugArea::Atc
		);
		this->ignoreAtc.insert(atcSI);
		return;
	}

	if (atcFreq < 0.1 || atcFreq > 199.0)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the freq. is invalid (" + std::to_string(atcFreq) + ").",
			vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}
	
	if (atcSI.empty())
	{
		if (this->atcSiFailCounter.contains(atcCallsign)) this->atcSiFailCounter[atcCallsign]++;
		else this->atcSiFailCounter[atcCallsign] = 1;

		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the SI is empty. Failed SI count: " +
			std::to_string(this->atcSiFailCounter[atcCallsign]), vsid::MessageHandler::DebugArea::Atc);

		return;
	}
	else if (std::all_of(atcSI.begin(), atcSI.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
	{
		if (this->atcSiFailCounter.contains(atcCallsign)) this->atcSiFailCounter[atcCallsign]++;
		else this->atcSiFailCounter[atcCallsign] = 1;

		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the SI contains a number (SI: " + atcSI +
			"). Failed SI count: " + std::to_string(this->atcSiFailCounter[atcCallsign]), vsid::MessageHandler::DebugArea::Atc);

		return;
	}
	else if (this->atcSiFailCounter.contains(atcCallsign))
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] removing from SI Fail Counter after SI is valid (SI: " + atcSI + ")",
			vsid::MessageHandler::DebugArea::Atc);

		this->atcSiFailCounter.erase(atcCallsign);
	}
	
	if (atcFac < 2)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the facility is below 2 (usually FIS).",
									vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}

	EuroScopePlugIn::CController atcMyself = ControllerMyself();
	std::set<std::string> atcIcaos;
	if (atcFac < 6 && atcIcao != "")
	{
		atcIcaos.insert(atcIcao);
	}

	if (atcFac >= 5 && !this->actAtc.contains(atcSI) && !this->ignoreAtc.contains(atcSI))
	{
		bool ignore = true;
		for (std::pair<const std::string, vsid::Airport> &apt : this->activeAirports)
		{
			if (apt.second.appSI.contains(atcSI))
			{
				ignore = false;
				atcIcaos.insert(apt.first);
			}
			else if (apt.first == atcIcao)
			{
				ignore = false;
			}
		}
		if (ignore)
		{
			messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Adding ATC to ignore list",
										vsid::MessageHandler::DebugArea::Atc
			);
			this->ignoreAtc.insert(atcSI);
			return;
		}
	}

	if (std::none_of(atcIcaos.begin(), atcIcaos.end(), [&](auto atcIcao)
		{
			return this->activeAirports.contains(atcIcao);
		})) return;

	for (const std::string& atcIcao : atcIcaos)
	{
		if (!this->activeAirports.contains(atcIcao)) continue;

		if (!this->activeAirports[atcIcao].controllers.contains(atcSI))
		{
			this->activeAirports[atcIcao].controllers[atcSI] = { atcSI, atcFac, atcFreq };
			this->actAtc[atcSI] = vsid::utils::join(atcIcaos, ',');
			messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Adding ATC to active ATC list",
										vsid::MessageHandler::DebugArea::Atc
			);
		}

		if (this->activeAirports[atcIcao].settings["auto"] &&
			!this->activeAirports[atcIcao].forceAuto &&
			this->activeAirports[atcIcao].hasLowerAtc(atcMyself))
		{
			this->activeAirports[atcIcao].settings["auto"] = false;
			messageHandler->writeMessage("INFO", "Disabling auto mode for " +
				atcIcao + ". " + atcCallsign + " now online."
			);
		}
	}
}

void vsid::VSIDPlugin::OnControllerDisconnect(EuroScopePlugIn::CController Controller)
{
	std::string atcCallsign = Controller.GetCallsign();
	std::string atcSI = Controller.GetPositionId();

	if (this->actAtc.contains(atcSI) && this->activeAirports.contains(this->actAtc[atcSI]))
	{
		if (this->activeAirports[this->actAtc[atcSI]].controllers.contains(atcSI))
		{
			messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] disconnected. Removing from ATC list for " + this->actAtc[atcSI],
										vsid::MessageHandler::DebugArea::Atc
			);
			this->activeAirports[this->actAtc[atcSI]].controllers.erase(atcSI);
		}
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] disconnected. Removing from general active ATC list",
									vsid::MessageHandler::DebugArea::Atc
		);
		this->actAtc.erase(atcSI);
	}

	if (this->ignoreAtc.contains(atcSI))
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] disconnected. Removing from ignore list" + this->actAtc[atcSI],
									vsid::MessageHandler::DebugArea::Atc
		);
		this->ignoreAtc.erase(atcSI);
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
	messageHandler->writeMessage("INFO", "Updating airports...");
	this->savedSettings.clear();
	this->savedRules.clear();
	this->savedAreas.clear();
	this->savedRequests.clear();
	this->savedRwyRequests.clear();

	for (std::pair<const std::string, vsid::Airport>& apt : this->activeAirports)
	{
		this->savedSettings.insert({ apt.first, apt.second.settings });
		this->savedRules.insert({ apt.first, apt.second.customRules });
		this->savedAreas.insert({ apt.first, apt.second.areas });
		this->savedRequests.insert({ apt.first, apt.second.requests });
		this->savedRwyRequests.insert({ apt.first, apt.second.rwyrequests });
	}

	for (std::map<std::string, vsid::Fpln>::iterator it = this->processed.begin(); it != this->processed.end();)
	{
		vsid::fplnhelper::saveFplnInfo(it->first, it->second, this->savedFplnInfo);
		it = this->processed.erase(it);
	}

	this->SelectActiveSectorfile();
	this->activeAirports.clear();
      
	// get active airports
	for (EuroScopePlugIn::CSectorElement sfe =	this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
												sfe.IsValid();
												sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT)
		)
	{
		if (sfe.IsElementActive(true))
		{
			this->activeAirports[vsid::utils::trim(sfe.GetName())] = vsid::Airport{};
			this->activeAirports[vsid::utils::trim(sfe.GetName())].icao = vsid::utils::trim(sfe.GetName());
		}
	}

	// get active rwys
	for (EuroScopePlugIn::CSectorElement sfe =	this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
												sfe.IsValid();
												sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY)
		)
	{
		if (this->activeAirports.contains(vsid::utils::trim(sfe.GetAirportName())))
		{
			std::string aptName = vsid::utils::trim(sfe.GetAirportName());
			if (sfe.IsElementActive(false, 0))
			{
				std::string rwyName = vsid::utils::trim(sfe.GetRunwayName(0));
				this->activeAirports[aptName].arrRwys.insert(rwyName);
			}
			if (sfe.IsElementActive(true, 0))
			{
				std::string rwyName = vsid::utils::trim(sfe.GetRunwayName(0));
				this->activeAirports[aptName].depRwys.insert(rwyName);
			}
			if (sfe.IsElementActive(false, 1))
			{
				std::string rwyName = vsid::utils::trim(sfe.GetRunwayName(1));
				this->activeAirports[aptName].arrRwys.insert(rwyName);
			}
			if (sfe.IsElementActive(true, 1))
			{
				std::string rwyName = vsid::utils::trim(sfe.GetRunwayName(1));
				this->activeAirports[aptName].depRwys.insert(rwyName);
			}
		}
	}

	// only load configs if at least one airport has been selected
	if (this->activeAirports.size() > 0)
	{
		this->configParser.loadAirportConfig(this->activeAirports, this->savedRules, this->savedSettings, this->savedAreas, this->savedRequests, this->savedRwyRequests);
		messageHandler->writeMessage("DEBUG", "Checking .ese file for SID mastering...", vsid::MessageHandler::DebugArea::Conf);

		// if there are configured airports check for remaining sid data

		for(auto &sectionSid : this->sectionSids)
		{
			if (!this->activeAirports.contains(sectionSid.apt)) continue;
			
			for (vsid::Sid& sid : this->activeAirports[sectionSid.apt].sids)
			{
				if (sid.base != sectionSid.base) continue;
				if (sid.designator != (sectionSid.desig ? std::string(1, *sectionSid.desig) : "")) continue;
				if (!vsid::utils::contains(sid.rwys, sectionSid.rwy)) continue;

				if (std::isdigit(sectionSid.number))
				{
					if (sid.number == "")
					{
						sid.number = sectionSid.number;

						messageHandler->writeMessage("DEBUG", "[" + sid.base + sid.number + sid.designator + "] (ID: " + sid.id +
							") mastered. Master rwy: " + sectionSid.rwy, vsid::MessageHandler::DebugArea::Conf);
					}
					else if (sid.number != "" && sid.number != std::string(1, sectionSid.number) && sid.allowDiffNumbers)
					{
						std::string oldNumber = sid.number; // debugging value
						sid.number = sectionSid.number;

						messageHandler->writeMessage("DEBUG", "[" + sid.base + sid.number + sid.designator + "] (ID: " + sid.id +
							") overwritten old number (" + oldNumber + ") with " + sid.number + ". RWYs matched and diff numbers allowed." +
							" Master rwy: " + sectionSid.rwy,
							vsid::MessageHandler::DebugArea::Conf);
					}
					else if (sid.number != "") // health check for possible errors in .ese config
					{
						int currNumber = std::stoi(sid.number);
						int newNumber = sectionSid.number - '0';

						if (currNumber > newNumber || (currNumber == 1 && newNumber == 9))
						{
							messageHandler->writeMessage("WARNING", "[" + sectionSid.apt + "] Check your .ese - file for " + sid.base + " ? " + sid.designator + " SID!Already set number : " +
								std::to_string(currNumber) + " (ID: " + sid.id + "). Now found additional number: " + std::to_string(newNumber) +
								" - (Runway: " + sectionSid.rwy + "). Skipping additional number (is lower or before restarting count) due to possible sectore file error!");
						}
						else if (currNumber < newNumber || (newNumber == 1 && currNumber == 9))
						{
							messageHandler->writeMessage("WARNING", "[" + sectionSid.apt + "] Check your .ese-file for " + sid.base + "?" + sid.designator + " SID! Already set number: " +
								std::to_string(currNumber) + " (ID: " + sid.id + "). Now found additional number: " + std::to_string(newNumber) +
								" - (Runway: " + sectionSid.rwy + ") . Setting additional number (is higher or after restarting count) due to possible sectore file error!");

							sid.number = newNumber;
						}
						else if (currNumber != newNumber)
						{
							messageHandler->writeMessage("WARNING", "[" + sectionSid.apt + "] Check your .ese-file for " + sid.base + "?" + sid.designator + " SID! Already set number: " +
								std::to_string(currNumber) + " (ID: " + sid.id + "). Now found additional number: " + std::to_string(newNumber) +
								" - (Runway: " + sectionSid.rwy + ") . Setting additional number as it couldn't be determined which one is more likely to be correct!");

							sid.number = newNumber;
						}
					}

					if (!sid.transition.empty() && sectionSid.trans.base != "")
					{
						messageHandler->writeMessage("DEBUG", "Checking SID [" + sectionSid.base + sectionSid.number +
							((sectionSid.desig) ? std::string(1, *sectionSid.desig) : "") + "] with transition [" + sectionSid.trans.base +
							((sectionSid.trans.number) ? std::string(1, *sectionSid.trans.number) : "") +
							((sectionSid.trans.desig) ? std::string(1, *sectionSid.trans.desig) : "") + "].", vsid::MessageHandler::DebugArea::Conf);

						for (auto& [transBase, trans] : sid.transition)
						{
							if (transBase != sectionSid.trans.base) continue;
							if (trans.designator != (sectionSid.trans.desig ? std::string(1, *sectionSid.trans.desig) : "")) continue;
							if (trans.number != "") // #refactor .number to char
							{
								messageHandler->writeMessage("DEBUG", "[" + sid.base + ((sid.number != "") ? sid.number : "?") +
									sid.designator + "] (ID: " + sid.id + ") transition [" + trans.base +
									trans.number + trans.designator + "] already mastered. Skipping current transition number: " +
									((sectionSid.trans.number) ? std::string(1, *sectionSid.trans.number) : ""), vsid::MessageHandler::DebugArea::Conf);

								continue;
							}

							if (sectionSid.trans.number && std::isdigit(*sectionSid.trans.number))
							{
								trans.number = *sectionSid.trans.number;

								messageHandler->writeMessage("DEBUG", "[" + sid.base + ((sid.number != "") ? sid.number : "?") +
									sid.designator + "] (ID: " + sid.id + ") mastered transition [" + trans.base +
									trans.number + trans.designator + "]", vsid::MessageHandler::DebugArea::Conf);

								break;
							}
							else if (!sectionSid.trans.number && (trans.designator == "" || trans.designator != "XXX"))
							{
								trans.number = "-1"; // dummy value for wpt transitions

								messageHandler->writeMessage("DEBUG", "[" + sid.base + ((sid.number != "") ? sid.number : "?") +
									sid.designator + "] (ID: " + sid.id + ") mastered transition [" + trans.base +
									trans.number + trans.designator + "] (dummy number)", vsid::MessageHandler::DebugArea::Conf);

								break;
							}
						}
					}
				}

				/*if (sid.designator != "")
				{					
					
				}
				else
				{
					if (sectionSid.base != sid.base) continue;
					sid.number = 'X';
					messageHandler->writeMessage("DEBUG", "[" + sid.base + "] has no designator but the base could be mastered", vsid::MessageHandler::DebugArea::Conf);
				}*/
			}
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

	for (std::pair<const std::string, vsid::Airport> &apt : this->activeAirports)
	{
		for (vsid::Sid& sid : apt.second.sids)
		{
			if (sid.designator != "")
			{
				if (std::string("0123456789").find_first_of(sid.number) == std::string::npos)
					incompSids[apt.first].insert(sid.base + '?' + sid.designator);
				else
				{
					for (auto &[_, trans] : sid.transition)
					{
						if (trans.number == "-1") trans.number = "";
						else if (std::string("0123456789").find_first_of(trans.number) == std::string::npos)
							incompTrans[apt.first][sid.base + sid.number + sid.designator].insert(trans.base + '?' + trans.designator);
					}
				}
			}
			else if (sid.number != "")
			{
				for (auto& [_, trans] : sid.transition)
				{
					if (trans.number == "-1") trans.number = "";
					else if (std::string("0123456789").find_first_of(trans.number) == std::string::npos)
						incompTrans[apt.first][sid.base + sid.number + sid.designator].insert(trans.base + '?' + trans.designator);
				}
			}
			else
			{
				if (sid.number != "X") incompSids[apt.first].insert(sid.base);
			}
		}
	}

	// if incompatible SIDs (not in sector file) have been found remove them

	for (std::pair<const std::string, std::set<std::string>>& incompSidPair : incompSids)
	{
		messageHandler->writeMessage("WARNING", "Check config for [" + incompSidPair.first +
									"] - Could not master sids with .sct or .ese file: " + vsid::utils::join(incompSidPair.second, ','));

		for (const std::string& incompSid : incompSidPair.second)
		{
			if (!this->activeAirports.contains(incompSidPair.first)) continue; // #monitor - if incomp sids get deleted

			for (auto it = this->activeAirports[incompSidPair.first].sids.begin(); it != this->activeAirports[incompSidPair.first].sids.end();)
			{
				if (incompTrans.contains(incompSidPair.first) && incompTrans[incompSidPair.first].contains(it->base + it->number + it->designator) &&
					it->transition.size() == 0)
				{
					messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->waypoint + it->number + it->designator +
						"] (ID: " + it->id + ") lost all transitions and erased", vsid::MessageHandler::DebugArea::Conf);

					it = this->activeAirports[incompSidPair.first].sids.erase(it);
					continue;
				}

				if (it->designator != "")
				{
					if (it->waypoint == incompSid.substr(0, incompSid.length() - 2) && it->designator == std::string(1, incompSid[incompSid.length() - 1]))
					{
						if (std::string("0123456789").find_first_of(it->number) != std::string::npos)
						{
							messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->waypoint + it->number + it->designator +
								"] (ID: " + it->id + ") has a number. Skipping removal (other SID with same base failed to master)", vsid::MessageHandler::DebugArea::Conf);
							++it;
							continue;
						}

						if (!it->transition.empty())
						{
							messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->waypoint +
								((it->number != "") ? it->number : "?") + it->designator + "] (ID: " + it->id +
								") incompatible and erased. Transition present, double check them for validity.",
								vsid::MessageHandler::DebugArea::Conf);
						}
						else
						{
							messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->waypoint +
								((it->number != "") ? it->number : "?") + it->designator + "] (ID: " + it->id +
								") incompatible and erased. No transition present.", vsid::MessageHandler::DebugArea::Conf);
						}
						
						it = this->activeAirports[incompSidPair.first].sids.erase(it);
						continue;
					}
				}
				else if(it->number == "" && it->base == incompSid)
				{
					messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->base +
						"] (ID: " + it->id + ") (base only) incompatible and erased", vsid::MessageHandler::DebugArea::Conf);

					it = this->activeAirports[incompSidPair.first].sids.erase(it);
					continue;
				}
				++it;
			}
		}
	}

	for (auto& [icao, sidMap] : incompTrans)
	{
		messageHandler->writeMessage("WARNING", "Check config for [" + icao +
			"] - Could not master following SIDs with transitions: ");

		for (auto& [sidName, transitions] : sidMap)
		{
			messageHandler->writeMessage("WARNING", "SID [" + sidName +
				"]: " + vsid::utils::join(transitions, ','));
		}
	}

	// remove dummy value for SIDs without designator // #evaluate

	for (std::pair<const std::string, vsid::Airport>& apt : this->activeAirports)
	{
		for (vsid::Sid& sid : apt.second.sids)
		{
			if (sid.number != "X") continue;
			sid.number = "";
		}
	}

	messageHandler->writeMessage("INFO", "Airports updated. [" + std::to_string(this->activeAirports.size()) + "] active.");
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
		this->processed.clear();
		this->removeProcessed.clear();
		this->savedFplnInfo.clear();

		for (auto& [_, aptInfo] : this->activeAirports)
		{
			for (auto& [_, reqList] : aptInfo.requests)
			{
				reqList.clear();
			}
			for (auto& [_, reqList] : aptInfo.rwyrequests)
			{
				reqList.clear();
			}
		}

		this->syncQueue.clear();
		this->spReleased.clear();
	}

	if (Counter % 10 == 0)
	{
		for (std::map<std::string, vsid::Fpln>::iterator it = this->processed.begin(); it != this->processed.end();)
		{
			EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(it->first.c_str());
			std::string adep = fpln.GetFlightPlanData().GetOrigin();

			// mark invalid flight plans for removal

			if (!fpln.IsValid())
			{
				if (!this->removeProcessed.contains(it->first))
				{
					messageHandler->writeMessage("DEBUG", "[" + it->first + "] is invalid. Removal in 1 min.", vsid::MessageHandler::DebugArea::Dev);
					auto now = std::chrono::utc_clock::now() + std::chrono::minutes{ 1 };
					this->removeProcessed[it->first] = { now, true }; // assume fpln is disconnected for some reason, might come back
				}			
				++it;
				continue;
			}

			// remove processed flight plans if outside of base vis range

			if(this->outOfVis(fpln))
			{
				messageHandler->writeMessage("DEBUG", "[" + it->first + "] is further away than my range of NM " +
					std::to_string(ControllerMyself().GetRange()), vsid::MessageHandler::DebugArea::Dev);

				this->removeProcessed.erase(it->first);
				this->savedFplnInfo.erase(it->first);
				this->removeFromRequests(it->first, adep);

				if (vsid::fplnhelper::findRemarks(fpln, "VSID/RWY"))
				{
					vsid::fplnhelper::removeRemark(fpln, "VSID/RWY");

					std::string callsign = fpln.GetCallsign();

					if (!fpln.GetFlightPlanData().AmendFlightPlan())
					{
						if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_AMEND))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend flight plan! - #FFDU");
							messageHandler->addFplnError(callsign, ERROR_FPLN_AMEND);
						}

						this->processed[callsign].noFplnUpdate = false;
					}
					else messageHandler->removeFplnError(callsign, ERROR_FPLN_AMEND);
				}

				it = this->processed.erase(it);

				messageHandler->removeCallsignFromErrors(fpln.GetCallsign());
			}
			else ++it;
		}
	}

	// check internally removed flight plans every 20 seconds if they re-connected

	if (this->removeProcessed.size() > 0 && Counter % 20 == 0)
	{
		auto now = std::chrono::utc_clock::now();

		for (auto it = this->removeProcessed.begin(); it != this->removeProcessed.end();)
		{
			EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(it->first.c_str());

			if (it->second.second && fpln.IsValid() && !this->outOfVis(fpln))
			{
				messageHandler->writeMessage("DEBUG", "[" + it->first + "] reconnected.", vsid::MessageHandler::DebugArea::Fpln);
				it = this->removeProcessed.erase(it);
				continue;
			}

			if (now > it->second.first)
			{
				std::string icao = fpln.GetFlightPlanData().GetOrigin();
				std::string callsign = fpln.GetCallsign();

				this->removeFromRequests(callsign, icao);

				messageHandler->writeMessage("DEBUG", "[" + it->first + "] exceeded disconnection time. Dropping", vsid::MessageHandler::DebugArea::Fpln);

				std::erase_if(this->processed, [&](const auto& fpln) { return it->first == fpln.first; });
				if (this->savedFplnInfo.contains(it->first)) this->savedFplnInfo.erase(it->first); // #refactor - erase_if

				it = this->removeProcessed.erase(it);
			}
			else ++it;
		}
	}
}

void vsid::VSIDPlugin::deleteScreen(int id)
{
	messageHandler->writeMessage("DEBUG", "(deleteScreen) size: " + std::to_string(this->radarScreens.size()), vsid::MessageHandler::DebugArea::Menu);
	for (auto [id, screen] : this->radarScreens)
	{
		messageHandler->writeMessage("DEBUG", "(deleteScreen) present id: " + std::to_string(id) + " is valid: " +
			((this->radarScreens.at(id) ? "true" : "false")), vsid::MessageHandler::DebugArea::Menu);
	}

	if (this->radarScreens.contains(id))
	{
		messageHandler->writeMessage("DEBUG", "(deleteScreen) Removing id: " + std::to_string(id) + " use count: " +
			std::to_string(this->radarScreens.at(id).use_count()), vsid::MessageHandler::DebugArea::Menu);
		this->radarScreens.erase(id);
	}
	else
	{
		messageHandler->writeMessage("DEBUG", "(deleteScreen) id: " + std::to_string(id) + " is unknown.", vsid::MessageHandler::DebugArea::Menu);
	}

	messageHandler->writeMessage("DEBUG", "(deleteScreen) size after deletion: " + std::to_string(this->radarScreens.size()), vsid::MessageHandler::DebugArea::Menu);
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
				messageHandler->writeMessage("ERROR", "Couldn't call ext func for [" + std::string(sCallsign) + "] as the screen (id: " +
					std::to_string(id) + ") couldn't be called. Code: " + ERROR_FPLN_EXTFUNC);
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
	this->shared.reset();
}

void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	// create the instance

	*ppPlugInInstance = vsidPlugin = new vsid::VSIDPlugin();
}


//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{
	/* no deletion of vsidPlugin needed - ownership taken over by this->shared */

	vsidPlugin->exit();
}
