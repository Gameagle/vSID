#include "pch.h"

#include "flightplan.h"
#include "utils.h"
#include "messageHandler.h"

std::vector<std::string> vsid::fpln::clean(const EuroScopePlugIn::CFlightPlan &FlightPlan, std::string filedSidWpt)
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
				" with route \"" + vsid::utils::join(filedRoute) + "\". #rcfe");
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
											" with route \"" + vsid::utils::join(filedRoute) + "\". #rcer-1");
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
					" with route \"" + vsid::utils::join(filedRoute) + "\". #rcer-2");
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
	messageHandler->writeMessage("DEBUG", "[" + std::string(FlightPlan.GetCallsign()) +
		"] no matching transition wpt found", vsid::MessageHandler::DebugArea::Sid);
	return ""; // fallback if no transition could be matched
}

std::pair<std::string, std::string> vsid::fpln::getAtcBlock(const EuroScopePlugIn::CFlightPlan &FlightPlan)
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
		}
		catch (std::out_of_range)
		{
			messageHandler->writeMessage("ERROR", "[" + callsign + "] Failed to get ATC block. First route entry: " + filedRoute.at(0) + ". #fpgb");
		}
	}
	return { atcSid, atcRwy };
}

bool vsid::fpln::findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(& searchStr))
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

bool vsid::fpln::findScratchPad(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toSearch)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();

	return std::string(cad.GetScratchPadString()).find(vsid::utils::toupper(toSearch));
}

bool vsid::fpln::setScratchPad(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toAdd)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string scratch = cad.GetScratchPadString();
	scratch += toAdd;

	messageHandler->writeMessage("DEBUG", "[" + std::string(FlightPlan.GetCallsign()) + "] Setting scratch: " + scratch, vsid::MessageHandler::DebugArea::Req);

	return cad.SetScratchPadString(vsid::utils::trim(scratch).c_str());
}

bool vsid::fpln::removeScratchPad(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toRemove)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string callsign = FlightPlan.GetCallsign();
	std::string scratch = cad.GetScratchPadString();
	size_t pos = vsid::utils::toupper(scratch).find(vsid::utils::toupper(toRemove));

	if (pos != std::string::npos)
	{
		vsid::utils::trim(scratch.erase(pos, toRemove.length()));

		messageHandler->writeMessage("DEBUG", "[" + callsign + "] Removing scratchpad entry \"" + toRemove +
			"\". New scratch : \"" + scratch + "\"", vsid::MessageHandler::DebugArea::Req);

		return cad.SetScratchPadString(vsid::utils::trim(scratch).c_str());
	}
	return false; // default / fall back state
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

std::string vsid::fpln::getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (vsid::fpln::findRemarks(FlightPlan, "PBN/"))
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
