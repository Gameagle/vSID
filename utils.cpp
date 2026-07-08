#include "pch.h"
#include "utils.h"
#include "logger.h"

#include <sstream>
#include <algorithm>
#include <format>

std::vector<std::string> vsid::utils::split(std::string_view sv, const char del, const bool keepEmpty)
{
	std::vector<std::string> elems;

	size_t start = 0;
	size_t end = sv.find(del);

	while (end != std::string_view::npos)
	{
		auto next = vsid::utils::trim(sv.substr(start, end - start));

		if (keepEmpty || !next.empty())
			elems.emplace_back(std::move(next));

		start = end + 1;
		end = sv.find(del, start);
	}

	auto last = vsid::utils::trim(sv.substr(start));

	if (keepEmpty || !last.empty())
		elems.emplace_back(std::move(last));

	return elems;
}

std::vector<std::string_view> vsid::utils::splitSV(std::string_view sv, const char del, const bool keepEmpty)
{
	std::vector<std::string_view> elems;

	size_t start = 0;
	size_t end = sv.find(del);

	while (end != std::string_view::npos)
	{
		auto next = vsid::utils::trimSV(sv.substr(start, end - start));

		if (keepEmpty || !next.empty())
			elems.emplace_back(next);

		start = end + 1;
		end = sv.find(del, start);
	}

	auto last = vsid::utils::trimSV(sv.substr(start));

	if (keepEmpty || !last.empty())
		elems.emplace_back(last);

	return elems;
}

bool vsid::utils::containsDigit(int number, int digit)
{
	if (!number)
	{
		return true;
	}
	while (number)
	{
		int currDigit = number % 10;
		if (currDigit == digit)
		{
			return true;
		}
		number /= 10;
	}
	return false;
}

int vsid::utils::getMinClimb(int elevation)
{
	return int (std::ceil((float)elevation / 1000) * 1000) + 500;
}

std::string vsid::utils::tolower(std::string input)
{
	std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return std::tolower(c); });
	return input;
}

std::string vsid::utils::toupper(std::string input)
{
	std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return std::toupper(c); });
	return input;
}

EuroScopePlugIn::CPosition vsid::utils::toPoint(const std::pair<std::string, std::string>& pos)
{
	EuroScopePlugIn::CPosition p;
	p.m_Latitude = toDeg(pos.first);
	p.m_Longitude = toDeg(pos.second);

	return p;
}

double vsid::utils::toDeg(const std::string& coord)
{
	if (coord.empty())
	{
		vsid::Logger::log(vsid::LogLevel::Warning, "Empty coordinate string found! Skipping coordinate.");
		return 0.0;
	}

	std::vector<std::string> dms = vsid::utils::split(coord, '.');
	int multi = 0; // default state in exception case

	try
	{
		if (dms.size() < 4)
			throw std::out_of_range(std::format("Coordinate string [{}] does not contain enough parts for DMS conversion!", coord));

		multi = (dms.at(0).find('S') != std::string::npos || dms.at(0).find('W') != std::string::npos) ? -1 : 1;

		double deg = std::stod(dms[0].substr(1, dms[0].length()));
		double min = std::stod(dms[1]) / 60;
		double sec = (std::stod(dms[2]) + std::stod("0." + dms[3])) / 3600;

		return (deg + min + sec) * multi;
	}
	catch (std::out_of_range& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Out of bounds while calculating coordinate [{}]. {}", coord, e.what()));
		return 0.0;
	}
	catch (const std::invalid_argument& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Invalid number format in coord [{}]. {}", coord, e.what()));
		return 0.0;
	}

	vsid::Logger::log(vsid::LogLevel::Warning, std::format("Fallback state for [{}]! Failed to calculate. DMS will be set to 0.0", coord));

	return 0.0;
}
