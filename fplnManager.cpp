#include "pch.h"

#include "fplnManager.h"
#include "messageHandler.h"
#include "constants.h"
#include "airportManager.h"
#include "syncManager.h"

#include "vSIDPlugin.h"

using AirportManager = vsid::apt::AirportManager;
using SyncManager = vsid::sync::SyncManager;

std::vector<std::string> vsid::fpln::clean(const EuroScopePlugIn::CFlightPlan& FlightPlan, std::string filedSidWpt)
{
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string origin = fplnData.GetOrigin();
	std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
	std::pair<std::string, std::string> atcBlock = getAtcBlock(FlightPlan);

	if (!filedRoute.empty())
	{
		try
		{
			if (filedRoute.at(0).find('/') != std::string::npos && filedRoute.at(0).find("/N") == std::string::npos)
			{
				filedRoute.erase(filedRoute.begin());
			}
		}
		catch (std::out_of_range)
		{
			vsid::Logger::log(
				vsid::LogLevel::Error,
				std::format(
					"[{}] Error during cleaning of route at first entry. ADEP [{}] with route [{}]. Code: {}",
					callsign,
					origin,
					vsid::utils::join(filedRoute | std::views::drop(0)),
					ERROR_FPLN_CLNFIRST
				)
			);
		}
	}

	/* stop cleaning if flight plan is VFR */

	if (std::string(fplnData.GetPlanType()) == "V") return filedRoute;

	/* if a possible SID block was found check the entire route until the sid waypoint is found*/
	if (filedRoute.size() > 0 && filedSidWpt != "")
	{
		for (std::vector<std::string>::iterator it = filedRoute.begin(); it != filedRoute.end();)
		{
			try
			{
				*it = vsid::utils::split(*it, '/').at(0); // to fetch wrong speed/level groups
			}
			catch (std::out_of_range)
			{
				vsid::Logger::log(vsid::LogLevel::Error, std::format("[{}] Error during cleaning of route. Cleaning was continued after false entry. "
					"ADEP [{}] with route [{}]. Code: {}", callsign, origin, vsid::utils::join(filedRoute), ERROR_FPLN_CLNSPDLVL));
			}
			if (*it == filedSidWpt) break;
			it = filedRoute.erase(it);
		}
	}

	/* if the route has no sid waypoint clean up until the probably first waypoint*/
	else if (filedRoute.size() > 0 && filedSidWpt == "")
	{
		for (std::vector<std::string>::iterator it = filedRoute.begin(); it != filedRoute.end();)
		{
			try
			{
				*it = vsid::utils::split(*it, '/').at(0); // to fetch wrong speed/level groups
			}
			catch (std::out_of_range)
			{
				vsid::Logger::log(vsid::LogLevel::Error, std::format("[{}] Error during cleaning of route. Cleaning was continued "
					"after false entry. ADEP [{}] with route [{}]. Code: {}",
					callsign, origin, vsid::utils::join(filedRoute), ERROR_FPLN_CLNSPDLVL));
			}
			if (*it != origin) break;
			it = filedRoute.erase(it);
		}
	}

	if (filedRoute.size() == 0 && vsid::utils::split(fplnData.GetRoute(), ' ').size() != 0)
	{
		vsid::Logger::log(vsid::LogLevel::Warning, std::format("[{}] did not clean route as cleaning resulted in an empty route "
			"(possible error in the filed route). Returning original route.", callsign));

		return vsid::utils::split(fplnData.GetRoute(), ' ');
	}

	return filedRoute;
}

std::pair<std::string, std::string> vsid::fpln::getAtcBlock(const EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	std::vector<std::string> filedRoute = vsid::utils::split(FlightPlan.GetFlightPlanData().GetRoute(), ' ');
	std::string origin = FlightPlan.GetFlightPlanData().GetOrigin();
	std::string callsign = FlightPlan.GetCallsign();
	std::string atcRwy = "";
	std::string atcSid = "";

	if (filedRoute.size() > 0)
	{
		try
		{
			if (filedRoute.at(0).find('/') != std::string::npos && filedRoute.at(0).find("/N") == std::string::npos)
			{
				std::vector<std::string> sidBlock = vsid::utils::split(filedRoute.at(0), '/');

				if (sidBlock.at(0).find_first_of("0123456789RV") != std::string::npos || sidBlock.at(0) == origin)
				{
					atcSid = sidBlock.front();
					atcRwy = sidBlock.back();
				}
			}

			messageHandler->removeFplnError(callsign, ERROR_FPLN_ATCBLOCK);
		}
		catch (std::out_of_range)
		{
			if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_ATCBLOCK))
			{
				vsid::Logger::log(vsid::LogLevel::Error, std::format("[{}] Failed to get ATC block. First route entry [{}]. Code: {}",
					callsign, filedRoute.at(0), ERROR_FPLN_ATCBLOCK));

				messageHandler->addFplnError(callsign, ERROR_FPLN_ATCBLOCK);
			}
		}
	}
	return { atcSid, atcRwy };
}

