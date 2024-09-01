#include "pch.h"

#include "display.h"
#include "constants.h"
#include "messageHandler.h"

// DEV
//LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// END DEV

vsid::Display::Display(int id, vsid::VSIDPlugin *plugin) : EuroScopePlugIn::CRadarScreen()
{ 
	this->id = id;
	this->plugin = plugin;
}
vsid::Display::~Display() {}

void vsid::Display::OnRefresh(HDC hDC, int Phase)
{
	if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

	CDC dc;
	dc.Attach(hDC);

	for (auto &[title, mMenu] : this->menues)
	{
		if (mMenu.render)
		{
			HPEN borderPen = CreatePen(PS_SOLID, 1, mMenu.border);
			HBRUSH bgBrush = CreateSolidBrush(mMenu.bg);

			dc.SelectObject(borderPen);
			dc.SelectObject(bgBrush);

			dc.Rectangle(mMenu.area);

			std::set<std::string> depRwys;

			for (const std::string& rwy : this->plugin->getDepRwy("EDDF"))
			{
				if (rwy.find("25") != std::string::npos) depRwys.insert("25");
				if (rwy.find("07") != std::string::npos) depRwys.insert("07");
				if (rwy.find("18") != std::string::npos) depRwys.insert("18");
			}

			for (auto &[title, txt] : mMenu.texts)
			{
				if (!txt.render) continue;

				messageHandler->writeMessage("DEBUG", "Text work on " + txt.title, vsid::MessageHandler::DebugArea::Menu);

				CFont font;
				LOGFONT lgfont;

				memset(&lgfont, 0, sizeof(LOGFONT));

				if (txt.title == "toprwy")
				{
					if (depRwys.contains("25")) txt.txt = "25";
					else if (depRwys.contains("07")) txt.txt = "07";
					else txt.txt = "";
				}
				if (txt.title == "bottomrwy")
				{
					if (depRwys.contains("18")) txt.txt = "18";
					else txt.txt = "";
				}

				lgfont.lfHeight = txt.height;
				lgfont.lfWeight = txt.weight;
				font.CreateFontIndirect(&lgfont);
				dc.SelectObject(font);
				dc.SetTextColor(txt.textColor);

				int startup2507 = 0;
				int startup18 = 0;
				for (std::pair<const std::string, vsid::fpln::Info>& fp : this->plugin->getProcessed())
				{

					EuroScopePlugIn::CFlightPlan fpln = this->plugin->FlightPlanSelect(fp.first.c_str());
					if (std::string(fpln.GetGroundState()) != "")
					{
						if (this->plugin->RadarTargetSelect(fp.first.c_str()).GetGS() >= 50) continue;
						if (std::string(fpln.GetGroundState()) == "ARR") continue;

						messageHandler->writeMessage("DEBUG", fp.first + " groundstate: \"" +
							std::string(this->plugin->FlightPlanSelect(fp.first.c_str()).GetGroundState()) + "\"", vsid::MessageHandler::DebugArea::Menu);

						std::string deprwy = fpln.GetFlightPlanData().GetDepartureRwy();
						if (deprwy.find("25") != std::string::npos && depRwys.contains("25")) startup2507++;
						if (deprwy.find("07") != std::string::npos && depRwys.contains("07")) startup2507++;
						if (deprwy.find("18") != std::string::npos && depRwys.contains("18")) startup18++;
					}
				}

				if (txt.title == "toprwy") dc.DrawText(std::string(txt.txt + ": " + std::to_string(startup2507)).c_str(), &txt.area, DT_LEFT);
				if (txt.title == "bottomrwy") dc.DrawText(std::string(txt.txt + ": " + std::to_string(startup18)).c_str(), &txt.area, DT_LEFT);

				DeleteObject(font);
			}
			
			DeleteObject(borderPen);
			DeleteObject(bgBrush);

			this->AddScreenObject(MENU, title.c_str(), mMenu.area, true, "");
		}
	}
	dc.Detach();
}

void vsid::Display::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (this->menues.contains(sObjectId))
	{
		int difLeft = Area.left - this->menues[sObjectId].area.left;
		int difTop = Area.top - this->menues[sObjectId].area.top;
		int difBot = Area.bottom - this->menues[sObjectId].area.bottom;
		int difRight = Area.right - this->menues[sObjectId].area.right;

		this->menues[sObjectId].area.left = Area.left;
		this->menues[sObjectId].area.right = Area.right;
		this->menues[sObjectId].area.top = Area.top;
		this->menues[sObjectId].area.bottom = Area.bottom;

		for(auto &[title, txt] : this->menues[sObjectId].texts)
		{
			txt.area.left += difLeft;
			txt.area.top += difTop;
			txt.area.right += difRight;
			txt.area.bottom += difBot;
		}
	}

	this->RequestRefresh();
}

