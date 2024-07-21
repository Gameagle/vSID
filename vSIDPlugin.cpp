#include "pch.h"

#include "vSIDPlugin.h"
#include "flightplan.h"
#include "timeHandler.h"
#include "messageHandler.h"
#include "area.h"
#include "Psapi.h"

#include <set>
#include <algorithm>

// DEV
//#include "display.h"
#include "airport.h"
#include <thread>
// END DEV

vsid::VSIDPlugin* vsidPlugin;

vsid::VSIDPlugin::VSIDPlugin() : EuroScopePlugIn::CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, pluginName.c_str(), pluginVersion.c_str(), pluginAuthor.c_str(), pluginCopyright.c_str()) {

	//this->detectPlugins();
	this->configParser.loadMainConfig();
	this->configParser.loadGrpConfig();
	this->gsList = "STUP,PUSH,TAXI,DEPA";

	messageHandler->setLevel("INFO");
	
	RegisterTagItemType("vSID SID", TAG_ITEM_VSID_SIDS);
	RegisterTagItemFunction("SIDs Auto Select", TAG_FUNC_VSID_SIDS_AUTO);
	RegisterTagItemFunction("SIDs Menu", TAG_FUNC_VSID_SIDS_MAN);

	RegisterTagItemType("vSID CLMB", TAG_ITEM_VSID_CLIMB);
	RegisterTagItemFunction("Climb Menu", TAG_FUNC_VSID_CLMBMENU);

	RegisterTagItemType("vSID RWY", TAG_ITEM_VSID_RWY);
	RegisterTagItemFunction("RWY Menu", TAG_FUNC_VSID_RWYMENU);

	RegisterTagItemType("vSID Squawk", TAG_ITEM_VSID_SQW);

	RegisterTagItemType("vSID Request", TAG_ITEM_VSID_REQ);
	RegisterTagItemFunction("REQ Menu", TAG_FUNC_VSID_REQMENU);

	RegisterTagItemType("vSID Request Timer", TAG_ITEM_VSID_REQTIMER);

	//RegisterDisplayType("vSID (no display)", false, false, false, true); /// DEV

	UpdateActiveAirports(); // preload rwy settings

	DisplayUserMessage("Message", "vSID", std::string("Version " + pluginVersion + " loaded").c_str(), true, true, false, false, false);
}

vsid::VSIDPlugin::~VSIDPlugin() {};

/*
* BEGIN OWN FUNCTIONS
*/

//void vsid::VSIDPlugin::detectPlugins()
//{
//	HMODULE hMods[1024];
//	HANDLE hProcess;
//	DWORD cbNeeded;
//	unsigned int i;
//
//	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
//	if (hProcess == NULL) return;
//
//	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
//	{
//		for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
//		{
//			TCHAR szModName[MAX_PATH];
//
//			if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
//			{
//				std::string modName = szModName;
//				if (modName.find("SYSTEM32") != std::string::npos ||
//					modName.find("System32") != std::string::npos ||
//					modName.find("system32") != std::string::npos) continue;
//				size_t pos = modName.find_last_of("\\");
//				if (pos == std::string::npos) continue;
//				modName = modName.substr(pos + 1);
//
//				if (modName == "CCAMS.dll") this->ccamsLoaded = true;
//				if (modName == "TopSky.dll") this->topskyLoaded = true;
//			}
//		}
//	}
//	CloseHandle(hProcess);
//}

