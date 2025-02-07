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
#include <string>

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

const int TAG_ITEM_VSID_CLRF = 646;
const int TAG_FUNC_VSID_CTL = 745;

const int TAG_ITEM_VSID_TRANS = 647;
const int TAG_FUNC_VSID_TRANS = 746;

const int TAG_ITEM_VSID_CLR = 648;
const int TAG_FUNC_VSID_CLR_SID = 747;
const int TAG_FUNC_VSID_CLR_SID_SU = 748;

// menues

const int MENU = 100;
const int MENU_TEXT = 110;
const int MENU_BUTTON = 120;
const int MENU_BUTTON_CLOSE = 121;
const int MENU_TOP_BAR = 111;
const int MENU_BOTTOM_BAR = 112;

// errors

const std::string ERROR_FPLN_ATCBLOCK = "E010";
const std::string ERROR_FPLN_SIDWPT = "E011";
const std::string ERROR_FPLN_SPLITWPT = "E012";
const std::string ERROR_FPLN_EXTFUNC = "E013";
const std::string ERROR_FPLN_ALLOWROUTE = "E014";
const std::string ERROR_FPLN_DENYROUTE = "E015";
const std::string ERROR_FPLN_ADEPINACTIVE = "E016";
const std::string ERROR_FPLN_SETROUTE = "E017";
const std::string ERROR_FPLN_AMEND = "E018";
const std::string ERROR_FPLN_SETALT = "E019-1";
const std::string ERROR_FPLN_SETALT_RESET = "E019-2";
const std::string ERROR_FPLN_INVALID = "E020";
const std::string ERROR_FPLN_SIDMAN_RWY = "E021";
const std::string ERROR_FPLN_REMARKSET = "E022-1";
const std::string ERROR_FPLN_REMARKRMV = "E022-2";
const std::string ERROR_FPLN_REQSET = "E023";
const std::string ERROR_FPLN_CLNFIRST = "E024-1";
const std::string ERROR_FPLN_CLNSPDLVL = "E024-2";

const std::string ERROR_GEN_SCREENCREATE = "E030";

const std::string ERROR_CONF_RWYMENU = "E040";
const std::string ERROR_CONF_COLOR = "E041";

const std::string ERROR_CMD_ATCICAO = "E050";

const std::string ERROR_ATC_ICAOSPLIT = "E060";