std::string vsid::fpln::getTransition(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::map<std::string, vsid::Transition>& transition,
	const std::string& filedSidWpt)
{
	if (transition.empty()) return "";

	std::vector<std::string> route = vsid::fpln::clean(FlightPlan, filedSidWpt);

	for (auto& [base, trans] : transition)
	{
		if (std::find(route.begin(), route.end(), base) == route.end()) continue;
		return trans.base + trans.number + trans.designator;
	}

	vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] no matching transition wpt found", FlightPlan.GetCallsign()), vsid::DebugLevel::Sid);

	return ""; // fallback if no transition could be matched
}

std::pair<std::string, std::string> vsid::fpln::splitTransition(std::string atcSid) // #refactor to string_view
{
	if (atcSid.size() < 2) return { "", "" };
	atcSid = vsid::utils::trim(atcSid);

	std::size_t pos = 0;
	std::pair<std::string, std::string> fb = { atcSid, "" };

	while ((pos = atcSid.find_first_of("xX", pos)) != std::string::npos)
	{
		if (pos == 0 || pos >= atcSid.size() - 1) { ++pos; continue; }

		const std::string sid = atcSid.substr(0, pos);
		const std::string trans = atcSid.substr(pos + 1);

		if (sid.size() >= 3 && trans.size() >= 3 && vsid::utils::containsDigit(sid))
		{
			if (trans.front() != 'x' && trans.front() != 'X') return { sid, trans };
			if (fb.second.empty()) fb = { sid, trans }; // save trans starting with x as fallback
		}

		++pos;
	}

	if (!fb.second.empty()) return fb;

	return { atcSid, "" };
}

std::pair<std::string_view, std::string_view> vsid::fpln::splitTransitionSV(std::string_view atcSid)
{
	if (atcSid.size() < 2) return { "", "" };
	atcSid = vsid::utils::trimSV(atcSid);

	std::size_t pos = 0;
	std::pair<std::string_view, std::string_view> fb = { atcSid, "" };

	while ((pos = atcSid.find_first_of("xX", pos)) != std::string_view::npos)
	{
		if (pos == 0 || pos >= atcSid.size() - 1) { ++pos; continue; }

		const std::string_view sid = atcSid.substr(0, pos);
		const std::string_view trans = atcSid.substr(pos + 1);

		if (sid.size() >= 3 && trans.size() >= 3 && vsid::utils::containsDigit(sid))
		{
			if (trans.front() != 'x' && trans.front() != 'X') return { sid, trans };
			if (fb.second.empty()) fb = { sid, trans }; // save trans starting with x as fallback
		}

		++pos;
	}

	if (!fb.second.empty()) return fb;

	return { atcSid, "" };
}

bool vsid::fpln::findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&searchStr))
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();

	return std::string(fplnData.GetRemarks()).find(searchStr) != std::string::npos;
}

bool vsid::fpln::removeRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toRemove))
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::vector<std::string> remarks = vsid::utils::split(fplnData.GetRemarks(), ' ');

	for (std::vector<std::string>::iterator it = remarks.begin(); it != remarks.end();)
	{
		if (*it == toRemove)
		{
			it = remarks.erase(it);
		}
		else if (it != remarks.end()) ++it;
	}

	return fplnData.SetRemarks(vsid::utils::join(remarks).c_str());
}

bool vsid::fpln::addRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toAdd))
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::vector<std::string> remarks = vsid::utils::split(fplnData.GetRemarks(), ' ');
	remarks.push_back(toAdd);

	return fplnData.SetRemarks(vsid::utils::join(remarks).c_str());
}

