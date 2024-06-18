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
#include "vSIDPlugin.h"

//#include "utils.h"
#ifndef __AFXWIN_H__
	#error "'pch.h' vor dieser Datei für PCH einschließen"
#endif

#include "resource.h"		// Hauptsymbole


// CvSIDApp
// Informationen zur Implementierung dieser Klasse finden Sie unter vSID.cpp.
//

/**
 * @brief DLL - main app for the plugin
 * 
 */
namespace vsid
{
	//class VSIDPlugin;
	class VSIDApp : public CWinApp
	{
	public:
		VSIDApp();

		// Überschreibungen
	public:
		virtual BOOL InitInstance() override;
		/**
		 * @brief pointer to the plugin for ES
		 *
		 */
		//vsid::VSIDPlugin* gpMyPlugin = NULL;
		//vsid::VSIDPlugin* vsidPlugin = NULL;
		DECLARE_MESSAGE_MAP()
	};
}
