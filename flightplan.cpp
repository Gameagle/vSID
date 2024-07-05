#include "pch.h"

#include "flightplan.h"
#include "utils.h"
#include "messageHandler.h"

void vsid::fpln::clean(std::vector<std::string> &filedRoute, const std::string origin, std::string filedSidWpt)
{
	std::pair<std::string, std::string> atcBlock = getAtcBlock(filedRoute, origin);

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
			messageHandler->writeMessage("ERROR", "Error during cleaning of route at first entry. ADEP: " + origin +
				" with route \"" + vsid::utils::join(filedRoute) + "\". Callsign is unknown here. #rcfe");
		}
	}
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
				messageHandler->writeMessage("ERROR", "Error during cleaning of route. Cleaning was continued after false entry. ADEP: " + origin +
											" with route \"" + vsid::utils::join(filedRoute) + "\". Callsign is unknown here. #rcer");
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
				messageHandler->writeMessage("ERROR", "Error during cleaning of route. Cleaning was continued after false entry. ADEP: " + origin +
					" with route \"" + vsid::utils::join(filedRoute) + "\". Callsign is unknown here. #rcer");
			}
			if (*it != origin) break;
			it = filedRoute.erase(it);
		}
	}
}

std::pair<std::string, std::string> vsid::fpln::getAtcBlock(const std::vector<std::string>& filedRoute, const std::string origin)
{
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
			messageHandler->writeMessage("ERROR", "Failed to get ATC block. Callsign is unknown. #fpgb");
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

	messageHandler->writeMessage("DEBUG", "[REQ] Setting scratch : " + scratch, vsid::MessageHandler::DebugArea::Req);

	return cad.SetScratchPadString(vsid::utils::trim(scratch).c_str());
}

bool vsid::fpln::removeScratchPad(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toRemove)
{
	if (!FlightPlan.IsValid()) return false;

	EuroScopePlugIn::CFlightPlanControllerAssignedData cad = FlightPlan.GetControllerAssignedData();
	std::string scratch = cad.GetScratchPadString();
	size_t pos = scratch.find(vsid::utils::toupper(toRemove));

	if (pos != std::string::npos)
	{
		std::string newScratch = scratch.substr(0, pos);

		if (newScratch != scratch)
		{
			messageHandler->writeMessage("DEBUG", "[REQ] Removing request. New scratch : \"" + newScratch + "\"", vsid::MessageHandler::DebugArea::Req);

			return cad.SetScratchPadString(vsid::utils::trim(newScratch).c_str());
		}
	}
	return false; // default / fallback state
}
