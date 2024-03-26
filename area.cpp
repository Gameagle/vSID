#include "pch.h"

#include "area.h"
#include "messageHandler.h"
#include "utils.h"

#include <algorithm>

vsid::Area::Area(std::vector<std::pair<std::string, std::string>> &coords, bool isActive, bool arrAsDep)
{
	for (std::pair<std::string, std::string>& coord : coords)
	{
		this->points.push_back(toPoint(coord));
	}

	for (auto it = this->points.begin(); it != this->points.end();)
	{
		Point p1 = *it;
		Point p2;
		++it;
		if (it != this->points.end())
		{
			p2 = *it;
		}
		else
		{
			p2 = *this->points.begin();
		}
		this->lines.push_back({ p1, p2 });
	}
	this->isActive = isActive;
	this->arrAsDep = arrAsDep;
}

void vsid::Area::showline()
{
	for (auto& line : this->lines)
	{
		messageHandler->writeMessage("DEBUG", "line: " + std::to_string(line.first.lon) + ":" + std::to_string(line.first.lat) + " - " + std::to_string(line.second.lon) + "." + std::to_string(line.second.lat));
	}
}

vsid::Area::Point vsid::Area::toPoint(std::pair<std::string, std::string> &pos)
{
	double lat = toDeg(pos.first);
	double lon = toDeg(pos.second);

	return {lat, lon};
}

double vsid::Area::toDeg(std::string& coord)
{
	std::vector<std::string> dms = vsid::utils::split(coord, '.');

	int multi = (dms.front().find('S') != std::string::npos || dms.front().find('W') != std::string::npos) ? -1 : 1;

	double deg = std::stod(dms[0].substr(1, dms[0].length()));
	double min = std::stod(dms[1]) / 60;
	double sec = (std::stod(dms[2]) + std::stod("0." + dms[3])) / 3600;

	return (deg + min + sec) * multi;
}

bool vsid::Area::inside(const EuroScopePlugIn::CPosition& fplnPos)
{
	std::pair<Point, Point> l5 = { {fplnPos.m_Latitude, fplnPos.m_Longitude}, {fplnPos.m_Latitude + 0.05, fplnPos.m_Longitude + 0.05} };
	bool inside = false;

	for (auto &line : this->lines)
	{
		if (fplnPos.m_Latitude > std::min<double>(line.first.lat, line.second.lat))
		{
			if (fplnPos.m_Latitude <= std::max<double>(line.first.lat, line.second.lat))
			{
				if (fplnPos.m_Longitude <= std::max<double>(line.first.lon, line.second.lon))
				{
					double x_inter = (fplnPos.m_Latitude - line.first.lat) *
									(line.second.lon - line.first.lon) /
									(line.second.lat - line.first.lat) + line.first.lon;

					if (line.first.lon == line.second.lon || fplnPos.m_Longitude <= x_inter)
					{
						inside = !inside;
					}
				}
			}
		}
	}	
	return inside;
}