std::string vsid::VSIDPlugin::findSidWpt(EuroScopePlugIn::CFlightPlanData FlightPlanData)
{
	std::string filedSid = FlightPlanData.GetSidName();
	std::vector<std::string> filedRoute = vsid::utils::split(FlightPlanData.GetRoute(), ' ');

	if (filedRoute.size() == 0) return "";
	if (filedSid != "")
	{
		std::string esWpt = filedSid.substr(0, filedSid.length() - 2);
		for (const vsid::Sid& sid : this->activeAirports[FlightPlanData.GetOrigin()].sids)
		{
			if (esWpt == sid.waypoint && std::any_of(filedRoute.begin(), filedRoute.end(), [&](std::string wpt)
				{
					try
					{
						if (esWpt == vsid::utils::split(wpt, '/').at(0)) return true;
						else return false;
					}
					catch (std::out_of_range)
					{
						messageHandler->writeMessage("ERROR", "Failed to split waypoint during SID waypoint checking #fswpt");
						return false;
					}
				})) return esWpt;
		}
	}

	// continue checks if esWpt hasn't been returned
	std::set<std::string> sidWpts = {};

	for (const vsid::Sid &sid : this->activeAirports[FlightPlanData.GetOrigin()].sids)
	{
		if(sid.waypoint != "XXX") sidWpts.insert(sid.waypoint);
	}

	for (std::string &wpt : filedRoute)
	{
		if (wpt.find("/") != std::string::npos)
		{
			try
			{
				wpt = vsid::utils::split(wpt, '/').at(0);
			}
			catch (std::out_of_range)
			{
				messageHandler->writeMessage("ERROR", "Failed to get the waypoint of a waypoint" 
											" and speed/level group. Waypoint is: " + wpt);
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

vsid::Sid vsid::VSIDPlugin::processSid(EuroScopePlugIn::CFlightPlan FlightPlan, std::string atcRwy)
{
	EuroScopePlugIn::CFlightPlan fpln = FlightPlan;
	EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
	std::string callsign = fpln.GetCallsign();
	std::string icao = fplnData.GetOrigin();
	std::string dest = fplnData.GetDestination();
	// EuroScopePlugIn::CFlightPlanControllerAssignedData cad = fpln.GetControllerAssignedData();
	std::vector<std::string> filedRoute = vsid::utils::split(std::string(fplnData.GetRoute()), ' ');
	std::string sidWpt = vsid::VSIDPlugin::findSidWpt(fplnData);
	vsid::Sid setSid = {};
	int prio = 99;
	bool customRuleActive = false;
	std::set<std::string> wptRules = {};
	std::set<std::string> actTSid = {};

	if (!this->activeAirports.contains(icao))
	{
		messageHandler->writeMessage("DEBUG", "[" + icao + "] is not an active airport. Skipping all SID checks", vsid::MessageHandler::DebugArea::Sid);
		return vsid::Sid();
	}

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

	if (customRuleActive)
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] [" + icao +
									"] Custom rules active. Checking for avbl areas and possible arr as dep rwys.",
									vsid::MessageHandler::DebugArea::Sid);
		std::map<std::string, bool> customRules = this->activeAirports[icao].customRules;
		for (auto it = this->activeAirports[icao].sids.begin(); it != this->activeAirports[icao].sids.end();)
		{
			//if (it->customRule == "" || wptRules.contains(it->waypoint)) { ++it; continue; }
			if (it->customRule == "" ||
				(this->activeAirports[icao].customRules.contains(it->customRule) &&
				!this->activeAirports[icao].customRules[it->customRule])) { ++it; continue; }

			std::set<std::string> depRwys = this->activeAirports[icao].depRwys;
			//std::vector<std::string> sidRules = vsid::utils::split(it->customRule, ',');
			//std::vector<std::string> sidRwys = vsid::utils::split(it->rwy, ',');

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

			/*if (std::any_of(customRules.begin(), customRules.end(), [&](auto rule)
				{
					return std::find(sidRules.begin(), sidRules.end(), rule.first) != sidRules.end() && rule.second;
				}
			) && std::any_of(sidRwys.begin(), sidRwys.end(), [&](auto rwy)
				{
					return depRwys.contains(rwy);
				}
			))
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] Inserted waypoint " + it->waypoint + " into rule set because rule is found and rwy active",
											vsid::MessageHandler::DebugArea::Sid);
				wptRules.insert(it->waypoint);
			}*/
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
			)
		)
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

	//std::set<std::string> depRwys = this->activeAirports[icao].depRwys;
	/*if (atcRwy != "" && this->processed.contains(callsign) && this->processed[callsign].atcRWY) depRwys.insert(atcRwy);
	else if (atcRwy != "" && !this->processed.contains(callsign) &&
			this->activeAirports[icao].settings["auto"] &&
			(vsid::fpln::findRemarks(fpln, "VSID/RWY") || fplnData.IsAmended())) depRwys.insert(atcRwy);*/

	
	std::string actAtcRwy = "";
	
	if (atcRwy != "" && this->processed.contains(callsign) && this->processed[callsign].atcRWY) actAtcRwy = atcRwy;
	else if (atcRwy != "" && !this->processed.contains(callsign) &&
		this->activeAirports[icao].settings["auto"] &&
		(vsid::fpln::findRemarks(fpln, "VSID/RWY") || fplnData.IsAmended())) actAtcRwy = atcRwy;

	for(vsid::Sid &currSid : this->activeAirports[icao].sids)
	{
		// copy depRwys to add arr rwys if enabled in an active area
		std::set<std::string> depRwys = this->activeAirports[icao].depRwys;

		if (actAtcRwy != "") depRwys.insert(actAtcRwy);

		// skip if current SID does not match found SID wpt
		if (currSid.waypoint != sidWpt && currSid.waypoint != "XXX") continue;

		bool rwyMatch = false;
		bool restriction = false;
		

		// checking areas for arrAsDep - actual area evaluation down below
		if (currSid.area != "")
		{
			std::vector<std::string> sidAreas = vsid::utils::split(currSid.area, ',');

			if (std::any_of(sidAreas.begin(), sidAreas.end(), [&](auto sidArea)
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
			if (currSid.actArrRwy["all"] != "")
			{
				std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["all"], ',');
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
			else if (currSid.actArrRwy["any"] != "")
			{
				std::vector<std::string> actArrRwy = vsid::utils::split(currSid.actArrRwy["any"], ',');
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

		// skip if SID needs active dep rwys
		if (!currSid.actDepRwy.empty())
		{
			if (currSid.actDepRwy["all"] != "")
			{
				std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["all"], ',');
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
			else if (currSid.actDepRwy["any"] != "")
			{
				std::vector<std::string> actDepRwy = vsid::utils::split(currSid.actDepRwy["any"], ',');
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
			if (currSid.rwy.find(depRwy) == std::string::npos)
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
		
		//if (customRuleActive &&
		//	((wptRules.contains(currSid.waypoint) && currSid.customRule == "") ||
		//	(!wptRules.contains(currSid.waypoint) && currSid.customRule != "")))
		//{
		//	// in addition - if a rwy was set by ATC don't skip
		//	bool skip = true;
		//	if (atcRwy != "" && this->processed.contains(callsign) && this->processed[callsign].atcRWY) skip = false;

		//	if (skip)
		//	{
		//		messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
		//			"\" because customRule active for waypoint and SID doesn't have a rule set OR " +
		//			" customRule NOT active for waypoint, but SID has a rule set",
		//			vsid::MessageHandler::DebugArea::Sid);
		//		continue;
		//	}
		//}

		if (customRuleActive && currSid.customRule != "" &&
			this->activeAirports[icao].customRules.contains(currSid.customRule) &&
			!this->activeAirports[icao].customRules[currSid.customRule])
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" +
				currSid.idName() + "\" because the SID custom rule is not active.",
				vsid::MessageHandler::DebugArea::Sid);
			continue;
		}

		// skip if equipment does not match

		if (this->activeAirports[icao].equipCheck)
		{
			std::string equip = vsid::fpln::getEquip(FlightPlan);
			std::string pbn = vsid::fpln::getPbn(FlightPlan);

			if (currSid.equip.contains("RNAV"))
			{
				if (equip.size() > 1 && equip.find_first_of("ABGR") == std::string::npos && pbn == "")
				{
					messageHandler->writeMessage("DEBU", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
												"\" because RNAV ('A', 'B', 'G' or 'R' is required, but not found in equipment \"" +
												equip + "\" and PBN is empty", vsid::MessageHandler::DebugArea::Sid);
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
					continue;
				}
				if ((equip.find_first_of("GR") != std::string::npos || equip == "") && pbn != "")
				{
					messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
						"\" because only NON-RNAV is allowed, but RNAV ('G' or 'R') was found in equipment \"" + equip +
						"\" or equipment was empty and PBN is not empty", vsid::MessageHandler::DebugArea::Sid);
					continue;
				}
			}
			else
			{
				for (const auto &sidEquip : currSid.equip)
				{
					if(sidEquip.second && equip.find(sidEquip.first) == std::string::npos) continue;
					if (!sidEquip.second && equip.find(sidEquip.first) != std::string::npos) continue;
				}
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
										"\" due to a RWY missmatch between SID rwy and active DEP rwy",
										vsid::MessageHandler::DebugArea::Sid
			);
			continue;
		}
		
		
		// skip if engine type doesn't match
		if (currSid.engineType != "" && currSid.engineType.find(fplnData.GetEngineType()) == std::string::npos)
		{
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
										"\" because of a missmatch in engineType",
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
										"\" because of a missmatch in aircraft type (type is set to false" +
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
										"\" because of a missmatch in engine number",
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
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() + "\" because of missmatch in WTC",
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

		// skip if destination restrictions present and flightplan doesn't match

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
			return vsid::Sid();
		}
		
		// if a SID is accepted when filed by a pilot set the SID
		if (currSid.pilotfiled && currSid.name() == fplnData.GetSidName() && currSid.prio < prio)
		{
			setSid = currSid;
			prio = currSid.prio;
		}
		else if(currSid.pilotfiled) messageHandler->writeMessage("DEBUG", "[" + callsign + "] Ignoring SID \"" + currSid.idName() +
																"\" because it is only accepted as pilot filed and the prio is higher or the filed SID was another",
																vsid::MessageHandler::DebugArea::Sid
									);
		if (currSid.prio < prio && (setSid.pilotfiled == currSid.pilotfiled || restriction))
		{
			setSid = currSid;
			prio = currSid.prio;
		}
		else messageHandler->writeMessage("DEBUG", "[" + callsign + "] Skipping SID \"" + currSid.idName() +
										"\" because prio is higher",
										vsid::MessageHandler::DebugArea::Sid);
	}
	messageHandler->writeMessage("DEBUG", "[" + callsign + "] Setting SID \"" + setSid.idName() + "\"", vsid::MessageHandler::DebugArea::Sid);
	return(setSid);
}

void vsid::VSIDPlugin::processFlightplan(EuroScopePlugIn::CFlightPlan FlightPlan, bool checkOnly, std::string atcRwy, vsid::Sid manualSid)
{
	if (!FlightPlan.IsValid()) return;

	EuroScopePlugIn::CFlightPlan fpln = FlightPlan;
	EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = fpln.GetControllerAssignedData();
	std::string callsign = fpln.GetCallsign();
	std::string icao = fplnData.GetOrigin();
	std::string filedSidWpt = this->findSidWpt(fplnData);
	std::vector<std::string> filedRoute = vsid::utils::split(std::string(fplnData.GetRoute()), ' ');
	vsid::Sid sidSuggestion = {};
	vsid::Sid sidCustomSuggestion = {};
	std::string setRwy = "";
	vsid::fpln::Info fplnInfo = {};

	if (!this->activeAirports.contains(icao))
	{
		messageHandler->writeMessage("ERROR", "[" + callsign + "] ADEP [" + icao + "] is not an active airport. Aborting processing.");
		return;
	}

	// restore requests after an airpot update was called but only for first processing
	if (!this->processed.contains(callsign) &&
		std::any_of(this->activeAirports[icao].requests.begin(), this->activeAirports[icao].requests.end(), [&](auto request)
	{
		for (const std::pair<std::string, long long>& fpln : request.second)
		{
			if (fpln.first == callsign) return true;
			else continue;
		}
		return false; // fallback state
	}))
	{
		fplnInfo.request = true;
	}

	vsid::fpln::clean(filedRoute, icao, filedSidWpt);

	if (this->processed.contains(callsign))
	{
		fplnInfo.atcRWY = this->processed[callsign].atcRWY;
		fplnInfo.request = this->processed[callsign].request;
		fplnInfo.noFplnUpdate = this->processed[callsign].noFplnUpdate;
	}

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
		sidSuggestion = this->processSid(fpln);
		sidCustomSuggestion = manualSid;

		if(atcRwy != "" && sidSuggestion.rwy.find(atcRwy) != std::string::npos && sidSuggestion == sidCustomSuggestion)
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
		sidSuggestion = this->processSid(fpln);
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] processing SID with atcRWY (for customSuggestion): " + atcRwy, vsid::MessageHandler::DebugArea::Sid);
		sidCustomSuggestion = this->processSid(fpln, atcRwy);
	}
	/* default state */
	else
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] processing SID without atcRWY", vsid::MessageHandler::DebugArea::Sid);
		sidSuggestion = this->processSid(fpln);
	}

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		try
		{
			std::string rwy;
			if (atcRwy != "" && sidSuggestion.rwy.find(atcRwy) != std::string::npos) rwy = atcRwy;
			else rwy = sidSuggestion.getRwy();

			if (this->activeAirports[icao].depRwys.contains(rwy) ||
				this->activeAirports[icao].arrRwys.contains(rwy)) setRwy = rwy;
			else setRwy = fplnData.GetDepartureRwy();
		}
		catch (std::out_of_range)
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to check RWY in sidSuggestion. Check config " +
										icao + " for SID \"" + sidSuggestion.idName() + "\". RWY value is: " + sidSuggestion.rwy
			);
		}
	}
	else if (sidCustomSuggestion.base != "")
	{
		try
		{
			std::string rwy;

			if (atcRwy != "" && sidCustomSuggestion.rwy.find(atcRwy) != std::string::npos) rwy = atcRwy;
			else rwy = sidCustomSuggestion.getRwy();

			if (this->activeAirports[fplnData.GetOrigin()].depRwys.contains(rwy) ||
				this->activeAirports[fplnData.GetOrigin()].arrRwys.contains(rwy)) setRwy = rwy;
			else setRwy = fplnData.GetDepartureRwy();
		}
		catch (std::out_of_range)
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to check RWY in sidCustomSuggestion. Check config " +
										icao + " for SID \"" +	sidCustomSuggestion.idName() + "\". RWY value is: " + sidCustomSuggestion.rwy);
		}
	}
	
	// building a new route with the selected sid
	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		std::ostringstream ss;
		ss << sidSuggestion.name() << "/" << setRwy;
		filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
	}
	else if (sidCustomSuggestion.base != "")
	{
		std::ostringstream ss;
		ss << sidCustomSuggestion.name() << "/" << setRwy;
		filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
	}

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		fplnInfo.sid = sidSuggestion;
	}
	else if (sidCustomSuggestion.base != "")
	{
		fplnInfo.sid = sidSuggestion;
		fplnInfo.customSid = sidCustomSuggestion;
	}
	this->processed[callsign] = fplnInfo;

	// if an IFR fpln has no matching sid but the route should be set inverse - otherwise rwy changes would be overwritten
	if (!checkOnly && std::string(fplnData.GetPlanType()) == "I" &&
		sidSuggestion.empty() && sidCustomSuggestion.empty()) checkOnly = true;

	if(!checkOnly)
	{	
		this->processed[callsign].noFplnUpdate = true;
		if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to change Flighplan! - #PFP");
			this->processed[callsign].noFplnUpdate = false;
		}
		//else this->processed[callsign].noFplnUpdate = true;

		if (!fplnData.AmendFlightPlan())
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend Flighplan! - #PFP");
			this->processed[callsign].noFplnUpdate = false;
		}
		else
		{
			if (this->activeAirports[fplnData.GetOrigin()].settings["auto"])
			{
				// EuroScopePlugIn::CFlightPlanControllerAssignedData cad = fpln.GetControllerAssignedData();
				std::string scratch = ".vsid_auto_" + std::string(ControllerMyself().GetCallsign());
				vsid::fpln::setScratchPad(FlightPlan, scratch);
			}
			this->processed[callsign].atcRWY = true;
		}
		if (sidSuggestion.base != "" && sidCustomSuggestion.base == "" && sidSuggestion.initialClimb)
		{
			if (!cad.SetClearedAltitude(sidSuggestion.initialClimb))
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude - #PFP");
			}
		}
		else if (sidCustomSuggestion.base != "" && sidCustomSuggestion.initialClimb)
		{
			if (!cad.SetClearedAltitude(sidCustomSuggestion.initialClimb))
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude - #PFP");
			}
		}
		//if (this->ccamsLoaded && this->radarScreen != nullptr) /// DEV
		//{
		//	messageHandler->writeMessage("DEBUG", "Triggering squawk assignement");
		//	std::string squawk = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
		//	if (squawk == "" || squawk == "1234" || squawk == "0000")
		//	{

		//		messageHandler->writeMessage("DEBUG", "trying to write squawk");

		//		this->radarScreen->StartTagFunction(callsign.c_str(), nullptr, 0, "", "CCAMS", 871, POINT(), RECT());
		//	}
		//}
		//else if (this->radarScreen == nullptr) messageHandler->writeMessage("DEBUG", "nullptr detected");// END DEV
	}
}
/*
* END OWN FUNCTIONS
*/

