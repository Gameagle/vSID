/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flightplans.

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

#include "es/EuroScopePlugIn.h"
#include "vSIDPlugin.h" // needed to delete stored pointers
#include "menu.h"

#include <gdiplus.h>
#include <map>




namespace vsid
{
	class Display : public EuroScopePlugIn::CRadarScreen
	{
	public:
		Display(int id, vsid::VSIDPlugin* plugin);
		virtual ~Display();

		inline void OnAsrContentToBeClosed() {
			for (auto& [title, menu] : this->menues) delete menu;
			this->menues.clear();

			this->plugin->deleteScreen(this->id);
			delete this; 
		}

		void OnRefresh(HDC hDC, int Phase);
		void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released);
		bool OnCompileCommand(const char* sCommandLine);

	private:

		const static COLORREF defaultBorder = RGB(50, 50, 40);
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultTxt = RGB(255, 255, 255);


		struct padding
		{
			int top;
			int left;
			int right;
			int bottom;
		};

		std::map<std::string, vsid::Menu*> menues;

		int id;
		vsid::VSIDPlugin* plugin;
	};
}