bool vsid::fpln::FplnManager::restoreIC(const std::string& callsign) // #refacotr - change to std::string_view
{
	checkThread(__func__);

	EuroScopePlugIn::CFlightPlan FlightPlan = VSIDPlugin::instance().FlightPlanSelect(std::string(callsign).c_str());

	if (!FlightPlan.IsValid()) return false;

	if (auto it = processed_.find(callsign); it != processed_.end())
	{
		auto& processedFpln = it->second;
		auto [blockSid, blockRwy] = vsid::fpln::getAtcBlock(FlightPlan);
		EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
		EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
		EuroScopePlugIn::CController atcMyself = VSIDPlugin::instance().ControllerMyself();

		if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
			std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
		{
			blockSid = vsid::fpln::splitTransition(blockSid).first;
		}

		if (cad.GetClearedAltitude() == 0 &&
			blockSid != "" &&
			atcMyself.IsController() &&
			(blockSid == processedFpln.sid.name() ||
				blockSid == processedFpln.customSid.name())
			)
		{
			if (!processedFpln.customSid.empty())
			{
				int initialClimb = (processedFpln.customSid.initialClimb > fplnData.GetFinalAltitude()) ?
					fplnData.GetFinalAltitude() : processedFpln.customSid.initialClimb;

				if (!cad.SetClearedAltitude(initialClimb))
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SETALT_RESET))
					{
						vsid::Logger::log(vsid::LogLevel::Error, std::format("[{}] - failed to set altitude using custom SID. Code: {}",
							callsign, ERROR_FPLN_SETALT_RESET));

						messageHandler->addFplnError(callsign, ERROR_FPLN_SETALT_RESET);
					}
					return false;
				}
				else
				{
					messageHandler->removeFplnError(callsign, ERROR_FPLN_SETALT_RESET);
					return true;
				}
			}
			else if (!processedFpln.sid.empty())
			{
				int initialClimb = (processedFpln.sid.initialClimb > fplnData.GetFinalAltitude()) ?
					fplnData.GetFinalAltitude() : processedFpln.sid.initialClimb;

				if (!cad.SetClearedAltitude(initialClimb))
				{
					if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SETALT_RESET))
					{
						vsid::Logger::log(vsid::LogLevel::Error, std::format("[{}] - failed to set altitude using SID. Code: {}",
							callsign, ERROR_FPLN_SETALT_RESET));

						messageHandler->addFplnError(callsign, ERROR_FPLN_SETALT_RESET);
					}
					return false;
				}
				else
				{
					messageHandler->removeFplnError(callsign, ERROR_FPLN_SETALT_RESET);
					return true;
				}
			}
		}
	}

	return false; // default state
}

void vsid::fpln::FplnManager::clearSidData(const std::string_view callsign) // #refactor rename to resetSidData
{
	checkThread(__func__);

	if (auto it = processed_.find(callsign); it != processed_.end())
	{
		vsid::Logger::log(
			LogLevel::Debug,
			DebugLevel::Sid,
			false,
			"[{}] clearing SID data. SID [{}] | custom SID [{}]",
			callsign, it->second.sid.idName(), it->second.customSid.idName()
		);

		it->second.sid = {};
		it->second.customSid = {};
		it->second.transition = "";
		it->second.sidWpt = "";
		it->second.validEquip = true;
		it->second.sidProcessed = false;
	}
}

bool vsid::fpln::findScratchPad(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toSearch)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();

	return std::string(cad.GetScratchPadString()).find(vsid::utils::toupper(toSearch)); // #refactor - string_view
}

