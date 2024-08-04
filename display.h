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

#include "include/es/EuroScopePlugIn.h"
#include "vSIDPlugin.h" // needed to delete stored pointers


namespace vsid
{
	class Display : public EuroScopePlugIn::CRadarScreen
	{
	public:
		Display(int id, vsid::VSIDPlugin* plugin);
		virtual ~Display();

		inline void OnAsrContentToBeClosed() {
			this->plugin->deleteScreen(this->id);
			delete this; 
		}

	private:
		int id;
		vsid::VSIDPlugin* plugin;
	};
}
