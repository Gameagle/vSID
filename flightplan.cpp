#include "pch.h"

#include "flightplan.h"
#include "utils.h"
#include "messageHandler.h"
#include "constants.h"

std::vector<std::string> vsid::fplnhelper::clean(const EuroScopePlugIn::CFlightPlan &FlightPlan, std::string filedSidWpt)
{
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string origin = fplnData.GetOrigin();
	std::vector<std::string> filedRoute = vsid::utils::split(fplnData.GetRoute(), ' ');
	std::pair<std::string, std::string> atcBlock = getAtcBlock(FlightPlan);

	if (filedRoute.size() > 0)
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
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Error during cleaning of route at first entry. ADEP: " + origin +
				" with route \"" + vsid::utils::join(filedRoute) + "\". Code: " + ERROR_FPLN_CLNFIRST);
		}
	}

	 /* stop cleaning if flightplan is VFR */

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
				messageHandler->writeMessage("ERROR", "[" + callsign + "] Error during cleaning of route. Cleaning was continued after false entry. ADEP: " + origin +
											" with route \"" + vsid::utils::join(filedRoute) + "\". Code: " + ERROR_FPLN_CLNSPDLVL);
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
				messageHandler->writeMessage("ERROR", "[" + callsign + "] Error during cleaning of route. Cleaning was continued after false entry. ADEP: " + origin +
					" with route \"" + vsid::utils::join(filedRoute) + "\". Code: " + ERROR_FPLN_CLNSPDLVL);
			}
			if (*it != origin) break;
			it = filedRoute.erase(it);
		}
	}

	if (filedRoute.size() == 0 && vsid::utils::split(fplnData.GetRoute(), ' ').size() != 0)
	{

		messageHandler->writeMessage("WARNING", "[" + callsign +
									"] did not clean route as cleaning resulted in an empty route (possible error in the filed route). Returning original route.");
		return vsid::utils::split(fplnData.GetRoute(), ' ');
	}

	return filedRoute;
}

// #evaluate - do we still need filedSidWpt or should we used the stored value?

std::string vsid::fplnhelper::getTransition(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::map<std::string, vsid::Transition>& transition,
	const std::string& filedSidWpt)
{
	if (transition.empty()) return "";

	std::vector<std::string> route = vsid::fplnhelper::clean(FlightPlan, filedSidWpt);

	for (auto& [base, trans] : transition)
	{
		if (std::find(route.begin(), route.end(), base) == route.end()) continue;
		return trans.base + trans.number + trans.designator;
	}
	messageHandler->writeMessage("DEBUG", "[" + std::string(FlightPlan.GetCallsign()) +
		"] no matching transition wpt found", vsid::MessageHandler::DebugArea::Sid);
	return ""; // fallback if no transition could be matched
}

std::pair<std::string, std::string> vsid::fplnhelper::splitTransition(std::string atcSid) // #refactor to string_view
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

	return { atcSid, ""};
}

std::pair<std::string, std::string> vsid::fplnhelper::getAtcBlock(const EuroScopePlugIn::CFlightPlan &FlightPlan)
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
				messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to get ATC block. First route entry: " +
					filedRoute.at(0) + ". Code: " + ERROR_FPLN_ATCBLOCK);
				messageHandler->addFplnError(callsign, ERROR_FPLN_ATCBLOCK);
			}
		}
	}
	return { atcSid, atcRwy };
}

bool vsid::fplnhelper::findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& searchStr))
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();

	return std::string(fplnData.GetRemarks()).find(searchStr) != std::string::npos;
}

bool vsid::fplnhelper::removeRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toRemove))
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

bool vsid::fplnhelper::addRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toAdd))
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();
	std::vector<std::string> remarks = vsid::utils::split(fplnData.GetRemarks(), ' ');
	remarks.push_back(toAdd);

	return fplnData.SetRemarks(vsid::utils::join(remarks).c_str());
}

bool vsid::fplnhelper::findScratchPad(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toSearch)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();

	return std::string(cad.GetScratchPadString()).find(vsid::utils::toupper(toSearch)); // #refactor - string_view
}

