#include "pch.h"

#include "flightplan.h"
#include "messageHandler.h"
#include "utils.h"

#include<sstream>

void vsid::fpln::clean(std::vector<std::string> &filedRoute, const std::string origin, std::string filedSidWpt)
{
	std::pair<std::string, std::string> atcBlock = getAtcBlock(filedRoute, origin);

	if (filedRoute.size() > 0)
	{
		if (filedRoute.front().find('/') != std::string::npos && filedRoute.front().find("/N") == std::string::npos)
		{
			filedRoute.erase(filedRoute.begin());
		}
	}
	/* if a possible SID block was found check the entire route for more SIDs (e.g. filed) and erase them as well*/
	if (filedRoute.size() > 0 && filedSidWpt != "")
	{
		for (std::vector<std::string>::iterator it = filedRoute.begin(); it != filedRoute.end();)
		{
			if ((it->find(filedSidWpt) != std::string::npos && *it != filedSidWpt) || *it == origin)
			{
				it = filedRoute.erase(it);
			}
			else ++it;
			//if (atcBlock.first != "")
			//{
			//	if (it->substr(0, it->length() - 2) == atcBlock.first.substr(0, atcBlock.first.length() - 2))
			//	{
			//		it = filedRoute.erase(it);
			//	}
			//	else
			//	{
			//		++it;
			//	}
			//}
			//else
			//{
			//	if (it->substr(0, it->length() - 2) == filedSidWpt)
			//	{
			//		it = filedRoute.erase(it);
			//	}
			//	else
			//	{
			//		++it;
			//	}
			//}
		}
	}
}

std::pair<std::string, std::string> vsid::fpln::getAtcBlock(const std::vector<std::string>& filedRoute, const std::string origin)
{
	std::string atcRwy = "";
	std::string atcSid = "";
	if (filedRoute.size() > 0)
	{
		if (filedRoute.front().find('/') != std::string::npos && filedRoute.front().find("/N") == std::string::npos)
		{
			std::vector<std::string> sidBlock = vsid::utils::split(filedRoute.at(0), '/');
			if (sidBlock.front().find_first_of("0123456789RV") != std::string::npos || sidBlock.front() == origin)
			{
				atcSid = sidBlock.front();
				atcRwy = sidBlock.back();
			}
		}
	}
	return { atcSid, atcRwy };
}

//bool vsid::fpln::findRemarks(const EuroScopePlugIn::CFlightPlanData& fplnData, const std::string(& searchStr))
//{
//	return std::string(fplnData.GetRemarks()).find(searchStr) != std::string::npos;
//}
//
//void vsid::fpln::removeRemark(EuroScopePlugIn::CFlightPlanData& fplnData, const std::string(&toRemove))
//{
//	messageHandler->writeMessage("DEBUG", "Remarks before: " + std::string(fplnData.GetRemarks()));
//	if (strlen(fplnData.GetRemarks()) == 0) return;
//	std::vector<std::string> remarks = vsid::utils::split(fplnData.GetRemarks(), ' ');
//	for (std::vector<std::string>::iterator it = remarks.begin(); it != remarks.end();)
//	{
//		if (*it == toRemove)
//		{
//			it = remarks.erase(it);
//		}
//		else if (it != remarks.end()) ++it;
//	}
//	messageHandler->writeMessage("DEBUG", "Remarks after: " + vsid::utils::join(remarks));
//	fplnData.SetRemarks(vsid::utils::join(remarks).c_str());
//}
//
//void vsid::fpln::addRemark(EuroScopePlugIn::CFlightPlanData& fplnData, const std::string(&toAdd))
//{
//	std::vector<std::string> remarks = vsid::utils::split(fplnData.GetRemarks(), ' ');
//	remarks.push_back(toAdd);
//	messageHandler->writeMessage("DEBUG", "New Remarks: " + vsid::utils::join(remarks));
//	fplnData.SetRemarks(vsid::utils::join(remarks).c_str());
//}
