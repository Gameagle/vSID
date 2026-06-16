#include "pch.h"

#include "eseparser.h"
#include "flightplan.h"
#include "logger.h"

#include <format>

#include <thread>

void vsid::EseParser::parseEse(const std::filesystem::path& folder, const std::filesystem::path& file)
{
	std::filesystem::path fullPath = folder / file;
	std::string fileICAO = file.string().substr(0, 4);

	size_t currAtcSize = this->tmpBuffer_.sectionAtc.size();
	size_t currSidSize = this->tmpBuffer_.sectionSids.size();

	vsid::Logger::log(vsid::LogLevel::Debug, "Path to .ese for parsing: " + fullPath.string(), vsid::DebugLevel::Conf);

	std::ifstream in(fullPath, std::ios::binary | std::ios::ate);
	if (!in)
	{
		throw std::runtime_error("Failed to open .ese file in path: " + fullPath.string());
	}

	std::streamsize size = in.tellg();
	in.seekg(0, std::ios::beg);

	std::string fileContent(size, '\0');
	if (!in.read(fileContent.data(), size))
	{
		throw std::runtime_error("Failed to read .ese file content");
	}

	in.close();

	std::vector<std::string_view> lines = vsid::utils::splitSV(fileContent, '\n');

	Section currSection = Section::None;

	for(std::string_view l : lines)
	{
		l = vsid::utils::trimSV(l);
		if(l.empty() || l.front() == ';') continue;

		if (isHeader(l)) // set new section and leave old one if active
		{
			if (currSection != Section::None) exit(currSection);

			currSection = toSection(l);
			enter(currSection);
			continue;
		}

		if (currSection != Section::None) // work through non-empty and non-comment lines
		{
			line(currSection, l);	
		}
	}

	if (currSection != Section::None) exit(currSection);

	size_t atcAdded = this->tmpBuffer_.sectionAtc.size() - currAtcSize;
	size_t sidAdded = this->tmpBuffer_.sectionSids.size() - currSidSize;

	vsid::Logger::log(LogLevel::Debug, std::format("Parsed [{}] - Added [{}] stations | [{}] SIDs", fileICAO, atcAdded, sidAdded));
}

void vsid::EseParser::enter(vsid::EseParser::Section s)
{
	switch (s)
	{
	case Section::Positions:
		vsid::Logger::log(LogLevel::Debug, "Started parsing of Positions.", DebugLevel::Ese);

		break;

	case Section::SidsStars:
		vsid::Logger::log(LogLevel::Debug, "Started parsing of Sids/Stars.", DebugLevel::Ese);
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
		std::vector<std::string_view> atcVec = vsid::utils::splitSV(l, ':', true);
		std::vector<EuroScopePlugIn::CPosition> visPoints = {};

		if (atcVec.size() > 10)
		{
			for (size_t idx = 11; idx + 1 < std::min(atcVec.size(), static_cast<size_t>(20)); ++idx)
			{
				std::string_view visLat = atcVec.at(idx);
				std::string_view visLon = atcVec.at(idx + 1);

				if (visLat.empty() || visLon.empty())
				{
					vsid::Logger::log(vsid::LogLevel::Warning, std::format("Empty coordinate string found for ATC station [{}] vis point (lat [{}] lon [{}]! Skipping coordinate.",
						atcVec.at(0), visLat, visLon));
					vsid::Logger::log(vsid::LogLevel::Debug, std::format("[ESE] vis point in line : {}", l), vsid::DebugLevel::Conf);

					++idx;
					continue;
				}

				if (visLat.size() < 2 || visLon.size() < 2)
				{
					++idx;
					continue;
				}

				if (((visLat.front() == 'N' || visLat.front() == 'S') && std::isdigit(static_cast<unsigned char>(visLat.back()))) &&
					((visLon.front() == 'E' || visLon.front() == 'W') && std::isdigit(static_cast<unsigned char>(visLon.back()))))
				{
					visPoints.push_back(vsid::utils::toPoint({ std::string(visLat), std::string(visLon) }));
				}

				++idx;
			}
		}

		try
		{
			int facility = 0;

			if (atcVec.at(1).find("Information") != std::string_view::npos) facility = 1;
			else
			{
				std::optional<std::string> callsignFac = std::nullopt;

				if(std::vector<std::string_view> callsignSplit = vsid::utils::splitSV(atcVec.at(0), '_'); callsignSplit.size() > 2) 
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

			this->tmpBuffer_.sectionAtc.emplace(
				std::string(atcVec.at(0)), // callsign
				std::string(atcVec.at(3)), // si
				facility,
				std::stod(std::string(atcVec.at(2))), // freq
				visPoints // additional vis points
			); 
		}
		catch (std::out_of_range& e)
		{
			vsid::Logger::log(vsid::LogLevel::Error, "Failed to parse ATC: " + std::string(e.what()));
		}

		break;
	}
	case Section::SidsStars:
	{
		std::vector<std::string_view> sidVec = vsid::utils::splitSV(l, ':');

		try
		{
			if (sidVec.empty() || sidVec.at(0) == "STAR") break; // only parse SIDs

			std::string_view currSid = vsid::utils::trimSV(sidVec.at(3));

			if (currSid.length() < 3) break; // protection for num / desig extraction

			auto [sid, trans] = vsid::fplnhelper::splitTransitionSV(currSid);

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("Parsing SID [{}] - sid: [{}] / trans : [{}]", currSid, sid, trans), vsid::DebugLevel::Ese, true);

			vsid::SectionTransition sectionTrans("", std::nullopt, std::nullopt);
			vsid::SectionSID sectionSid("", "", '\0', std::nullopt, "", sectionTrans, "");		

			if (!sid.empty())
			{
				sectionSid.apt = vsid::utils::trimSV(sidVec.at(1));
				sectionSid.rwy = vsid::utils::trimSV(sidVec.at(2));
				if(sidVec.size() > 4) sectionSid.route = vsid::utils::trimSV(sidVec.at(4));

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

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("Stored SID [{}{}{}] and transition: [{}{}{}]",
				sectionSid.base, sectionSid.number, (sectionSid.desig) ? std::string(1, *sectionSid.desig) : "",
				sectionSid.trans.base, (sectionSid.trans.number) ? std::string(1, *sectionSid.trans.number) : "",
				(sectionSid.trans.desig) ? std::string(1, *sectionSid.trans.desig) : ""),
				vsid::DebugLevel::Ese, true);

			this->tmpBuffer_.sectionSids.emplace(std::move(sectionSid));
		}
		catch (const std::out_of_range& e)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to parse SID [{}] in .ese line [{}]", e.what(), l));
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
		break;
	case Section::SidsStars:
		break;

	case Section::Unknown:
	case Section::None:
		break;
	}
}
