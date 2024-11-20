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
#include "menu.h"

#include <gdiplus.h>
#include <map>
#include <memory>

namespace vsid
{
	class VSIDPlugin; // forward declaration

	class Display : public EuroScopePlugIn::CRadarScreen
	{
	public:
		//Display(int id, vsid::VSIDPlugin* plugin);

		/**
		 * @brief standard constructor
		 * 
		 * @param id - internal id to distinguish multiple displays
		 * @param plugin - reference to the "main" plugin
		 */
		Display(int id, std::shared_ptr<vsid::VSIDPlugin> plugin);

		/**
		 * @brief copy constructor
		 * 
		 * @param other - object to copy from
		 */
		Display(vsid::Display& other) noexcept : id(other.id), plugin(other.plugin) {};

		/**
		 * @brief move constructor
		 * 
		 * @param other - object to move
		 */
		Display(vsid::Display&& other) noexcept : id(other.id), plugin(other.plugin) {};
		virtual ~Display();

		/* ES Functions*/

		void OnAsrContentToBeClosed();
		void OnRefresh(HDC hDC, int Phase);
		void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released);
		void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button);
		bool OnCompileCommand(const char* sCommandLine);

		/* Own Functions*/

		/**
		 * @brief will be called from main plugin whenever the same function there is called
		 */
		void OnAirportRunwayActivityChanged();

		/**
		 * @brief opens main menu or creates it if it doesn't exist
		 * 
		 * @param top - initial top position
		 * @param left - initial left position
		 */
		void openMainMenu(int top = -1, int left = -1);
		void openStartupMenu(const std::string &apt, const std::string parent);

		void closeMenu(const std::string& title);
		void removeMenu(const std::string& title);

		inline EuroScopePlugIn::CRadarScreen* getRadarScreen() { return this; };
		

	private:

		const static COLORREF defaultBorder = RGB(50, 50, 40);
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultTxt = RGB(255, 255, 255);

		struct padding
		{
			int top;
			int right;
			int bottom;
			int left;
		};

		std::map<std::string, vsid::Menu> menues;;
		int id;
		std::weak_ptr<vsid::VSIDPlugin> plugin;
	};
}
