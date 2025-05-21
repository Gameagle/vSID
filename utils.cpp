#include "pch.h"
#include "utils.h"

#include <sstream>
#include <algorithm>

std::string vsid::utils::ltrim(const std::string& string)
{
	std::string string_to_trim = string;
	string_to_trim.erase(string.find_last_not_of(' ') + 1);
	return string_to_trim;
}

std::string vsid::utils::rtrim(const std::string& string)
{
	std::string string_to_trim = string;
	string_to_trim.erase(0, string.find_first_not_of(' '));
	return string_to_trim;
}

std::string vsid::utils::trim(const std::string& string)
{
	return vsid::utils::ltrim(vsid::utils::rtrim(string));
}

std::vector<std::string> vsid::utils::split(const std::string &string, const char &del, const bool keepWhitespace)
{
	std::istringstream ss(string);
	std::vector<std::string> elems;
	std::string elem;

	while (std::getline(ss, elem, del))
	{
		if (elem == "" && !keepWhitespace) continue; // remove excessive whitespaces to prevent a crash caused by wrong routes
		elems.push_back(vsid::utils::trim(elem));
	}
	return elems;
}

std::string vsid::utils::join(const std::vector<std::string>& toJoin, const char del)
{
	if (toJoin.empty()) return "";
	std::ostringstream ss;
	for (const auto& elem : toJoin) // possible improvement back to a copy function
	{
		ss << elem << del;
	}
	std::string joinedStr = ss.str();
	return joinedStr.erase(joinedStr.length() - 1, 1);
}

std::string vsid::utils::join(const std::set<std::string>& toJoin, char del)
{
	if (toJoin.empty()) return "";
	std::ostringstream ss;
	for (const auto& elem : toJoin) // possible improvement back to a copy function
	{
		ss << elem << del;
	}
	std::string joinedStr = ss.str();
	return joinedStr.erase(joinedStr.length() - 1, 1);
}

std::vector<std::string> vsid::utils::splitRoute(std::string& string)
{
	std::vector<std::string> elems;
	std::string elem;
	size_t pos = 0;

	string = vsid::utils::trim(string);

	while ((pos = string.find(' ')) != std::string::npos)
	{
		elem = string.substr(0, pos);
		if (elem.find('/') != std::string::npos)
		{
			elem = vsid::utils::split(elem, '/').at(0);
		}
		elems.push_back(elem);
		string.erase(0, pos + 1);
		vsid::utils::ltrim(string);
	}

	elems.push_back(string);

	return elems;
}

bool vsid::utils::isIcaoInVector(const std::vector<vsid::Airport>& airportVector, const std::string& toSearch)
{
	for (const vsid::Airport &elem : airportVector)
	{
		if (elem.icao == toSearch)
		{
			return true;
		}
	}
	return false;
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