std::string vsid::fplnhelper::getEquip(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::set<std::string>& rnav)
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
		messageHandler->writeMessage("DEBUG", "[" + callsign +
			"] found in RNAV list. Returning \"SDE2E3FGIJ1RWY\"", vsid::MessageHandler::DebugArea::Sid);

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
			messageHandler->writeMessage("DEBUG", "[" + callsign +
				"] failed to get equipment, Nothing will be returned.", vsid::MessageHandler::DebugArea::Sid);

			return "";
		}
	}
	else if (cap != ' ')
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign +
									"] failed to get equipment, falling back to capability checking. Reported equipment \"" +
									equip + "\". Reported capability \"" + cap + "\"", vsid::MessageHandler::DebugArea::Sid);

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
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] failed to get equipment or capabilities. Returning empty equipment",
									vsid::MessageHandler::DebugArea::Sid);
		return "";
	}
}

std::string vsid::fplnhelper::getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (vsid::fplnhelper::findRemarks(FlightPlan, "PBN/"))
	{
		std::string pbn;
		std::vector<std::string> vecPbn = vsid::utils::split(FlightPlan.GetFlightPlanData().GetRemarks(), ' ');

		for (std::string &rem : vecPbn)
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

void vsid::fplnhelper::saveFplnInfo(const std::string& callsign, vsid::Fpln fplnInfo, std::map<std::string, vsid::Fpln>& savedFplnInfo)
{
	messageHandler->writeMessage("DEBUG", "[" + callsign + "] saving flight plan info for possible reconnection.", vsid::MessageHandler::DebugArea::Fpln);
	savedFplnInfo[callsign] = std::move(fplnInfo);

	savedFplnInfo[callsign].sid = {};
	savedFplnInfo[callsign].customSid = {};
	savedFplnInfo[callsign].sidWpt = "";
	savedFplnInfo[callsign].transition = "";
	savedFplnInfo[callsign].validEquip = true;
}

bool vsid::fplnhelper::restoreFplnInfo(const std::string& callsign, std::map<std::string, vsid::Fpln>& processed, std::map<std::string, vsid::Fpln>& savedFplnInfo)
{
	if (!savedFplnInfo.contains(callsign)) return false;

	processed[callsign] = std::move(savedFplnInfo[callsign]);

	savedFplnInfo.erase(callsign);

	if (processed.contains(callsign))
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] successfully  restored.", vsid::MessageHandler::DebugArea::Fpln);
		return true;
	}
	else
	{
		messageHandler->writeMessage("DEBUG", "[" + callsign + "] failed to restore from saved info.", vsid::MessageHandler::DebugArea::Fpln); 
		return false;
	}
}

bool vsid::fplnhelper::restoreIC(const vsid::Fpln& fplnInfo, EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CController atcMyself)
{
	if (!FlightPlan.IsValid()) return false;

	auto [blockSid, blockRwy] = vsid::fplnhelper::getAtcBlock(FlightPlan);
	std::string callsign = FlightPlan.GetCallsign();
	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	EuroScopePlugIn::CFlightPlanData fplnData = FlightPlan.GetFlightPlanData();

	if (std::find(blockSid.begin(), blockSid.end(), 'x') != blockSid.end() || // #refactor - remove checks for xX
		std::find(blockSid.begin(), blockSid.end(), 'X') != blockSid.end())
	{
		blockSid = vsid::fplnhelper::splitTransition(blockSid).first;
	}

	if (cad.GetClearedAltitude() == 0 &&
		blockSid != "" &&
		atcMyself.IsController() &&
		(blockSid == fplnInfo.sid.name() ||
			blockSid == fplnInfo.customSid.name())
		)
	{
		if (!fplnInfo.customSid.empty())
		{
			int initialClimb = (fplnInfo.customSid.initialClimb > fplnData.GetFinalAltitude()) ?
				fplnData.GetFinalAltitude() : fplnInfo.customSid.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SETALT_RESET))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude. Code: " + ERROR_FPLN_SETALT_RESET);
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
		else if (!fplnInfo.sid.empty())
		{
			int initialClimb = (fplnInfo.sid.initialClimb > fplnData.GetFinalAltitude()) ?
				fplnData.GetFinalAltitude() : fplnInfo.sid.initialClimb;
			if (!cad.SetClearedAltitude(initialClimb))
			{
				if (!messageHandler->getFplnErrors(callsign).contains(ERROR_FPLN_SETALT_RESET))
				{
					messageHandler->writeMessage("ERROR", "[" + callsign + "] - failed to set altitude. Code: " + ERROR_FPLN_SETALT_RESET);
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
	return false;
}
