#include "pch.h"

#include "eseparser.h"
#include "flightplan.h"

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
			int facility = 0;

			if (atcVec.at(1).find("Information") != std::string::npos) facility = 1;
			else
			{
				std::optional<std::string> callsignFac = std::nullopt;

				if(std::vector<std::string> callsignSplit = vsid::utils::split(atcVec.at(0), '_'); callsignSplit.size() > 2) 
					callsignFac = callsignSplit.at(2);
				else if(callsignSplit.size() > 1)
					callsignFac = callsignSplit.at(1);
				
				
				if (callsignFac)
				{
					const std::string& fac = *callsignFac;

					if (fac == "DEL") facility = 2;
					else if (fac == "GND") facility = 3;
					else if (fac == "TWR") facility = 4;
					else if (fac == "APP" || fac == "DEP") facility = 5;
					else if (fac == "CTR") facility = 6;
				}
			}

			this->sectionAtc_.emplace(
				atcVec.at(0), // callsign
				atcVec.at(3), // si
				facility,
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

			auto [sid, trans] = vsid::fplnhelper::splitTransition(currSid);

			messageHandler->writeMessage("DEBUG", "[ESE] Parsing SID \"" + currSid + "\" - sid: " + sid + " / trans : " + trans, vsid::MessageHandler::DebugArea::Dev);

			//const bool lastIsDigit = vsid::utils::lastIsDigit(sid);

			vsid::SectionSID sectionSid("", "", '\0', std::nullopt, "");
			vsid::SectionTransition sectionTrans("", std::nullopt, std::nullopt);

			if (!sid.empty())
			{
				sectionSid.apt = vsid::utils::trim(sidVec.at(1));
				sectionSid.rwy = vsid::utils::trim(sidVec.at(2));

				if (vsid::utils::lastIsDigit(sid))
				{
					sectionSid.base = sid.substr(0, sid.length() - 1);
					sectionSid.number = sid.back();
				}
				else if (sid.length() > 2)
				{
					if (vsid::utils::lastIsDigit(sid.substr(0, sid.length() - 1)))
					{
						sectionSid.base = sid.substr(0, sid.length() - 2);
						sectionSid.number = sid[sid.length() - 2];
						sectionSid.desig = sid.back();
					}
					else sectionSid.base = sid;
				}
			}
			else break;

			if (!trans.empty())
			{
				if (vsid::utils::lastIsDigit(trans))
				{
					sectionTrans.base = trans.substr(0, trans.length() - 1);
					sectionTrans.number = trans.back();
				}
				else if(trans.length() > 2)
				{
					if (vsid::utils::lastIsDigit(trans.substr(0, trans.length() - 1)))
					{
						sectionTrans.base = trans.substr(0, trans.length() - 2);
						sectionTrans.number = trans[trans.length() - 2];
						sectionTrans.desig = trans.back();
					}
					else sectionTrans.base = trans;
				}
			}

			sectionSid.trans = std::move(sectionTrans);

			messageHandler->writeMessage("DEBUG", "[ESE] Stored value: [" + sectionSid.base + sectionSid.number +
				((sectionSid.desig) ? std::string(1, *sectionSid.desig) : "") +
				"] and trans: [" + sectionSid.trans.base + ((sectionSid.trans.number) ? std::string(1, *sectionSid.trans.number) : "") +
				((sectionSid.trans.desig) ? std::string(1, *sectionSid.trans.desig) : "") + "]",
				vsid::MessageHandler::DebugArea::Dev);

			this->sectionSids_.emplace(std::move(sectionSid));
		}
		catch (const std::out_of_range& e)
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