std::string vsid::fpln::getEquip(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::set<std::string>& rnav)
{

	std::string equip = FlightPlan.GetFlightPlanData().GetAircraftInfo();
	std::string callsign = FlightPlan.GetCallsign();
	char cap = FlightPlan.GetFlightPlanData().GetCapibilities();
	std::vector<std::string> vecEquip = {};

	if (equip.find("-") != std::string::npos)
	{
		vecEquip = vsid::utils::split(equip, '-');
	}
	else vecEquip.clear(); // equipment not present

	/* default state - if an aircraft is known rnav capable skip checks */
	if (rnav.contains(FlightPlan.GetFlightPlanData().GetAircraftFPType()))
	{
		vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] found in RNAV list. Returning [SDE2E3FGIJ1RWY]", callsign), vsid::DebugLevel::Sid);
		return "SDE2E3FGIJ1RWY";
	}
	else if (vecEquip.size() >= 2)
	{
		vecEquip = vsid::utils::split(vecEquip.at(1), '/');

		try
		{
			return vecEquip.at(0);
		}
		catch (std::out_of_range)
		{
			vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] failed to get equipment, Nothing will be returned.", callsign), vsid::DebugLevel::Sid);
			return "";
		}
	}
	else if (cap != ' ')
	{
		vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] failed to get equipment, falling back to capability checking. "
			"Reported equipment [{}]. Reported capability [{}]",
			callsign, equip, cap), vsid::DebugLevel::Sid);

		std::map<char, std::string> faaToIcao = {
			// X disabled due to too many occurences with fplns that are RNAV capable
			//{'X', "SF"},
			// T and U also disabled and will default to L
			// {'T', "SF"}, {'U', "SF"},
			{'D', "SDF"}, {'B', "SDF"}, {'A', "SDF"},
			{'M', "DFILTUV"}, {'N', "DFILTUV"}, {'P', "DFILTUV"},
			{'Y', "SDFIRY"}, {'C', "SDFIRY"}, {'I', "SDFIRY"},
			{'V', "SDFGRY"}, {'S', "SDFGRY"}, {'G', "SDFGRY"},
			{'W', "SDFWY"},
			{'Z', "SDE2E3FIJ1RWY"},
			{'L', "SDE2E3FGIJ1RWY"}
		};

		if (faaToIcao.contains(cap)) return faaToIcao[cap];
		else return faaToIcao['L'];
	}
	else
	{
		vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] failed to get equipment or capabilities. Returning empty equipment", callsign), vsid::DebugLevel::Sid);
		return "";
	}
}

std::string vsid::fpln::getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (vsid::fpln::findRemarks(FlightPlan, "PBN/"))
	{
		std::string pbn;
		std::vector<std::string> vecPbn = vsid::utils::split(FlightPlan.GetFlightPlanData().GetRemarks(), ' ');

		for (std::string& rem : vecPbn)
		{
			if (rem.find("PBN/") != std::string::npos)
			{
				try
				{
					return vsid::utils::split(rem, '/').at(1);
				}
				catch (std::out_of_range)
				{
					return "";
				}
			}
		}
	}
	return "";
}

std::string vsid::fpln::findSidWpt(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	std::string filedSid = FlightPlan.GetFlightPlanData().GetSidName();
	std::vector<std::string> filedRoute = vsid::utils::split(FlightPlan.GetFlightPlanData().GetRoute(), ' ');

	if (filedRoute.empty()) return "";
	if (!AirportManager::isActive(adep)) return "";

	const auto aptData = AirportManager::getData(adep);

	if(aptData == nullptr) return "";

	if (filedSid != "")
	{
		std::string esWpt = filedSid.substr(0, filedSid.length() - 2);
		for (const vsid::Sid& sid : aptData->sids)
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
							vsid::Logger::log(LogLevel::Error, std::format("[{}] Failed to split waypoint during SID waypoint checking. Waypoint [{}]. Code: {}",
								callsign, wpt, ERROR_FPLN_SIDWPT));

							messageHandler->addFplnError(callsign, ERROR_FPLN_SIDWPT);
						}
						return false;
					}
				})) return esWpt;
		}
	}

	// continue checks if esWpt hasn't been returned

	std::set<std::string> sidWpts = {};

	for (const vsid::Sid& sid : aptData->sids)
	{
		if (sid.waypoint != "XXX") sidWpts.insert(sid.waypoint);

		for (auto& [base, _] : sid.transition)
		{
			sidWpts.insert(base);
		}
	}

	for (std::string& wpt : filedRoute)
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
					vsid::Logger::log(LogLevel::Error, std::format("[{}] Failed to get the waypoint of a waypoint and speed/level group. Waypoint [{}]. Code: {}",
						callsign, wpt, ERROR_FPLN_SIDWPT));

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