bool vsid::Display::OnCompileCommand(const char* sCommandLine)
{
	messageHandler->writeMessage("DEBUG", "Display command: " + std::string(sCommandLine), vsid::MessageHandler::DebugArea::Dev);
	if (std::string(sCommandLine) == ".vsid startup")
	{
		messageHandler->writeMessage("DEBUG", "Startup triggered.", vsid::MessageHandler::DebugArea::Dev);
		
		// test menu
		if (!this->menues.contains("mainmenu"))
		{
			messageHandler->writeMessage("DEBUG", "Creating menu...", vsid::MessageHandler::DebugArea::Dev);

			CRect rArea = this->GetRadarArea();

			vsid::Display::menu newMenu = this->createMenu(MENU, "mainmenu", rArea.bottom - 100, rArea.right - 200, 50, 50, true);
			this->addText(newMenu, MENU_TEXT, "toprwy", "", 10, 10, 40, 20, 400, true);
			this->addText(newMenu, MENU_TEXT, "bottomrwy", "", 10, 0, 40, 20, 400, true, "toprwy");

			this->updateMenu(newMenu);

			this->menues.insert({ newMenu.title, newMenu });
		}
		else this->menues["mainmenu"].render = !this->menues["mainmenu"].render;

		return true;
	}
	return false;
}

vsid::Display::menu vsid::Display::createMenu(int type, std::string title, int top, int left, int minWidth, int minHeight, bool render,
									COLORREF border, COLORREF bg)
{
	messageHandler->writeMessage("DEBUG", "Creating menu \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);

	vsid::Display::menu newMenu;

	newMenu.title = title;
	newMenu.type = type;

	newMenu.area.top = top;
	newMenu.area.left = left;
	newMenu.area.right = left + minWidth;
	newMenu.area.bottom = top + minHeight;

	newMenu.border = border;
	newMenu.bg = bg;

	newMenu.render = render;

	return newMenu;
}

void vsid::Display::addText(menu& menu, int type, std::string title, std::string txt, int topPad, int leftPad, int minWidth, int height, int weight, bool render,
							std::string relElem, COLORREF txtColor, COLORREF bg)
{
	messageHandler->writeMessage("DEBUG", "Creating new text \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);

	vsid::Display::menuText newText;

	newText.type = type;
	newText.title = title;

	if (menu.texts.contains(relElem))
	{
		(topPad == 0) ? newText.area.top = menu.texts[relElem].area.top : newText.area.top = menu.texts[relElem].area.bottom + topPad;
		(leftPad == 0) ? newText.area.left = menu.texts[relElem].area.left : newText.area.left = menu.texts[relElem].area.right + leftPad;
	}
	else if (menu.buttons.contains(relElem))
	{
		newText.area.top = menu.buttons[relElem].area.bottom + topPad;
		newText.area.left = menu.buttons[relElem].area.right + leftPad;
	}
	else
	{
		newText.area.top = menu.area.top + topPad;
		newText.area.left = menu.area.left + leftPad;
	}

	(txt.size() >= minWidth) ? newText.area.right = newText.area.left + txt.size() : newText.area.right = newText.area.left + minWidth;
	newText.area.bottom = newText.area.top + height;

	newText.textColor = txtColor;
	newText.bg = bg;

	newText.txt = txt;
	newText.height = height;
	newText.weight = weight;

	newText.render = render;

	menu.texts.insert({ title, newText });
}

void vsid::Display::updateMenu(vsid::Display::menu& menu)
{
	messageHandler->writeMessage("DEBUG", "Menu width before update: " + std::to_string(menu.area.right - menu.area.left), vsid::MessageHandler::DebugArea::Menu);
	messageHandler->writeMessage("DEBUG", "Menu heigth before update: " + std::to_string(menu.area.bottom - menu.area.top), vsid::MessageHandler::DebugArea::Menu);
	for (auto &[title, txt] : menu.texts)
	{
		if (txt.area.right >= menu.area.right) menu.area.right = txt.area.right + 10;
		if (txt.area.bottom >= menu.area.bottom) menu.area.bottom = txt.area.bottom + 10;
	}

	for (auto &[title, btn] : menu.buttons)
	{
		if (btn.area.right >= menu.area.right) menu.area.right = btn.area.right + 10;
		if (btn.area.bottom >= menu.area.bottom) menu.area.bottom = btn.area.bottom + 10;
	}
	messageHandler->writeMessage("DEBUG", "Menu width after update: " + std::to_string(menu.area.right - menu.area.left), vsid::MessageHandler::DebugArea::Menu);
	messageHandler->writeMessage("DEBUG", "Menu heigth after update: " + std::to_string(menu.area.bottom - menu.area.top), vsid::MessageHandler::DebugArea::Menu);
}
