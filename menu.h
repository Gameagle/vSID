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

#include "messageHandler.h"

#include "Windows.h"
#include "afxwin.h"
#include "gdiplus.h"
#include <string>
#include <map>
#include <utility>
#include <set>


namespace vsid {

	struct Spacing
	{
		int top = 0;
		int right = 0;
		int bottom = 0;
		int left = 0;
	};

	class Text
	{
	public:

		/**
		 * @brief standard constructor
		 */
		Text(int type = 0, std::string title = "", CRect base = {}, std::string txt = "", int minWidth = 0, int height = 0, int weight = 0, vsid::Spacing margin = {0,0,0,0},
			std::string relElem = "", bool render = false, COLORREF textColor = defaultTxt, COLORREF bg = defaultBg, std::pair<int, int> tablePos = { -1,-1 }) :
			type(type), title(title), base(base), txt(txt), minWidth(minWidth), height(height), weight(weight), margin(margin), relElem(relElem), render(render),
			 textColor(textColor), bg(bg), tablePos(tablePos)
			{
				messageHandler->writeMessage("DEBUG", "Text " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
			};

		/**
		 * @brief copy constructor
		 * 
		 * @param other - the copied Text class
		 */
		Text(const vsid::Text& other) : type(other.type), title(other.title), base(other.base), txt(other.txt), minWidth(other.minWidth), height(other.height),
			weight(other.weight), margin(other.margin), relElem(other.relElem), render(other.render), textColor(other.textColor),
			bg(other.bg), tablePos(other.tablePos), area(other.area)
		{
			messageHandler->writeMessage("DEBUG", "(copy) Text " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
		};

		/**
		 * @brief move constructor
		 * 
		 * @param other - the moved Text class
		 */
		Text(vsid::Text&& other) : type(other.type), title(std::move(other.title)), base(std::move(other.base)), txt(std::move(other.txt)),
			minWidth(other.minWidth), height(other.height), weight(other.weight), margin(std::move(other.margin)), relElem(std::move(other.relElem)),
			render(other.render), textColor(other.textColor), bg(other.bg), tablePos(other.tablePos), area(std::move(other.area))
		{
			messageHandler->writeMessage("DEBUG", "(move) Text " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
		};

		virtual ~Text() { messageHandler->writeMessage("DEBUG", "Text " + title + " was destroyed", vsid::MessageHandler::DebugArea::Menu); };

		int type;
		std::string title;
		CRect base;
		std::string txt;
		int minWidth;
		int height;
		int weight;
		vsid::Spacing margin;
		std::string relElem;
		bool render = false;
		COLORREF textColor;
		COLORREF bg;
		/**
		 * @param first - row
		 * @param second - column
		 */
		std::pair<int, int> tablePos;
		CRect area;

	private:
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultTxt = RGB(255, 255, 255);
	};

	class Button
	{
	public:

		/**
		 * @brief standard constructor 
		 */
		Button(int type = 0, std::string title = "", CRect base = {}, std::string label = "", int minWidth = 0, int height = 0,
			int weight = 0, vsid::Spacing margin = {0,0,0,0}, std::string relElem = "", bool render = false,
			COLORREF textColor = defaultTxt, COLORREF bg = defaultBg, COLORREF border = defaultBorder, std::pair<int, int> tablePos = { -1,-1 }) :
			type(type), title(title), base(base), label(label), height(height), weight(weight), margin(margin), relElem(relElem), render(render),
			textColor(textColor), bg(bg), border(border), tablePos(tablePos)
		{
			messageHandler->writeMessage("DEBUG", "Button " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
		};
		
		/**
		 * @brief copy constructor
		 * 
		 * @param other - copied Button class
		 */
		Button(const vsid::Button& other) : type(other.type), title(other.title), base(other.base), label(other.label), minWidth(other.minWidth),
			height(other.height), weight(other.weight), margin(other.margin), relElem(other.relElem), render(other.render), textColor(other.textColor),
			bg(other.bg), border(other.border), tablePos(other.tablePos), area(other.area)
		{
			messageHandler->writeMessage("DEBUG", "(copy) Button " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
		};

		/**
		 * @brief move constructor
		 * 
		 * @param other - moved Button class
		 */
		Button(vsid::Button&& other) : type(other.type), title(std::move(other.title)), base(std::move(other.base)), label(std::move(other.label)),
			height(other.height), weight(other.weight), margin(std::move(other.margin)), relElem(std::move(other.relElem)), render(other.render),
			textColor(other.textColor), bg(other.bg), border(other.border), tablePos(std::move(other.tablePos)), area(std::move(other.area))
		{
			messageHandler->writeMessage("DEBUG", "(move) Button " + title + " was created", vsid::MessageHandler::DebugArea::Menu);
		};

		virtual ~Button() { messageHandler->writeMessage("DEBUG", "Button " + title + " was destroyed", vsid::MessageHandler::DebugArea::Menu); };

		int type;
		std::string title;
		CRect base;
		std::string label;
		int minWidth;
		int height;
		int weight;
		vsid::Spacing margin;
		std::string relElem;
		bool render = false;	
		COLORREF textColor;
		COLORREF bg;
		COLORREF border;
		/**
		 * @brief tablePos
		 * @param first - row
		 * @param second - column
		 */
		std::pair<int, int> tablePos;
		CRect area;

	private:
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultBorder = RGB(50, 50, 40);
		const static COLORREF defaultTxt = RGB(255, 255, 255);

	};

	class Menu
	{
	public:

		// default
		Menu() : type(0), title(""), parent(""), area(), topBar(), bottomBar(), render(false), maxCol(1), border(defaultBorder), bg(defaultBg)
		{
			messageHandler->writeMessage("DEBUG", "Creating menu (default Constructor)", vsid::MessageHandler::DebugArea::Menu);
		};
		// standard
		Menu(int type, std::string title, std::string parent, int top, int left, int minWidth, int minHeight, bool render = false, int maxCol = 1,
			COLORREF border = defaultBorder, COLORREF bg = defaultBg);
		// copy
		Menu(const vsid::Menu& other) noexcept : type(other.type), title(other.title), parent(other.parent), area(other.area), topBar(other.topBar),
			bottomBar(other.bottomBar), render(other.render), maxCol(other.maxCol), border(other.border), bg(other.bg), texts(other.texts),
			buttons(other.buttons), table(other.table), submenues(other.submenues)
		{
			messageHandler->writeMessage("DEBUG", "Creating menu (copy Constructor): \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);
		};
		// move
		Menu(vsid::Menu&& other) noexcept : type(other.type), title(std::move(other.title)), parent(std::move(other.parent)), area(std::move(other.area)),
			topBar(std::move(other.topBar)), bottomBar(std::move(other.bottomBar)), render(other.render), maxCol(other.maxCol), border(other.border),
			bg(other.bg), texts(std::move(other.texts)), buttons(std::move(other.buttons)),
			table(other.table), submenues(std::move(other.submenues))
		{
			messageHandler->writeMessage("DEBUG", "Creating menu (move Constructor): \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);
		};
		virtual ~Menu() { messageHandler->writeMessage("DEBUG", "Deleting menu \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu); };

		/**
		 * @brief adds a text to the menu
		 * 
		 * @param type - type of text (mostly MENU_TEXT)
		 * @param title - internal name, must be unique
		 * @param base - area the text should be placed on
		 * @param txt - visible text that is rendered
		 * @param minWidth - minimum text width
		 * @param height - text height
		 * @param weight - text weight ("boldness")
		 * @param margin - text margin (top, right, bottom, left)
		 * @param relElem - title of element this is relative to
		 * @param render - if text should render
		 * @param textColor - font color
		 * @param bg - background color
		 */
		void addText(int type, std::string title, const CRect& base, std::string txt, int minWidth, int height, int weight, Spacing margin = { 0,0,0,0 },
					std::string relElem = "", bool render = true, COLORREF textColor = defaultTxt, COLORREF bg = defaultBg);

		/**
		 * @brief adds a button to the menu
		 *
		 * @param type - type of text (mostly MENU_TEXT)
		 * @param title - internal name, must be unique
		 * @param base - area the text should be placed on
		 * @param label - visible text that is rendered
		 * @param minWidth - minimum text width
		 * @param height - text height
		 * @param weight - text weight ("boldness")
		 * @param margin - text margin (top, right, bottom, left)
		 * @param relElem - title of element this is relative to
		 * @param render - if text should render
		 * @param textColor - font color
		 * @param bg - background color
		 * @param border - border color
		 */
		void addButton(int type, std::string title, const CRect& base, std::string label, int minWidth, int height, int weight, Spacing margin = { 0,0,0,0 },
			std::string relElem = "", bool render = true, COLORREF textColor = defaultTxt, COLORREF bg = defaultBg, COLORREF border = defaultBorder);

		/**
		 * @brief resizes the menu in absolute values
		 */
		void resize(int top, int left, int right, int bottom);

		/**
		 * @brief iterates over the menu and adjusts menu dimensions and element positions
		 * 
		 */
		void update();

		/**
		 * @brief calculates difference between Area and area of type and then moves entire menu accordingly
		 * 
		 * @param type - MENU or MENU_TOP_BAR
		 * @param Area - area that has been moved according to ES
		 */
		void move(int type, RECT& Area);
		inline std::string getTitle() const { return this->title; };
		inline std::string getParent() const { return this->parent; };
		inline bool getRender() const { return this->render; };
		inline COLORREF getBg() const { return this->bg; };
		inline COLORREF getBorder() const { return this->border; };
		inline const CRect &getArea() const { return this->area; };
		inline std::map<std::string, vsid::Text>& getTexts() { return this->texts; };
		inline std::map<std::string, vsid::Button>& getBtns() { return this->buttons; };

		/**
		 * @brief add title of an existing menu as submenu
		 */
		inline void addSubmenu(std::string title) { this->submenues.insert(title); };
		inline std::set<std::string>& getSubmenues() { return this->submenues; };
		inline void toggleRender() { this->render = !this->render; };
		//inline const std::pair<CRect, CRect> &getBars() const { return { this->topBar, this->bottomBar }; };
		/**
		 * @brief returns the top left position (x - left, y - top)
		 */
		inline const CPoint topLeft() const { return this->area.TopLeft(); };
		inline const CRect& getTopBar() const { return this->topBar; };
		inline const CRect& getBotBar() const { return this->bottomBar; };

	private:

		const static COLORREF defaultBorder = RGB(50, 50, 40);
		const static COLORREF defaultBg = RGB(130, 150, 140);
		const static COLORREF defaultTxt = RGB(255, 255, 255);

		int type;
		std::string title;
		std::string parent;
		CRect area;
		CRect topBar;
		CRect bottomBar;
		bool render = false;
		int maxCol;
		COLORREF border;
		COLORREF bg;
		/**
		 * @param first - title of text
		 * @param second - Text class
		 */
		std::map<std::string, vsid::Text> texts;
		/**
		 * @param first - title of button
		 * @param second - Button class
		 */
		std::map<std::string, vsid::Button> buttons;
		/**
		 * @brief counter for current rows and columns (new row only when columns filled)
		 * @param first - row counter
		 * @param second - column counter with title of element
		 */
		std::map<int, std::map<int, std::string>> table;
		/**
		 * @brief Menu names of submenues
		 */
		std::set<std::string> submenues;
	};
}
