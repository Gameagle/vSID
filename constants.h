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

const int TAG_ITEM_VSID_CLIMB = 640;
const int TAG_FUNC_VSID_CLMBMENU = 740;

const int TAG_ITEM_VSID_SIDS = 641;
const int TAG_FUNC_VSID_SIDS_AUTO = 741;
const int TAG_FUNC_VSID_SIDS_MAN = 742;

const int TAG_ITEM_VSID_RWY = 642;
const int TAG_FUNC_VSID_RWYMENU = 743;

const int TAG_ITEM_VSID_SQW = 643;

const int TAG_ITEM_VSID_REQ = 644;
const int TAG_FUNC_VSID_REQMENU = 744;

const int TAG_ITEM_VSID_REQTIMER = 645;

// menues

const int MENU = 100;
const int MENU_TEXT = 110;
const int MENU_BUTTON = 120;
const int MENU_BUTTON_CLOSE = 121;
const int MENU_TOP_BAR = 111;
const int MENU_BOTTOM_BAR = 112;
