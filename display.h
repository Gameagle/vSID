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

#include "include/es/EuroScopePlugIn.h"
#include "menu.h"

#include <gdiplus.h>
#include <map>
#include <memory>

#include <cmath>
#include <numbers>

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
		Display(int id, std::shared_ptr<vsid::VSIDPlugin> plugin, const std::string name);

		/**
		 * @brief copy constructor
		 * 
		 * @param other - object to copy from
		 */
		Display(vsid::Display& other) noexcept : id(other.id), plugin(other.plugin), name(other.name) {};

		/**
		 * @brief move constructor
		 * 
		 * @param other - object to move
		 */
		Display(vsid::Display&& other) noexcept : id(other.id), plugin(other.plugin), name(std::move(other.name)) {};
		virtual ~Display();

		/* ES Functions*/

		void OnAsrContentLoaded(bool loaded);
		void OnAsrContentToBeClosed();
		void OnRefresh(HDC hDC, int Phase);
		void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released);
		void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button);
		bool OnCompileCommand(const char* sCommandLine);

		//************************************
		// Description: will be called from main plugin whenever the same function there is called
		// Method:    OnAirportRunwayActivityChanged
		// FullName:  vsid::Display::OnAirportRunwayActivityChanged
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void OnAirportRunwayActivityChanged();

		/* Own Functions*/

		//************************************
		// Description: opens main menu or creates it if it doesn't exist
		// Method:    openMainMenu
		// FullName:  vsid::Display::openMainMenu
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: int top - initial top position
		// Parameter: int left - initial left position
		// Parameter: bool render - if the menu should be rendered or not
		//************************************
		void openMainMenu(int top = -1, int left = -1, bool render = true);

		//************************************
		// Description: opens startup menu or creates it if it doesn't exist
		// Method:    openStartupMenu
		// FullName:  vsid::Display::openStartupMenu
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string apt - upper string ICAO
		// Parameter: const std::string parent - title of parent menu
		// Parameter: bool render - if the menu should be rendered or not
		// Parameter: int top - top position
		// Parameter: int left - left position
		//************************************
		void openStartupMenu(const std::string apt, const std::string parent, bool render = true, int top = 0, int left = 0);

		//************************************
		// Description: closes specified menu
		// Method:    closeMenu
		// FullName:  vsid::Display::closeMenu
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & title - title of menu to close
		//************************************
		void closeMenu(const std::string& title);

		//************************************
		// Description: deletes (destroys) the specified menu
		// Method:    removeMenu
		// FullName:  vsid::Display::removeMenu
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & title - title of menu to remove
		//************************************
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
		std::string name;

		//************************************
		// Description: Gets the current diagonal distance of the ES instance in NM
		// Method:    getScreenNM
		// FullName:  vsid::Display::getScreenNM
		// Access:    private 
		// Returns:   double
		// Qualifier:
		//************************************
		inline double getScreenNM()
		{
			POINT topLeft = { this->GetRadarArea().top, this->GetRadarArea().left };
			POINT botRight = { this->GetRadarArea().bottom, this->GetRadarArea().right };

			EuroScopePlugIn::CPosition coordTL = this->ConvertCoordFromPixelToPosition(topLeft);
			EuroScopePlugIn::CPosition coordBR = this->ConvertCoordFromPixelToPosition(botRight);

			return coordTL.DistanceTo(coordBR);
		}

		//************************************
		// Description: Gets the current diagonal distance of the ES instance in pixels
		// Method:    getScreenDiagonalPx
		// FullName:  vsid::Display::getScreenDiagonalPx
		// Access:    private 
		// Returns:   double
		// Qualifier:
		//************************************
		inline double getScreenDiagonalPx()
		{
			return std::hypot(this->GetRadarArea().right, this->GetRadarArea().bottom);
		}

		//************************************
		// Description: Gets the current "zoom level" of the ES instance rounded to a base of hundreds
		// Method:    getZoomLevel
		// FullName:  vsid::Display::getZoomLevel
		// Access:    private 
		// Returns:   int
		// Qualifier:
		//************************************
		inline int getZoomLevel()
		{
			return (std::round(getScreenNM() * 100.0) / 100.0) * 100.0;
		}

		//************************************
		// Description: Calculates the offset for indicators in meters as geodatic values (lat/lon)
		// Method:    calculateIndicatorMeterOffset
		// FullName:  vsid::Display::calculateIndicatorMeterOffset
		// Access:    private 
		// Returns:   EuroScopePlugIn::CPosition
		// Qualifier:
		// Parameter: double lat - base lat
		// Parameter: double lon - base lon
		// Parameter: double offset - offset in meters
		// Parameter: double deg - screen bearing (0.0 top of screen, 180.0 bottom of screen, etc)
		//************************************
		EuroScopePlugIn::CPosition calculateIndicatorMeterOffset(double lat, double lon, double offset, double deg);

		//************************************
		// Description: Get the actual offset as ES::CPosition for the drawing of an indicator (convert to PixelCoordinates after)
		// Method:    getIndicatorPxOffset
		// FullName:  vsid::Display::getIndicatorPxOffset
		// Access:    private 
		// Returns:   EuroScopePlugIn::CPosition
		// Qualifier:
		// Parameter: EuroScopePlugIn::CPosition basePos - radar target position
		// Parameter: double offset - desired offset in px
		// Parameter: double zoomScale - affects offset (0 - constant px gap, 1 - constant meter gap, < 0 - px shrink on zoom in, > 0 - pixel grow on zoom in)
		//			  -> technically this behaviour is only valid if internal pxPerMeter stays < 1
		// Parameter: double deg - screen bearing (0.0 top of screen, 180.0 bottom of screen, etc)
		//************************************
		EuroScopePlugIn::CPosition getIndicatorOffset(EuroScopePlugIn::CPosition basePos, double offset, double zoomScale, double deg);
	};
}
