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
		void openMainMenu(int top = -1, int left = -1, bool render = true);

		/**
		 * @brief opens startup menu or creates it if it doesn't exist
		 * 
		 * @param apt - upper string ICAO
		 * @param parent - title of parent menu
		 * @note test
		 */
		void openStartupMenu(const std::string apt, const std::string parent, bool render = true, int top = 0, int left = 0);

		/**
		 * @brief closes specified menu
		 * 
		 * @param title of menu to close
		 */
		void closeMenu(const std::string& title);

		/**
		 * @brief deletes (destroys) the specified menu
		 * 
		 * @param title of menu to remove
		 */
		void removeMenu(const std::string& title);

		
		//************************************
		// Method:    getRadarScreen
		// FullName:  vsid::Display::getRadarScreen
		// Access:    public 
		// Returns:   EuroScopePlugIn::CRadarScreen* - Display deprived from CRadarScreen
		//************************************
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

		struct storedStartup
		{
			CPoint topLeft;
			std::string parent;
			bool render;
		};

		// ************************************
		// Stores menues
		// 
		// Access:   private
		// Parameter std::string - title of menu
		// Parameter vsid::Menu - menu object
		// ************************************
		std::map<std::string, vsid::Menu> menues;
		// ************************************
		// Stores old render states of startup menu to re-enable them after rwy change
		// 
		// Access:    private
		// Parameter std::string - name of startup menu
		// Parameter bool - render state of menu
		// ************************************
		std::map<std::string, vsid::Display::storedStartup> reopenStartup;
		int id;
		std::weak_ptr<vsid::VSIDPlugin> plugin;
	};
}
