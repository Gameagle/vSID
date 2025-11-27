/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flight plans.

Copyright (C) 2024 Gameagle (Philip Maier)
Repo @ https://github.com/Gameagle/vSID

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <fstream>
#include <set>
#include <vector>
#include <compare>

#include "utils.h"
#include "area.h" // only for point calculation

namespace vsid
{
	struct SectionAtc
	{
		std::string callsign;
		std::string si;
		double freq;
		std::vector<EuroScopePlugIn::CPosition> visPoints;

		explicit SectionAtc(std::string callsign, std::string si, double freq, std::vector<EuroScopePlugIn::CPosition> visPoints) :
			callsign(std::move(callsign)), si(std::move(si)), freq(freq), visPoints(std::move(visPoints)) {}

		bool operator<(const vsid::SectionAtc& other) const noexcept
		{
			return si < other.si;
		}
	};

	struct SectionTransition
	{
		std::string base;
		std::optional<char> number;
		std::optional<char> desig;

		explicit SectionTransition(std::string base, std::optional<char> number, std::optional<char> desig) :
			base(std::move(base)), number(std::move(number)), desig(std::move(desig)) {}

		auto operator<=>(const vsid::SectionTransition&) const = default;
	};

	struct SectionSID
	{
		std::string apt;
		std::string base;
		char number;
		std::optional<char> desig;
		std::string rwy;
		vsid::SectionTransition trans;

		explicit SectionSID(std::string apt, std::string base, char number, std::optional<char> desig, std::string rwy = "",
			vsid::SectionTransition trans = vsid::SectionTransition( "", std::nullopt, std::nullopt)) :
			apt(std::move(apt)), base(std::move(base)), number(number), desig(std::move(desig)), rwy(std::move(rwy)), trans(std::move(trans)) {}

		auto operator<=> (const vsid::SectionSID&) const = default;
	};

	class EseParser
	{
	public:
		EseParser(std::set<vsid::SectionAtc>& sectionAtc, std::set<vsid::SectionSID>& sectionSid) :
			sectionAtc_(sectionAtc), sectionSids_(sectionSid)
		{
		}

		void parseEse(const std::filesystem::path& path);

	private:
		enum class Section { None, Positions, SidsStars, Unknown };

		//************************************
		// Description: Transform a found ese section into section type
		// Method:    toSection
		// FullName:  vsid::EseParser::toSection
		// Access:    private static 
		// Returns:   vsid::EseParser::Section
		// Qualifier:
		// Parameter: std::string_view header
		//************************************
		static Section toSection(std::string_view header)
		{
			if (header == "[POSITIONS]") return Section::Positions;
			if (header == "[SIDSSTARS]") return Section::SidsStars;
			return Section::Unknown;
		}

		//************************************
		// Description: Work to be done on entering given section of the ese-file
		// Method:    enter
		// FullName:  vsid::EseParser::enter
		// Access:    private 
		// Returns:   void
		// Qualifier:
		// Parameter: Section s
		//************************************
		void enter(Section s);

		//************************************
		// Description: Work to be done on each line in an ese file section
		// Method:    line
		// FullName:  vsid::EseParser::line
		// Access:    private 
		// Returns:   void
		// Qualifier:
		// Parameter: Section s
		// Parameter: std::string_view l
		//************************************
		void line(Section s, std::string_view l);

		//************************************
		// Description: Work to be done on leaving a section of the ese file
		// Method:    exit
		// FullName:  vsid::EseParser::exit
		// Access:    private 
		// Returns:   void
		// Qualifier:
		// Parameter: Section s
		//************************************
		void exit(Section s);

		//************************************
		// Description: Checks if the given line is a header / section start in the ese file
		// Method:    isHeader
		// FullName:  vsid::EseParser::isHeader
		// Access:    private static 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view line
		//************************************
		static bool isHeader(std::string_view line)
		{
			return line.size() >= 2 && line.front() == '[' && line.back() == ']';
		}

		//************************************
		// Description: Checks if the given line is empty or is considered a comment in the ese file
		// Method:    isEmptyOrComment
		// FullName:  vsid::EseParser::isEmptyOrComment
		// Access:    private static 
		// Returns:   bool
		// Qualifier:
		// Parameter: std::string_view svLine
		//************************************
		static bool isEmptyOrComment(std::string_view svLine)
		{
			size_t i = 0;
			while (i < svLine.size() && isspace(static_cast<unsigned char>(svLine[i]))) ++i;
			if (i >= svLine.size()) return true;

			char c = svLine[i];
			return c == ';';
		};

		Section currSection = Section::None;
		std::set<vsid::SectionAtc>& sectionAtc_;
		std::set<vsid::SectionSID>& sectionSids_;

	};
}
