#include "pch.h"

#include "eseparser.h"

void vsid::EseParser::parseEse(const std::filesystem::path& path)
{
	messageHandler->writeMessage("DEBUG", "Ese path to parse: \"" + path.string() + "\"", vsid::MessageHandler::DebugArea::Conf);

	std::ifstream in(path);
	if (!in)
	{
		messageHandler->writeMessage("ERROR", "Failed to open .ese file in path: " + path.string());
		return;
	}

	std::string l;

	while (std::getline(in, l))
	{
		if (isEmptyOrComment(l)) continue;

		if (isHeader(l)) // set new section and leave old one if active
		{
			if (currSection != Section::None) exit(currSection);

			currSection = toSection(l);
			enter(currSection);
			continue;
		}

		if (currSection != Section::None && !isEmptyOrComment(l)) // work through non-empty and non-comment lines
		{
			line(currSection, std::string_view(l));
			
		}
	}

	if (currSection != Section::None) exit(currSection);

	currSection = Section::None;
}

void vsid::EseParser::enter(vsid::EseParser::Section s)
{
	switch (s)
	{
	case Section::Positions:
		this->sectionAtc_.clear();

		break;

	case Section::SidsStars:
		this->sectionSids_.clear();
		break;

	case Section::Unknown:
	case Section::None:
		break;
	}
}

void vsid::EseParser::line(Section s, std::string_view l)
{
	switch (s)
	{
	case Section::Positions:
	{
		std::vector<std::string> atcVec = vsid::utils::split(vsid::utils::trim(std::string(l)), ':', true);
		std::vector<EuroScopePlugIn::CPosition> visPoints = {};

		if (atcVec.size() > 10)
		{
			for (size_t idx = 11; idx + 1 < std::min(atcVec.size(), static_cast<size_t>(20)); ++idx)
			{
				std::string& visLat = atcVec.at(idx);
				std::string& visLon = atcVec.at(idx + 1);

				if (visLat.size() < 2 || visLon.size() < 2)
				{
					++idx;
					continue;
				}

				if (((visLat.front() == 'N' || visLat.front() == 'S') && std::isdigit(static_cast<unsigned char>(visLat.back()))) &&
					((visLon.front() == 'E' || visLon.front() == 'W') && std::isdigit(static_cast<unsigned char>(visLon.back()))))
				{
					visPoints.push_back(vsid::utils::toPoint({ visLat, visLon }));
				}

				++idx;
			}
		}

		try
		{
			this->sectionAtc_.emplace(
				atcVec.at(0), // callsign
				atcVec.at(3), // si
				std::stod(atcVec.at(2)), // freq
				visPoints // additional vis points
			); 
		}
		catch (std::out_of_range& e)
		{
			messageHandler->writeMessage("ERROR", "Failed to parse ATC: " + std::string(e.what()));
		}

		break;
	}
	case Section::SidsStars:
	{
		std::vector<std::string> sidVec = vsid::utils::split(vsid::utils::trim(std::string(l)), ':');

		try
		{
			if (sidVec.empty() || sidVec.at(0) == "STAR") break; // only parse SIDs

			std::string currSid = vsid::utils::trim(sidVec.at(3));

			if (currSid.length() < 3) break; // protection for num / desig extraction

			const bool lastIsDigit = vsid::utils::lastIsDigit(currSid);

			this->sectionSids_.emplace(
				vsid::utils::trim(sidVec.at(1)), // apt
				currSid.substr(0, currSid.length() - (lastIsDigit ? 1 : 2)), // sid base
				currSid.at(currSid.length() - (lastIsDigit ? 1 : 2)), // sid number
				(lastIsDigit ? ' ' : currSid.back()), // sid designator
				vsid::utils::trim(sidVec.at(2)) // sid rwy
			);
		}
		catch (std::out_of_range& e)
		{
			messageHandler->writeMessage("ERROR", "Failed to parse SID: " + std::string(e.what()));
		}

		break;
	}
	case Section::Unknown:
	case Section::None:
		break;
	}
}

void vsid::EseParser::exit(Section s)
{
	switch (s)
	{
	case Section::Positions:
		messageHandler->writeMessage("INFO", "Parsed " + std::to_string(this->sectionAtc_.size()) + " atc stations.");
		break;
	case Section::SidsStars:
		messageHandler->writeMessage("INFO", "Parsed " + std::to_string(this->sectionSids_.size()) + " SIDs.");
		break;

	case Section::Unknown:
	case Section::None:
		break;
	}
}
