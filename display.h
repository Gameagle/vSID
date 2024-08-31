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

		struct menuText
		{
			int type;
			std::string title;
			std::string txt;
			int height;
			int weight;
			bool render = false;
			CRect area;
			COLORREF textColor;
			COLORREF bg;
		};

		struct menuButton
		{
			int type;
			std::string title;
			std::string label;
			CRect area;
			bool render = false;
			COLORREF textColor;
			COLORREF border;
			COLORREF bg;
		};

		struct menu
		{
			int type;
			std::string title;
			CRect area;
			bool render = false;
			COLORREF border;
			COLORREF bg;
			/**
			 * first - title of text
			 * second - menuText
			 */
			std::map<std::string, menuText> texts;
			/**
			 * first - title of button
			 * second - menuButton
			 */
			std::map<std::string, menuButton> buttons;
			/**
			 * first - title of submenu
			 * second - menu
			 */
			std::map<std::string, menu> submenues;
		};

		struct padding
		{
			int top;
			int left;
			int right;
			int bottom;
		};

		// Own Function
		menu createMenu(int type, std::string title, int top, int left, int minWidth, int minHeight, bool render = false,
						COLORREF border = defaultBorder, COLORREF bg = defaultBg);

		void addText(menu& menu, int type, std::string title, std::string txt, int topPad, int leftPad, int minWidth, int height, int weight,
			bool render = false, std::string relElem = "", COLORREF textColor = defaultTxt, COLORREF bg = defaultBg);

		/**
		 * @brief Check position of all elements and increases the menu with a default padding of 10 right and bottom
		 * 
		 * @param menu to be updated
		 */
		void updateMenu(menu& menu);
		/**
		 * first - name of the menu (ObjectID)
		 * second - menu of struct menu
		 */
		std::map<std::string, menu> menues;

		int id;
		vsid::VSIDPlugin* plugin;
	};
}
