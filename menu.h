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

#include "Windows.h"
#include "afxwin.h"
#include "gdiplus.h"
#include <string>
#include <map>

#include "messageHandler.h"

namespace vsid {
	class Menu
	{
	public:

		Menu(int type, std::string title, int top, int left, int minWidth, int minHeight, bool render = false,
			COLORREF border = defaultBorder, COLORREF bg = defaultBg);
		virtual ~Menu() { messageHandler->writeMessage("DEBUG", "Deleting menu " + title, vsid::MessageHandler::DebugArea::Menu); };

		struct Text
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

		struct Button
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

		void addText(int type, std::string title, std::string txt, int topPad, int leftPad, int minWidth, int height, int weight,
			bool render = false, std::string relElem = "", COLORREF textColor = defaultTxt, COLORREF bg = defaultBg);

		void resize(int top, int left, int right, int bottom);

		void update();
		inline std::string getTitle() const { return this->title; };
		inline bool getRender() const { return this->render; };
		inline COLORREF getBg() const { return this->bg; };
		inline COLORREF getBorder() const { return this->border; };
		inline CRect getArea() const { return this->area; };
		inline std::map<std::string, vsid::Menu::Text>& getTexts() { return this->texts; };
		inline std::map<std::string, vsid::Menu::Button>& getBtns() { return this->buttons; };
		inline void toggleRender() { this->render = !this->render; };

		

	private:

		const static COLORREF defaultBorder = RGB(50, 50, 40);
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultTxt = RGB(255, 255, 255);

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
		std::map<std::string, Text> texts;
		/**
		 * first - title of button
		 * second - menuButton
		 */
		std::map<std::string, Button> buttons;
		/**
		 * first - title of submenu
		 * second - menu
		 */
		std::map<std::string, Menu> submenues;
	};
}