void vsid::fpln::FplnManager::processFlightplan(EuroScopePlugIn::CFlightPlan& FlightPlan, bool checkOnly, std::string atcRwy, vsid::Sid manualSid)
{
	if (!FlightPlan.IsValid()) return;

	std::string callsign = FlightPlan.GetCallsign();

	if (!FplnManager::contains(callsign))
	{
		vsid::Logger::log(
			LogLevel::Debug,
			std::format("[{}] trying to process an unknown flight plan. Aborting.", callsign),
			DebugLevel::Fpln
		);

		return;
	}

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string adep = fplnData.GetOrigin();
	std::string filedSidWpt = vsid::fpln::findSidWpt(FlightPlan);
	std::vector<std::string> filedRoute = vsid::fpln::clean(FlightPlan, filedSidWpt);
	vsid::Sid sidSuggestion = {};
	vsid::Sid sidCustomSuggestion = {};
	std::string setRwy = "";
	vsid::fpln::FplnData fpln = {};
	bool resetIC = false;

	const auto adepData = AirportManager::getData(adep);

	if (adepData == nullptr)
	{
		if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_ADEPINACTIVE))
		{
			vsid::Logger::log(LogLevel::Warning, std::format("[{}] ADEP [{}] is not an active airport. Aborting processing. Code: {}",
				callsign, adep, ERROR_FPLN_ADEPINACTIVE));

			messageHandler->addFplnError(callsign, ERROR_FPLN_ADEPINACTIVE);
		}

		return;
	}
	else messageHandler->removeFplnError(callsign, ERROR_FPLN_ADEPINACTIVE);

	if (auto it = processed_.find(callsign); it != processed_.end())
	{
		if (it->second.removalTime.has_value())
		{
			auto& fpln = it->second;
			resetIC = true;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] re-syncing req and states for reconnected flight plan.", callsign), DebugLevel::Fpln);

			// sync requests - sync function requires fpln to be known in one of the lists

			if (!fpln.request.empty() && fpln.reqTime != -1)
			{
				std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(fpln.reqTime);

				vsid::Logger::log(LogLevel::Debug, std::format("[{}] syncing [{}] with scratch. New [{}] | Old [{}]", callsign,
					fpln.request, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString()), DebugLevel::Req);

				SyncManager::add(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
			}

			SyncManager::syncStates(FlightPlan);

			std::string ades = fplnData.GetDestination();

			if (AirportManager::isActive(ades) && fpln.ctl)
			{
				std::string newScratch = ".VSID_CTL_TRUE";
				SyncManager::add(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
			}
		}
		else
		{
			fpln = it->second;

			fpln.sid = {};
			fpln.customSid = {};
			fpln.sidWpt = "";
			fpln.transition = "";
			fpln.validEquip = true;
		}
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
		sidSuggestion = VSIDPlugin::instance().processSid(FlightPlan);
		sidCustomSuggestion = manualSid;
	}
	/* if a rwy is given by atc check for a sid for this rwy and for a normal sid
	* to be then able to compare those two
	*/
	else if (atcRwy != "")
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] processing SID without atcRWY (atcRWY present, will be next check)", callsign), DebugLevel::Sid);
		sidSuggestion = VSIDPlugin::instance().processSid(FlightPlan);

		vsid::Logger::log(LogLevel::Debug, std::format("[{}] processing SID with atcRWY (for customSuggestion) [{}]", callsign, atcRwy), DebugLevel::Sid);
		sidCustomSuggestion = VSIDPlugin::instance().processSid(FlightPlan, atcRwy);
	}
	/* default state */
	else
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] processing SID without atcRWY", callsign), DebugLevel::Sid);
		sidSuggestion = VSIDPlugin::instance().processSid(FlightPlan);
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

	vsid::Logger::log(
		LogLevel::Debug,
		DebugLevel::Sid,
		false,
		"[{}] comparing suggestion [{}] (rwys: {}) with custom suggestion [{}] (rwys: {}) for atcRwy [{}]. Equal: {}",
		callsign,
		sidSuggestion.idName(),
		vsid::utils::join(sidSuggestion.rwys),
		sidCustomSuggestion.idName(),
		vsid::utils::join(sidCustomSuggestion.rwys),
		atcRwy,
		sidSuggestion == sidCustomSuggestion
	);

	// clear the custom suggestion whenever it resolves to the same SID as the automatic suggestion,
	// regardless of how it was derived (manual selection, ATC-assigned rwy, ATC-assigned SID)
	if (!sidCustomSuggestion.empty() && sidSuggestion == sidCustomSuggestion &&
		(atcRwy.empty() || vsid::utils::contains(sidSuggestion.rwys, atcRwy)))
	{
		sidCustomSuggestion = {};
	}

	// if a custom sid has already been detected but evaluation fails (e.g. old airac entry)
	// preserve data previously pulled from the flight plan (suggestion value for other checks below)

	if (auto it = processed_.find(callsign); it != processed_.end())
	{
		auto& fpln = it->second;

		if (fpln.customSid.empty() && sidSuggestion.empty() && sidCustomSuggestion.empty() &&
			std::string(fplnData.GetRoute()).find(fpln.customSid.name()) != std::string::npos)
		{
			sidCustomSuggestion = fpln.customSid;
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

				if (auto it = adepData->areas.find(area); !area.empty() && it != adepData->areas.end() &&
					it->second.isActive && it->second.inside(FlightPlan.GetFPTrackPosition().GetPosition()))
				{
					arrAsDep = it->second.arrAsDep;
				}

				for (const std::string& sidRwy : sidSuggestion.rwys)
				{
					if (adepData->isDepRwy(sidRwy, arrAsDep))
					{
						rwy = sidRwy;
						break;
					}
				}
			}

			if (rwy != "") setRwy = rwy;
			else
			{
				vsid::Logger::log(LogLevel::Debug, std::format("Fall back to ES dep rwy for [{}] in fpln processing for sidSuggestion", callsign), DebugLevel::Sid);

				setRwy = fplnData.GetDepartureRwy();
			}
		}
		catch (std::out_of_range) // old remains - might be removed #checkforremoval
		{
			vsid::Logger::log(LogLevel::Error, std::format("[{}] Failed to check RWY in sidSuggestion. Check config [{}] for SID [{}]. RWY value is [{}]", callsign,
				adep, sidSuggestion.idName(), vsid::utils::join(sidSuggestion.rwys)));
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

				if (auto it = adepData->areas.find(area); !area.empty() && it != adepData->areas.end() &&
					it->second.isActive && it->second.inside(FlightPlan.GetFPTrackPosition().GetPosition()))
				{
					arrAsDep = it->second.arrAsDep;
				}

				for (const std::string& sidRwy : sidCustomSuggestion.rwys)
				{
					if (adepData->isDepRwy(sidRwy, arrAsDep))
					{
						rwy = sidRwy;
						break;
					}
				}
			}

			if (rwy != "") setRwy = rwy;
			else
			{
				vsid::Logger::log(LogLevel::Debug, std::format("Fall back to ES dep rwy for [{}] in fpln processing for sidCustomSuggestion", callsign), DebugLevel::Sid);

				setRwy = fplnData.GetDepartureRwy();
			}
		}
		catch (std::out_of_range) // old remains - might be removed #checkforremoval
		{
			vsid::Logger::log(LogLevel::Error, std::format("[{}] Failed to check RWY in sidCustomSuggestion. Check config [{}] for SID [{}]. RWY value is [{}]", callsign,
				adep, sidCustomSuggestion.idName(), vsid::utils::join(sidCustomSuggestion.rwys)));
		}
	}

	// building a new route with the selected sid

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		std::ostringstream ss;
		ss << sidSuggestion.name();
		if (std::string transition = vsid::fpln::getTransition(FlightPlan, sidSuggestion.transition, filedSidWpt); transition != "")
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
		if (std::string transition = vsid::fpln::getTransition(FlightPlan, sidCustomSuggestion.transition, filedSidWpt); transition != "")
		{
			ss << "x" << transition;
		}
		ss << "/" << setRwy;
		filedRoute.insert(filedRoute.begin(), vsid::utils::trim(ss.str()));
	}

	if (sidSuggestion.base != "" && sidCustomSuggestion.base == "")
	{
		fpln.sid = sidSuggestion;
		fpln.transition = vsid::fpln::getTransition(FlightPlan, sidSuggestion.transition, filedSidWpt);
	}
	else if (sidCustomSuggestion.base != "")
	{
		fpln.sid = sidSuggestion;
		fpln.customSid = sidCustomSuggestion;
		fpln.transition = vsid::fpln::getTransition(FlightPlan, sidCustomSuggestion.transition, filedSidWpt);
	}

	fpln.sidProcessed = true;

	FplnManager::update(callsign, [&fpln](vsid::fpln::FplnData& data) // overwrite procesed fpln with new data
		{
			data = std::move(fpln);
		});

	// if an IFR fpln has no matching sid but the route should be set inverse - otherwise rwy changes would be overwritten
	if (!checkOnly && std::string(fplnData.GetPlanType()) == "I" &&
		sidSuggestion.empty() && sidCustomSuggestion.empty()) checkOnly = true;

	if (auto it = processed_.find(callsign); it != processed_.end() && !checkOnly)
	{
		auto& processedFpln = it->second;;

		// only touch the route if it changes

		bool routeAmended = true;

		if (vsid::utils::split(fplnData.GetRoute(), ' ') == filedRoute)
		{
			vsid::Logger::log(
				LogLevel::Debug,
				std::format("[{}] route already matches. Skipping route update.", callsign),
				DebugLevel::Sid
			);
		}
		else
		{
			if (!fplnData.SetRoute(vsid::utils::join(filedRoute).c_str()))
			{
				vsid::Logger::log(
					LogLevel::Error,
					std::format("[{}] - Failed to change flight plan! Code: {}", callsign, ERROR_FPLN_SETROUTE)
				);
			}

			if (!fplnData.AmendFlightPlan())
			{
				vsid::Logger::log(
					LogLevel::Error,
					std::format("[{}] - Failed to amend flight plan! Code: {}", callsign, ERROR_FPLN_AMEND)
				);

				routeAmended = false;
			}
		}

		if (routeAmended)
		{
			if (adepData->settings.at("auto"))
			{
				std::string newScratch = ".vsid_auto_" + std::string(VSIDPlugin::instance().ControllerMyself().GetCallsign());

				SyncManager::add(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());
			}

			FplnManager::update(callsign, [](vsid::fpln::FplnData& data)
				{
					data.atcRWY = true;
				});
		}

		if (sidSuggestion.base != "" && sidCustomSuggestion.base == "" && sidSuggestion.initialClimb) // #evaluate - custom kept if same as std - might remove one branch
		{
			int initialClimb = (sidSuggestion.initialClimb > fplnData.GetFinalAltitude()) ? fplnData.GetFinalAltitude() : sidSuggestion.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				vsid::Logger::log(LogLevel::Error, std::format("[{}] - failed to set altitude. Code: {}", callsign, ERROR_FPLN_SETALT));
			}
		}
		else if (sidCustomSuggestion.base != "" && sidCustomSuggestion.initialClimb)
		{
			int initialClimb = (sidCustomSuggestion.initialClimb > fplnData.GetFinalAltitude()) ? fplnData.GetFinalAltitude() : sidCustomSuggestion.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				vsid::Logger::log(LogLevel::Error, std::format("[{}] - failed to set altitude. Code: {}", callsign, ERROR_FPLN_SETALT));
			}
		}

		std::string squawk = FlightPlan.GetControllerAssignedData().GetSquawk();
		if (squawk == "" || squawk == "0000" || squawk == "1234") VSIDPlugin::instance().addOrSetSquawk(callsign);
	}

	// reset IC if it doesn't match

	if (resetIC && processed_.contains(callsign)) restoreIC(callsign);
}

