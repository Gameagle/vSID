#pragma once

#include <string>

namespace vsid
{
	namespace sids
	{
		struct Point
		{
			float lat;
			float lon;
		};

		struct SIDArea
		{
			Point P1;
			Point P2;
			Point P3;
			Point P4;
		};

		struct sid
		{
			std::string waypoint = "";
			char number = ' ';
			char designator = ' ';
			std::string rwy = "";
			int initialClimb = 0;
			bool climbvia = 0;
			int prio = 0;
			bool pilotfiled = 0;
			std::string wtc = "";
			std::string engineType = "";
			int engineCount = 0;
			int mtow = 0;
			SIDArea area = {};
		};
	}
}