/*
* BEGIN ES FUNCTIONS
*/

void vsid::VSIDPlugin::OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area) {
	
	// if logged in as observer disable functions
	if (!ControllerMyself().IsController()) return;

	EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelectASEL();

	if (!fpln.IsValid())
	{
		messageHandler->writeMessage("ERROR", "Couldn't process flightplan as it was reported invalid (technical invalid).");
		return;
	}

	EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
	std::string callsign = fpln.GetCallsign();


	/* DOCUMENTATION 
		//for (int i = 0; i < 8; i++) // test on how to get annotations
		//{
		//	std::string annotation = flightPlanASEL.GetControllerAssignedData().GetFlightStripAnnotation(i);
		//	vsid::messagehandler::LogMessage("Debug", "Annotation: " + annotation);
		//}

		delete FlightPlan;
	}*/

	// dynamically fill sid selection list with valid sids
	if (FunctionId == TAG_FUNC_VSID_SIDS_MAN)
	{
		std::string filedSidWpt = this->findSidWpt(fplnData);
		std::map<std::string, vsid::Sid> validDepartures;
		std::string depRWY = fplnData.GetDepartureRwy();

		for (vsid::Sid &sid : this->activeAirports[fplnData.GetOrigin()].sids)
		{
			std::vector<std::string> sidRwys = vsid::utils::split(sid.rwy, ',');

			if ((sid.waypoint == filedSidWpt || sid.waypoint == "XXX") && depRWY != "" && sid.rwy.find(depRWY) != std::string::npos)
			{
				validDepartures[sid.base + sid.number + sid.designator] = sid;
				validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", depRWY);
			}
			else if (filedSidWpt == "" && depRWY == "" &&
					std::any_of(sidRwys.begin(), sidRwys.end(), [&](std::string sidRwy)
					{
						return this->activeAirports[fplnData.GetOrigin()].depRwys.contains(sidRwy);
					}
					))
			{
				validDepartures[sid.base + sid.number + sid.designator] = sid;
				validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", depRWY);
			}
			else if (filedSidWpt == "" && depRWY != "" && sid.rwy.find(depRWY) != std::string::npos)
			{
				validDepartures[sid.base + sid.number + sid.designator] = sid;
				validDepartures[sid.base + 'R' + 'V'] = vsid::Sid(sid.base, sid.waypoint, "", "R", "V", depRWY);
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
			std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
			vsid::fpln::clean(filedRoute, fplnData.GetOrigin(), filedSidWpt);

			if (depRWY != "")
			{
				std::ostringstream ss;
				ss << fplnData.GetOrigin() << "/" << depRWY;
				filedRoute.insert(filedRoute.begin(), ss.str());
			}
			this->processed[callsign].noFplnUpdate = true;
			if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
			{
				messageHandler->writeMessage("ERROR", "[" + std::string(fpln.GetCallsign()) + "] - Failed to change Flighplan! #MSS");
				this->processed[callsign].noFplnUpdate = false;
			}
			if (!fplnData.AmendFlightPlan())
			{
				messageHandler->writeMessage("ERROR", "[" + std::string(fpln.GetCallsign()) + "] - Failed to amend Flighplan! #MSS");
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
				depRWY = validDepartures[sItemString].getRwy();
			}
			messageHandler->writeMessage("DEBUG", "[" + callsign + "] Calling manual SID with rwy : " + depRWY, vsid::MessageHandler::DebugArea::Sid);
			this->processFlightplan(fpln, false, depRWY, validDepartures[sItemString]);
		}
		// FlightPlan->flightPlan.GetControllerAssignedData().SetFlightStripAnnotation(0, sItemString) // test on how to set annotations (-> exclusive per plugin)
	}

	if (FunctionId == TAG_FUNC_VSID_SIDS_AUTO)
	{
		if (this->processed.contains(callsign))
		{
			std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
			std::string atcRwy = vsid::fpln::getAtcBlock(filedRoute, fplnData.GetOrigin()).second;
			if (std::string(fplnData.GetPlanType()) == "I")
			{
				if (this->processed[callsign].atcRWY && atcRwy != "")
				{
					this->processFlightplan(fpln, false, atcRwy);
				}
				else this->processFlightplan(fpln, false);
			}
		}
	}

	if (FunctionId == TAG_FUNC_VSID_CLMBMENU)
	{
		std::map<std::string, int> alt;

		// code order important! the popuplist may only be generated when no sItemString is present (if a button has been clicked)
		// if the list gets set up again while clicking a button wrong values might occur
		for (int i = this->activeAirports[fplnData.GetOrigin()].maxInitialClimb; i >= vsid::utils::getMinClimb(this->activeAirports[fplnData.GetOrigin()].elevation); i -= 500)
		{
			std::string menuElem = (i > this->activeAirports[fplnData.GetOrigin()].transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
			alt[menuElem] = i;
		}

		if (strlen(sItemString) == 0)
		{
			this->OpenPopupList(Area, "Select Climb", 1);
			for (int i = this->activeAirports[fplnData.GetOrigin()].maxInitialClimb; i >= vsid::utils::getMinClimb(this->activeAirports[fplnData.GetOrigin()].elevation); i -= 500)
			{
				std::string clmbElem = (i > this->activeAirports[fplnData.GetOrigin()].transAlt) ? "0" + std::to_string(i / 100) : "A" + std::to_string(i / 100);
				this->AddPopupListElement(clmbElem.c_str(), clmbElem.c_str(), TAG_FUNC_VSID_CLMBMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
			}
		}
		if (strlen(sItemString) != 0)
		{
			if (!fpln.GetControllerAssignedData().SetClearedAltitude(alt[sItemString]))
			{
				messageHandler->writeMessage("ERROR", "Failed to set cleared altitude - #CLM");
			}
		}
	}

	if (FunctionId == TAG_FUNC_VSID_RWYMENU)
	{
		std::vector<std::string> allRwys = this->activeAirports[fplnData.GetOrigin()].allRwys;
		std::string rwy;

		if (strlen(sItemString) == 0)
		{
			this->OpenPopupList(Area, "RWY", 1);
			for (std::vector<std::string>::iterator it = allRwys.begin(); it != allRwys.end();)
			{
				rwy = *it;
				if (this->activeAirports[fplnData.GetOrigin()].depRwys.count(rwy) ||
					this->activeAirports[fplnData.GetOrigin()].arrRwys.count(rwy)
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
			std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
			vsid::fpln::clean(filedRoute, fplnData.GetOrigin());

			std::ostringstream ss;
			ss << fplnData.GetOrigin() << "/" << sItemString;
			filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
			
			if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to change Flighplan! - #RM");
			}
			//if (!vsid::fpln::findRemarks(fplnData, "VSID/RWY"))
			if (!vsid::fpln::findRemarks(fpln, "VSID/RWY"))
			{
				/*if (!vsid::fpln::addRemark(fplnData, "VSID/RWY"))*/
				if (!vsid::fpln::addRemark(fpln, "VSID/RWY"))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to set remarks! - #RM");
				}
			}

			if (!fplnData.AmendFlightPlan())
			{
				messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend Flighplan! - #RM");
			}
			else if (this->activeAirports[fplnData.GetOrigin()].settings["auto"] && this->processed.contains(callsign))
			{
				if (std::string(fpln.GetFlightPlanData().GetPlanType()) == "I" &&
					(!this->processed[callsign].sid.empty() || !this->processed[callsign].customSid.empty())
					)
				{
					this->processed[callsign].noFplnUpdate = true;
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

			if (this->activeAirports.contains(fplnData.GetOrigin()))
			{
				this->AddPopupListElement("Clearance", "Clearance", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Startup", "Startup", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Pushback", "Pushback", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Taxi", "Taxi", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
				this->AddPopupListElement("Departure", "Departure", TAG_FUNC_VSID_REQMENU, false, EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX, false, false);
			}
		}
		else if (strlen(sItemString) != 0)
		{
			// EuroScopePlugIn::CFlightPlanControllerAssignedData cad = fpln.GetControllerAssignedData();
			
			std::string scratch = ".vsid_req_" + std::string(sItemString) + "/" +
								std::to_string(std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()).time_since_epoch().count());

			//vsid::fpln::setScratchPad(cad, scratch);
			vsid::fpln::setScratchPad(fpln, scratch);
		}
	}
}

void vsid::VSIDPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	if (!FlightPlan.IsValid())
	{
		std::string callsign = FlightPlan.GetCallsign();
		if (this->processed.contains(callsign)) this->processed.erase(callsign);
		return;
	}

	std::string callsign = FlightPlan.GetCallsign();
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();

	if (ItemCode == TAG_ITEM_VSID_SIDS)
	{
		*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
		std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), fplnData.GetOrigin());

		if (this->removeProcessed.size() > 0 &&
			this->removeProcessed.contains(callsign) &&
			this->removeProcessed[callsign].second)
		{
			this->removeProcessed.erase(callsign);

			if (this->processed.contains(callsign))
			{
				if (atcBlock.first == "") this->processed.erase(callsign);

				EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
				if (cad.GetClearedAltitude() == cad.GetFinalAltitude() &&
					atcBlock.first != "" &&
					ControllerMyself().IsController() &&
					(atcBlock.first == this->processed[callsign].sid.name() ||
					atcBlock.first == this->processed[callsign].customSid.name())
					)
				{
					if (!this->processed[callsign].customSid.empty())
					{
						if (!cad.SetClearedAltitude(this->processed[callsign].customSid.initialClimb))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude #RICS");
						}
					}
					else if (!this->processed[callsign].sid.empty())
					{
						if (!cad.SetClearedAltitude(this->processed[callsign].sid.initialClimb))
						{
							messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude #RICS");
						}
					}
				}
			}
		}

		if (this->processed.contains(callsign))
		{
			// DEV
			//messageHandler->writeMessage("DEBUG", "[" + callsign + "] equipment \"" + vsid::fpln::getEquip(FlightPlan) + "\"", vsid::MessageHandler::DebugArea::Dev);
			//messageHandler->writeMessage("DEBUG", "[" + callsign + "] PBN \"" + vsid::fpln::getPbn(FlightPlan) + "\"", vsid::MessageHandler::DebugArea::Dev);
			//messageHandler->writeMessage("DEBUG", "[" + callsign + "] capabilites \"" + FlightPlan.GetFlightPlanData().GetCapibilities() + "\"", vsid::MessageHandler::DebugArea::Dev);
			// END DEV

			std::string sidName = this->processed[callsign].sid.name();
			std::string customSidName = this->processed[callsign].customSid.name();

			if (((atcBlock.first == fplnData.GetOrigin() &&
				this->processed[callsign].atcRWY &&
				this->processed[callsign].customSid.empty()) ||
				(atcBlock.first == "" &&
				this->processed[callsign].sid.empty() &&
				this->processed[callsign].customSid.empty())) &&
				std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "I"
				)
			{
				*pRGB = this->configParser.getColor("noSid");
			}
			else if ((atcBlock.first == "" ||
					atcBlock.first == fplnData.GetOrigin()) &&
					((this->processed[callsign].customSid.empty() ||
					this->processed[callsign].sid == this->processed[callsign].customSid) ||
					((std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V" &&
					std::string(sItemString) == "VFR")
					))
					)
			{
				*pRGB = this->configParser.getColor("sidSuggestion");
			}
			else if (atcBlock.first != "" &&
					atcBlock.first == fplnData.GetOrigin() &&
					!this->processed[callsign].customSid.empty() &&
					this->processed[callsign].sid != this->processed[callsign].customSid
					)
			{
				*pRGB = this->configParser.getColor("customSidSuggestion");
			}
			else if ((atcBlock.first != "" &&
					atcBlock.first != fplnData.GetOrigin() &&
					atcBlock.first == customSidName &&
					this->processed[callsign].sid != this->processed[callsign].customSid) ||
					atcBlock.first != sidName
					)
			{
				*pRGB = this->configParser.getColor("customSidSet");
			}
			else if (atcBlock.first != "" && atcBlock.first == sidName)
			{
				*pRGB = this->configParser.getColor("suggestedSidSet");
			}
			else
			{
				*pRGB = RGB(140, 140, 60);
			}

			if (atcBlock.first != "" && atcBlock.first != fplnData.GetOrigin())
			{
				strcpy_s(sItemString, 16, atcBlock.first.c_str());
			}
			else if ((atcBlock.first == "" ||
					atcBlock.first == fplnData.GetOrigin()) &&
					std::string(FlightPlan.GetFlightPlanData().GetPlanType()) == "V"
					)
			{
				strcpy_s(sItemString, 16, "VFR");
			}
			else if ((atcBlock.first != "" &&
					atcBlock.first == fplnData.GetOrigin() &&
					this->processed[callsign].atcRWY &&
					this->processed[callsign].customSid.rwy.find(atcBlock.second) == std::string::npos) ||
					(this->processed[callsign].sid.empty() &&
					this->processed[callsign].customSid.empty())
					)
			{
				strcpy_s(sItemString, 16, "MANUAL");
			}
			else if(sidName != "" && customSidName == "")
			{
				strcpy_s(sItemString, 16, sidName.c_str());
			}
			else if (customSidName != "")
			{
				strcpy_s(sItemString, 16, customSidName.c_str());
			}
		}
		else if(this->activeAirports.contains(fplnData.GetOrigin()) && RadarTarget.GetGS() <= 50 )
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
				else if (!FlightPlan.GetClearenceFlag() && atcBlock.first != fplnData.GetOrigin() && fplnData.IsAmended())
				{
					// prevent automode to use rwys set before
					atcBlock.second = "";
				}
			}
			if (atcBlock.second != "" &&
				//(vsid::fpln::findRemarks(fplnData, "VSID/RWY") ||
				(vsid::fpln::findRemarks(FlightPlan, "VSID/RWY") ||
				atcBlock.first != fplnData.GetOrigin() ||
				fplnData.IsAmended())
				)
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] not yet processed, calling processFlightplan with atcRwy: " +
											atcBlock.second + ((!checkOnly) ? " and setting the fpln." : " and only checking the fpln"),
											vsid::MessageHandler::DebugArea::Sid);
				this->processFlightplan(FlightPlan, checkOnly, atcBlock.second);
			}
			else
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] not yet processed, calling processFlightplan without atcRwy" +
											((!checkOnly) ? " and setting the fpln." : " and only checking the fpln"),
											vsid::MessageHandler::DebugArea::Sid);
				this->processFlightplan(FlightPlan, checkOnly);
			}
			
		}
	}

	if (ItemCode == TAG_ITEM_VSID_CLIMB)
	{
		*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
		EuroScopePlugIn::CFlightPlan fpln = FlightPlan;

		int transAlt = this->activeAirports[fplnData.GetOrigin()].transAlt;
		int tempAlt = 0;
		int climbVia = 0;

		if (this->processed.find(callsign) != this->processed.end())
		{
			// determine if a found non-standard sid exists in valid sids and set it for ic comparison

			std::string sidName = this->processed[callsign].sid.name();
			std::string customSidName = this->processed[callsign].customSid.name();
			std::string atcSid = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), fplnData.GetOrigin()).first;
			
			// if an unknown Sid is set (non-standard or non-custom) try to find matching Sid in config
			if (atcSid != "" && atcSid != fplnData.GetOrigin() && atcSid != sidName && atcSid != customSidName)
			{
				for (vsid::Sid& sid : this->activeAirports[fplnData.GetOrigin()].sids)
				{
					if (atcSid.find_first_of("0123456789") != std::string::npos)
					{
						if (sid.waypoint != atcSid.substr(0, atcSid.length() - 2)) continue;
						if (sid.designator != std::string(1, atcSid[atcSid.length() - 1])) continue;
						if (sid.number != std::string(1, atcSid[atcSid.length() - 2])) break;
					}
					else
					{
						if (sid.base != atcSid) continue;
					}
					
					this->processed[callsign].customSid = sid;
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

			if (this->processed[callsign].sid.initialClimb != 0 &&
				this->processed[callsign].customSid.empty() &&
				(atcSid == sidName || atcSid == "" || atcSid == fplnData.GetOrigin())
				)
			{
				tempAlt = this->processed[callsign].sid.initialClimb;
			}
			else if (this->processed[callsign].customSid.initialClimb != 0 &&
					(atcSid == customSidName || atcSid == "" || atcSid == fplnData.GetOrigin())
					)
			{
				tempAlt = this->processed[callsign].customSid.initialClimb;
			}

			if (fpln.GetClearedAltitude() == fpln.GetFinalAltitude())
			{
				*pRGB = this->configParser.getColor("suggestedClmb"); // white
			}
			else if (fpln.GetClearedAltitude() != fpln.GetFinalAltitude() &&
					fpln.GetClearedAltitude() == tempAlt
					)
			{
				if (climbVia)
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
				if(fpln.GetClearedAltitude() == 0)
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
			std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(
																		vsid::utils::split(fplnData.GetRoute(), ' '),
																		fplnData.GetOrigin()
																		);

			if (atcBlock.first == fplnData.GetOrigin() &&
				!this->processed[callsign].atcRWY &&
				!this->processed[callsign].remarkChecked
				)
			{
				/*this->processed[callsign].atcRWY = vsid::fpln::findRemarks(fplnData, "VSID/RWY");*/
				this->processed[callsign].atcRWY = vsid::fpln::findRemarks(FlightPlan, "VSID/RWY");
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
			else if (!this->processed[callsign].atcRWY && // MONITOR
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
					try
					{
						sidRwy = vsid::utils::split(this->processed[callsign].sid.rwy, ',').at(0);
					}
					catch (std::out_of_range)
					{
						messageHandler->writeMessage("ERROR", "Failed to get RWY in the RWY menu. Check config " +
							this->activeAirports[fplnData.GetOrigin()].icao + " for SID \"" +
							this->processed[callsign].sid.idName() + "\". RWY value is: " +
							this->processed[callsign].sid.rwy);
					}
				}
				if (sidRwy == "" && std::string(fplnData.GetDepartureRwy()) != "")
				{
					sidRwy = fplnData.GetDepartureRwy();
				}
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
		
		if(assignedSquawk != "0000") strcpy_s(sItemString, 16, assignedSquawk.c_str());
	}

	if (ItemCode == TAG_ITEM_VSID_REQ)
	{
		if (this->processed.contains(callsign) && this->processed[callsign].request && this->activeAirports.contains(fplnData.GetOrigin()))
		{
			for (auto& request : this->activeAirports[fplnData.GetOrigin()].requests)
			{
				bool found = false;
				for (std::set<std::pair<std::string, long long>>::iterator it = request.second.begin(); it != request.second.end();++it)
				{
					if (it->first != callsign) continue;

					int pos = std::distance(it, request.second.end());
					std::string req = vsid::utils::toupper(request.first).at(0) + std::to_string(pos);
					strcpy_s(sItemString, 16, req.c_str());
					found = true;
					break;
				}
				if (found) break;
			}
		}
	}

	if (ItemCode == TAG_ITEM_VSID_REQTIMER)
	{
		if (this->processed.contains(callsign) && this->processed[callsign].request && this->activeAirports.contains(fplnData.GetOrigin()))
		{
			*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
			long long now = std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()).time_since_epoch().count();
			for (auto& request : this->activeAirports[fplnData.GetOrigin()].requests)
			{
				for (auto& fp : request.second)
				{
					if (fp.first != callsign) continue;
					
					int minutes = static_cast<int>((now - fp.second) / 60);

					if (minutes < this->configParser.getReqTime("caution")) *pRGB = this->configParser.getColor("requestNeutral");
					else if (minutes >= this->configParser.getReqTime("caution") &&
							minutes < this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestCaution");
					else if (minutes >= this->configParser.getReqTime("warning")) *pRGB = this->configParser.getColor("requestWarning");

					std::string strMinute = std::to_string(minutes) + "m";
					strcpy_s(sItemString, 16, strMinute.c_str());
				}
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
				"Debug - toggle debug mode");
			return true;
		}
		if (vsid::utils::tolower(command[1]) == "version")
		{
			messageHandler->writeMessage("INFO", "vSID Version " + pluginVersion + " loaded.");
			return true;
		}
		// debugging only
		else if (vsid::utils::tolower(command[1]) == "removed")
		{
			for (auto &elem : this->removeProcessed)
			{
				messageHandler->writeMessage("DEBUG", "[" + elem.first + "] being removed at: " + vsid::time::toFullString(elem.second.first) +
											" and is disconnected " + ((elem.second.second) ? "YES" : "NO"));
			}
			if (this->removeProcessed.size() == 0)
			{
				messageHandler->writeMessage("DEBUG", "Removed list empty");
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

						std::erase_if(this->processed, [&](auto item)
							{
								EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(item.first.c_str());
								EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
								if (fpln.IsValid() &&
									!fpln.GetClearenceFlag() &&
									this->activeAirports.contains(fplnData.GetOrigin()) &&
									this->activeAirports[fplnData.GetOrigin()].settings["auto"]
									)
								{
									return true;
								}
								else return false;
							}
						);

						for (std::pair<const std::string, vsid::fpln::Info> pFpln : this->processed)
						{
							EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(pFpln.first.c_str());
							EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
							auto atcBlock = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), fplnData.GetRoute());
							messageHandler->writeMessage("DEBUG", "[" + pFpln.first + "] rechecking due to rule change.", vsid::MessageHandler::DebugArea::Sid);
							if (atcBlock.second != "" && fpln.IsValid())
							{
								this->processFlightplan(FlightPlanSelect(pFpln.first.c_str()), true, atcBlock.second);
							}
							else if(fpln.IsValid()) this->processFlightplan(FlightPlanSelect(pFpln.first.c_str()), true);
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
				if (this->activeAirports.count(vsid::utils::toupper(command[2])))
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
				if (this->activeAirports.count(vsid::utils::toupper(command[2])))
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
			
			try
			{
				atcIcao = vsid::utils::split(ControllerMyself().GetCallsign(), '_').at(0);
			}
			catch (std::out_of_range)
			{
				messageHandler->writeMessage("ERROR", "Failed to get own ATC ICAO for automode");
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

					// remove processed flightplans if they're not cleared or if the set rwy is not part of depRwys anymore
					std::erase_if(this->processed, [&](std::pair<std::string, vsid::fpln::Info> pFpln)
						{
							EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(pFpln.first.c_str());
							EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
							std::string icao = fplnData.GetOrigin();
							std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), icao);
							if (this->activeAirports.contains(fplnData.GetOrigin()) &&
								this->activeAirports[icao].settings["auto"] &&
								!fpln.GetClearenceFlag() && !pFpln.second.atcRWY
								)
							{
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
							// remove processed flightplans if they're not cleared or if the set rwy is not part of depRwys anymore
							std::erase_if(this->processed, [&](std::pair<std::string, vsid::fpln::Info> pFpln)
								{
									EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(pFpln.first.c_str());
									EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
									std::string icao = fplnData.GetOrigin();
									std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), icao);
									if (*it == icao && !fpln.GetClearenceFlag() && !pFpln.second.atcRWY)
									{
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
					
					std::erase_if(this->processed, [&](auto item)
						{
							EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(item.first.c_str());
							EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
							if (fpln.IsValid() &&
								!fpln.GetClearenceFlag() &&
								this->activeAirports.contains(fplnData.GetOrigin()) &&
								this->activeAirports[fplnData.GetOrigin()].settings["auto"]
								)
							{
								return true;
							}
							else return false;
						}
					);

					for (std::pair<const std::string, vsid::fpln::Info> pFpln : this->processed)
					{
						EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(pFpln.first.c_str());
						EuroScopePlugIn::CFlightPlanData fplnData = fpln.GetFlightPlanData();
						auto atcBlock = vsid::fpln::getAtcBlock(vsid::utils::split(fplnData.GetRoute(), ' '), fplnData.GetRoute());
						messageHandler->writeMessage("DEBUG", "[" + pFpln.first + "] Rechecking due to area change.", vsid::MessageHandler::DebugArea::Sid);
						if (atcBlock.second != "" && fpln.IsValid())
						{
							this->processFlightplan(FlightPlanSelect(pFpln.first.c_str()), true, atcBlock.second);
						}
						else if(fpln.IsValid()) this->processFlightplan(FlightPlanSelect(pFpln.first.c_str()), true);
					}
				}
			}
			else messageHandler->writeMessage("INFO", vsid::utils::toupper(command[2]) + " not in active airports");
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "sync")
		{
			messageHandler->writeMessage("DEBUG", "Syncinc all requests.", vsid::MessageHandler::DebugArea::Req);
			for (std::pair<const std::string, vsid::fpln::Info> &fp : this->processed)
			{
				if (fp.second.request)
				{
					EuroScopePlugIn::CFlightPlan fpln = FlightPlanSelect(fp.first.c_str());

					if (!fpln.IsValid()) continue;

					std::string icao = fpln.GetFlightPlanData().GetOrigin();
					if(!this->activeAirports.contains(icao)) continue;
					
					bool found = false;
					for (auto& request : this->activeAirports[icao].requests)
					{
						for (auto& fpReq : request.second)
						{
							if (fpReq.first != fp.first) continue;

							found = true;
							// EuroScopePlugIn::CFlightPlanControllerAssignedData cad = fpln.GetControllerAssignedData();
							std::string scratch = ".vsid_req_" + request.first + "/" + std::to_string(fpReq.second);

							messageHandler->writeMessage("DEBUG", "[" + fp.first + "] syncing " + request.first +
														" with scratch: " + scratch, vsid::MessageHandler::DebugArea::Req
														);
							/*vsid::fpln::setScratchPad(cad, scratch);*/
							vsid::fpln::setScratchPad(fpln, scratch);
							break;
						}
						if (found) break;
					}
				}
			}
			return true;
		}
		else if (vsid::utils::tolower(command[1]) == "req")
		{
			if (command.size() == 3)
			{
				std::string icao = vsid::utils::toupper(command[2]);

				if (!this->activeAirports.contains(icao))
				{
					messageHandler->writeMessage("WARNING", icao + " not in active airports. Cannot check for requests");
					return true;
				}

				std::ostringstream ss;

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
			}

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
		else
		{
			messageHandler->writeMessage("INFO", "Unknown command");
			return true;
		}
	}
	return false;
}

//EuroScopePlugIn::CRadarScreen* vsid::VSIDPlugin::OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated) /// DEV
//{
//	messageHandler->writeMessage("INFO", "OnRadarScreenCreated called");
//	this->radarScreen = new vsid::Display();
//	return this->radarScreen;
//}

void vsid::VSIDPlugin::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	// if we receive updates for flightplans validate entered sid again to sync between controllers
	// updates are received for any flightplan not just that under control

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();

	
	if (this->processed.contains(callsign))
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] flightplan updated", vsid::MessageHandler::DebugArea::Dev);

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
		//									" due to more than 3 flightplan changes in the last 3 seconds");
		//		this->processed[callsign].updateCounter = 1;
		//	}
		//	else if()*/

		//	this->processed[callsign].lastUpdate = updateNow;
		//}
		
		
		// messageHandler->writeMessage("DEBUG", callsign + " fplnupdate called");

		std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
		if (filedRoute.size() > 0)
		{
			std::pair<std::string, std::string> atcBlock = vsid::fpln::getAtcBlock(filedRoute, fplnData.GetOrigin());
			if (this->activeAirports[fplnData.GetOrigin()].settings["auto"] &&
				atcBlock.first == "")
			{
				this->processed.erase(callsign);
				return;
			}

			if (!this->processed[callsign].atcRWY &&
				(fplnData.IsAmended() || FlightPlan.GetClearenceFlag() ||
				atcBlock.first != fplnData.GetOrigin() ||
				//(atcBlock.first == fplnData.GetOrigin() && vsid::fpln::findRemarks(fplnData, "VSID/RWY")))
				(atcBlock.first == fplnData.GetOrigin() && vsid::fpln::findRemarks(FlightPlan, "VSID/RWY")))
				)
			{
				this->processed[callsign].atcRWY = true;
			}

			/*if (vsid::fpln::findRemarks(fplnData, "VSID/RWY") &&*/
			if (vsid::fpln::findRemarks(FlightPlan, "VSID/RWY") &&
				atcBlock.first != fplnData.GetOrigin() &&
				ControllerMyself().IsController()
				)
			{
				this->processed[callsign].noFplnUpdate = true;
				//if (!vsid::fpln::removeRemark(fplnData, "VSID/RWY"))
				if (!vsid::fpln::removeRemark(FlightPlan, "VSID/RWY"))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to remove remarks! - #FFDU");
					this->processed[callsign].noFplnUpdate = false;
				}
				//else this->processed[callsign].noFplnUpdate = false;
				if (!fplnData.AmendFlightPlan())
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - Failed to amend Flighplan! - #FFDU");
					this->processed[callsign].noFplnUpdate = false;
				}
				//else this->processed[callsign].noFplnUpdate = false;
			}

			// DEV
			// if we want to supress an update that happened and skip sid checking  /////////////// needs further checking - this the right position? now several flightplanupdates again
			if (this->processed[callsign].noFplnUpdate)
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] nofplnUpdate true. Disabling.", vsid::MessageHandler::DebugArea::Dev);
				this->processed[callsign].noFplnUpdate = false;
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] nofplnUpdate after disabling " + ((this->processed[callsign].noFplnUpdate) ? "TRUE" : "FALSE"),
											vsid::MessageHandler::DebugArea::Dev);
				return;
			}

			// END DEV
			if (atcBlock.first == fplnData.GetOrigin() && atcBlock.second != "")
			{
				messageHandler->writeMessage("DEBUG", "[" + callsign + "] fpln updated, calling processFlightplan with atcRwy : " + atcBlock.second,
											vsid::MessageHandler::DebugArea::Sid
				);
				this->processFlightplan(FlightPlan, true, atcBlock.second);
			}
			else if (atcBlock.first != fplnData.GetOrigin() && atcBlock.second != "")
			{
				vsid::Sid atcSid;
				for (vsid::Sid& sid : this->activeAirports[fplnData.GetOrigin()].sids)
				{
					if (atcBlock.first.find_first_of("0123456789") != std::string::npos)
					{
						if (sid.waypoint != atcBlock.first.substr(0, atcBlock.first.length() - 2)) continue;
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
			else
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
	std::string icao = FlightPlan.GetFlightPlanData().GetOrigin();

	if (!this->activeAirports.contains(icao)) return;

	std::string scratchpad = cad.GetScratchPadString();

	if (this->processed.contains(callsign) && scratchpad.size() > 0)
	{
		if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_SCRATCH_PAD_STRING && this->activeAirports[icao].settings["auto"])
		{
			if (scratchpad.find(".VSID_AUTO_") != std::string::npos)
			{
				std::string toFind = ".VSID_AUTO_";
				size_t pos = scratchpad.find(toFind);

				if (pos != std::string::npos)
				{
					std::string atc = scratchpad.substr(pos + toFind.size(), scratchpad.size());
					if (ControllerMyself().GetCallsign() != atc)
					{
						messageHandler->writeMessage("WARNING", "[" + callsign + "] assigned SID \"" + FlightPlan.GetFlightPlanData().GetSidName() + "\" by " + atc);
					}
				}
				/*vsid::fpln::removeScratchPad(cad, scratchpad.substr(pos, scratchpad.size()));*/
				vsid::fpln::removeScratchPad(FlightPlan, scratchpad.substr(pos, scratchpad.size()));
			}
		}
		if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_SCRATCH_PAD_STRING)
		{
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
					for (auto it = this->activeAirports[icao].requests.begin(); it != this->activeAirports[icao].requests.end();++it)
					{
						for (std::set<std::pair<std::string, long long>>::iterator jt = it->second.begin(); jt != it->second.end();)
						{
							if (jt->first != callsign)
							{
								++jt;
								continue;
							}
							this->processed[callsign].request = false;
							messageHandler->writeMessage("DEBUG", "[" + callsign + "] removing from requests in: " + it->first, vsid::MessageHandler::DebugArea::Req);
							jt = it->second.erase(jt);
						}
						if (it->first == reqType && this->processed.contains(callsign))
						{
							messageHandler->writeMessage("DEBUG", "[" + callsign + "] setting in requests in: " + it->first, vsid::MessageHandler::DebugArea::Req);
							it->second.insert({ callsign, reqTime });
							this->processed[callsign].request = true;
						}
					}
					/*vsid::fpln::removeScratchPad(cad, scratchpad.substr(pos, scratchpad.size()));*/
					vsid::fpln::removeScratchPad(FlightPlan, scratchpad.substr(pos, scratchpad.size()));
				}
				catch (std::out_of_range)
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] failed to set the request");
				}
			}
		}
	}
	else if (this->processed.contains(callsign) && this->processed[callsign].request && this->activeAirports.contains(icao))
	{
		if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_CLEARENCE_FLAG)
		{
			if (FlightPlan.GetClearenceFlag())
			{
				for (auto& fp : this->activeAirports[icao].requests["clearance"])
				{
					if (fp.first != callsign) continue;

					this->activeAirports[icao].requests["clearance"].erase(fp);
					this->processed[callsign].request = false;
					break;
				}
			}
		}
		if (DataType == EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE)
		{
			std::string state = FlightPlan.GetGroundState();

			if (state == "STUP")
			{
				for (auto& fp : this->activeAirports[icao].requests["startup"])
				{
					if (fp.first != callsign) continue;

					this->activeAirports[icao].requests["startup"].erase(fp);
					this->processed[callsign].request = false;
					break;
				}
			}
			else if (state == "PUSH")
			{
				for (auto& fp : this->activeAirports[icao].requests["pushback"])
				{
					if (fp.first != callsign) continue;

					this->activeAirports[icao].requests["pushback"].erase(fp);
					this->processed[callsign].request = false;
					break;
				}
			}
			else if (state == "TAXI")
			{
				for (auto& fp : this->activeAirports[icao].requests["taxi"])
				{
					if (fp.first != callsign) continue;

					this->activeAirports[icao].requests["taxi"].erase(fp);
					this->processed[callsign].request = false;
					break;
				}
				
			}
			else if (state == "DEPA")
			{
				for (auto& fp : this->activeAirports[icao].requests["departure"])
				{
					if (fp.first != callsign) continue;

					this->activeAirports[icao].requests["departure"].erase(fp);
					this->processed[callsign].request = false;
					break;
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
		auto now = std::chrono::utc_clock::now() + std::chrono::minutes{ 1 };
		this->removeProcessed[callsign] = { now, true };

		if (this->processed[callsign].request && this->activeAirports.contains(icao))
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
					this->processed[callsign].request = false;
					jt = it->second.erase(jt);
				}
			}
		}
	}
}