void vsid::fpln::FplnManager::reprocessImpl(std::string_view callsign, const FplnData& fplnData)
{
	checkThread(__func__);

	auto& instance = VSIDPlugin::instance();

	EuroScopePlugIn::CFlightPlan FlightPlan = instance.FlightPlanSelect(std::string(callsign).c_str());

	if (!FlightPlan.IsValid())
	{
		vsid::Logger::log(
			LogLevel::Debug,
			DebugLevel::Fpln,
			false,
			"[{}] could not reprocess. Flight plan not valid.",
			callsign
		);

		return;
	}

	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	auto adepData = AirportManager::getData(adep);

	if (adepData == nullptr) return;

	bool checkOnly = true;
	auto [sid, rwy] = vsid::fpln::getAtcBlock(FlightPlan);

	if (adepData->settings.at("auto") && !FlightPlan.GetClearenceFlag()) checkOnly = false;

	processFlightplan(
		FlightPlan,
		checkOnly,
		rwy,
		((sid == fplnData.customSid.name()) ? fplnData.customSid : vsid::Sid{})
	);
}

void vsid::fpln::FplnManager::reprocess(std::string_view callsign)
{
	if (auto it = processed_.find(callsign); it != processed_.end())
	{
		reprocessImpl(callsign, it->second);
	}
	else
	{
		vsid::Logger::log(
			LogLevel::Debug,
			DebugLevel::Fpln,
			false,
			"[{}] could not reprocess. Not found in processed.",
			callsign
		);
	}
}

void vsid::fpln::FplnManager::reprocessAll()
{
	for (const auto& [callsign, fplnData] : processed_)
	{
		reprocessImpl(callsign, fplnData);
	}
}