void vsid::VSIDPlugin::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget RadarTarget)
{
	if (!RadarTarget.IsValid()) return;

	std::string callsign = RadarTarget.GetCallsign();

	if (this->processed.contains(callsign) &&
		RadarTarget.GetGS() >= 50)
	{
		
		if (!this->removeProcessed.contains(callsign))
		{
			auto now = std::chrono::utc_clock::now() + std::chrono::minutes{ 10 };
			this->removeProcessed[callsign] = { now, false };
		}

		std::string icao = RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetOrigin();

		if (this->processed[callsign].request && this->activeAirports.contains(icao))
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
					this->processed[callsign].request = false;
					jt = it->second.erase(jt);
				}
			}
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
	}
	catch (std::out_of_range)
	{
		messageHandler->writeMessage("ERROR", "Failed to get ICAO part of controller callsign \"" + atcCallsign + "\" in ATC update.");
	}
	if (atcCallsign == ControllerMyself().GetCallsign()) return;
	else if (this->actAtc.contains(atcSI) ||
		this->ignoreAtc.contains(atcSI))
		{
			return;
	}
	else if (!Controller.IsController())
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because it is not a controller.",
									vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}
	else if (atcCallsign.find("ATIS") != std::string::npos)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Adding ATC to ignore list.",
									vsid::MessageHandler::DebugArea::Atc
		);
		this->ignoreAtc.insert(atcSI);
		return;
	}
	else if (atcSI.find_first_of("0123456789") != std::string::npos)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the SI contains a number (" + atcSI + ").",
									vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}
	else if (atcFac < 2)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the facility is below 2 (usually FIS).",
									vsid::MessageHandler::DebugArea::Atc
		);
		return;
	}
	else if (atcFreq == 0.0 || atcFreq > 199.0)
	{
		messageHandler->writeMessage("DEBUG", "[" + atcCallsign + "] Skipping ATC because the freq. is invalid (" + std::to_string(atcFreq) + ").",
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
	//dev only
	//this->detectPlugins();
	// end dev
	this->UpdateActiveAirports();
}

void vsid::VSIDPlugin::UpdateActiveAirports()
{
	messageHandler->writeMessage("INFO", "Updating airports...");
	this->savedSettings.clear();
	this->savedRules.clear();
	this->savedAreas.clear();
	this->savedRequests.clear();
	for (std::pair<const std::string, vsid::Airport>& apt : this->activeAirports)
	{
		this->savedSettings.insert({ apt.first, apt.second.settings });
		this->savedRules.insert({ apt.first, apt.second.customRules });
		this->savedAreas.insert({ apt.first, apt.second.areas });
		this->savedRequests.insert({ apt.first, apt.second.requests });
	}

	this->SelectActiveSectorfile();
	this->activeAirports.clear();
	this->processed.clear();
      
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
		this->configParser.loadAirportConfig(this->activeAirports, this->savedRules, this->savedSettings, this->savedAreas, this->savedRequests);
		messageHandler->writeMessage("DEBUG", "Checking .ese file for SID mastering...", vsid::MessageHandler::DebugArea::Conf);

		// if there are configured airports check for remaining sid data
		for (EuroScopePlugIn::CSectorElement sfe = this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS);
			sfe.IsValid();
			sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS)
			)
		{
			if (!this->activeAirports.contains(vsid::utils::trim(sfe.GetAirportName()))) continue;

			std::string name = sfe.GetName();
			
			for (vsid::Sid& sid : this->activeAirports[vsid::utils::trim(sfe.GetAirportName())].sids)
			{
				if (sid.designator != "")
				{
					if (sid.base != name.substr(0, name.length() - 2)) continue;
					if (sid.designator != std::string(1, name[name.length() - 1])) continue;

					if (std::string("0123456789").find_first_of(name[name.length() - 2]) != std::string::npos)
					{
						sid.number = name[name.length() - 2];
						messageHandler->writeMessage("DEBUG", "[" + sid.base + sid.number + sid.designator + "] mastered", vsid::MessageHandler::DebugArea::Conf);
					}
				}
				else
				{
					if (name != sid.base) continue;
					sid.number = 'X';
					messageHandler->writeMessage("DEBUG", "[" + sid.base + "] has no designator but the base could be mastered", vsid::MessageHandler::DebugArea::Conf);
				}
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
	std::map<std::string, std::set<std::string>> incompSids;
	for (std::pair<const std::string, vsid::Airport> &apt : this->activeAirports)
	{
		for (vsid::Sid& sid : apt.second.sids)
		{
			if (sid.designator != "")
			{
				if (std::string("0123456789").find_first_of(sid.number) == std::string::npos)
				{
					incompSids[apt.first].insert(sid.base + '?' + sid.designator);
				}
			}
			else
			{
				if (sid.number != "X") incompSids[apt.first].insert(sid.base);
			}
		}
	}

	// fallback check if incompatible SIDs remain after checking .ese file. Now .sct is checked
	if (incompSids.size() > 0)
	{
		messageHandler->writeMessage("DEBUG", "Incompatible SIDs found. Rechecking in .sct file...", vsid::MessageHandler::DebugArea::Conf);
		for (EuroScopePlugIn::CSectorElement sfe = this->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_SID);
			sfe.IsValid();
			sfe = this->SectorFileElementSelectNext(sfe, EuroScopePlugIn::SECTOR_ELEMENT_SID))
		{
			std::vector<std::string> components = vsid::utils::split(std::string(sfe.GetName()), ' ');
			std::string compIcao = "";
			std::string compSid = "";

			if (components.size() == 4)
			{
				compIcao = components.at(0);
				compSid = components.at(3);
			}
			else continue;

			if (!incompSids.contains(compIcao)) continue;
			if (!this->activeAirports.contains(compIcao)) continue;

			for (vsid::Sid& sid : this->activeAirports[compIcao].sids)
			{
				if (sid.number != "") continue;

				if (sid.designator != "")
				{
					if (sid.base != compSid.substr(0, compSid.length() - 2)) continue;
					if (sid.designator != std::string(1, compSid[compSid.length() - 1])) continue;

					if (std::string("0123456789").find_first_of(compSid[compSid.length() - 2]) != std::string::npos)
					{
						sid.number = compSid[compSid.length() - 2];
						incompSids[compIcao].erase(sid.base + '?' + sid.designator);
						messageHandler->writeMessage("DEBUG", "[" + sid.base + sid.number + sid.designator + "] mastered", vsid::MessageHandler::DebugArea::Conf);
					}
				}
				else
				{
					if (sid.base != compSid) continue;
					incompSids[compIcao].erase(sid.base);
					messageHandler->writeMessage("DEBUG", "[" + sid.base + "] has no designator but the base could be mastered", vsid::MessageHandler::DebugArea::Conf);
				}
			}

			if (incompSids[compIcao].size() == 0) incompSids.erase(compIcao);

			if (incompSids.size() == 0)
			{
				messageHandler->writeMessage("DEBUG", "All incompatible SIDs mastered, stopping fallback checks...", vsid::MessageHandler::DebugArea::Conf);
				break;
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
			for (auto it = this->activeAirports[incompSidPair.first].sids.begin(); it != this->activeAirports[incompSidPair.first].sids.end();)
			{
				if (it->designator != "")
				{
					if (it->waypoint == incompSid.substr(0, incompSid.length() - 2) && it->designator == std::string(1, incompSid[incompSid.length() - 1]))
					{
						messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->waypoint + it->number + it->designator + "] incompatible and erased");
						it = this->activeAirports[incompSidPair.first].sids.erase(it);
						continue;
					}
				}
				else
				{
					if (it->base == incompSid)
					{
						messageHandler->writeMessage("DEBUG", "[" + incompSidPair.first + "] [" + it->base + "] incompatible and erased");
						it = this->activeAirports[incompSidPair.first].sids.erase(it);
						continue;
					}
				}
				++it;
			}
		}
	}

	// remove dummy value for SIDs without designator
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

	std::pair<std::string, std::string> msg = messageHandler->getMessage();
	if (msg.first != "" && msg.second != "")
	{
		DisplayUserMessage("vSID", msg.first.c_str(), msg.second.c_str(), true, true, false, true, false);
	}

	if (this->removeProcessed.size() > 0)
	{
		auto now = std::chrono::utc_clock::now();

		for (auto it = this->removeProcessed.begin(); it != this->removeProcessed.end();)
		{
			if (now > it->second.first)
			{
				std::erase_if(this->processed, [&](auto fpln) { return it->first == fpln.first; });
				it = this->removeProcessed.erase(it);
			}
			else ++it;
		}
	}
}
/*
* END ES FUNCTIONS
*/

void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	// create the instance
	*ppPlugInInstance = vsidPlugin = new vsid::VSIDPlugin();
	//*ppPlugInInstance = vsidApp.vsidPlugin = new vsid::VSIDPlugin();
}


//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{
	// delete the instance
	delete vsidPlugin;
	//delete vsidApp.vsidPlugin;
}
