#include "pch.h"

#include "display.h"
#include "constants.h"
#include "messageHandler.h"

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
		if (mMenu->getRender())
		{
			HPEN borderPen = CreatePen(PS_SOLID, 1, mMenu->getBorder());
			HBRUSH bgBrush = CreateSolidBrush(mMenu->getBg());

			dc.SelectObject(borderPen);
			dc.SelectObject(bgBrush);

			dc.Rectangle(mMenu->getArea());

			std::set<std::string> depRwys;

			for (const std::string& rwy : this->plugin->getDepRwy("EDDF"))
			{
				if (rwy.find("25") != std::string::npos) depRwys.insert("25");
				if (rwy.find("07") != std::string::npos) depRwys.insert("07");
				if (rwy.find("18") != std::string::npos) depRwys.insert("18");
			}

			for (auto &[title, txt] : mMenu->getTexts())
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

			this->AddScreenObject(MENU, title.c_str(), mMenu->getArea(), true, "");
		}
	}
	dc.Detach();
}

void vsid::Display::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (this->menues.contains(sObjectId))
	{
		int difLeft = Area.left - this->menues[sObjectId]->getArea().left;
		int difTop = Area.top - this->menues[sObjectId]->getArea().top;
		int difBot = Area.bottom - this->menues[sObjectId]->getArea().bottom;
		int difRight = Area.right - this->menues[sObjectId]->getArea().right;

		this->menues[sObjectId]->resize(Area.top, Area.left, Area.right, Area.bottom);

		for(auto &[title, txt] : this->menues[sObjectId]->getTexts())
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
	if (std::string(sCommandLine) == ".vsid startup")
	{
		messageHandler->writeMessage("DEBUG", "Startup triggered.", vsid::MessageHandler::DebugArea::Dev);

		if (!this->menues.contains("mainmenu"))
		{
			CRect rArea = this->GetRadarArea();

			vsid::Menu* newMenu = new vsid::Menu(MENU, "mainmenu", rArea.bottom - 100, rArea.right - 200, 50, 50, true);
			newMenu->addText(MENU_TEXT, "toprwy", "", 10, 10, 40, 20, 400, true);
			newMenu->addText(MENU_TEXT, "bottomrwy", "", 10, 0, 40, 20, 400, true, "toprwy");

			newMenu->update();

			this->menues.insert({ newMenu->getTitle(), newMenu });
		}
		else this->menues["mainmenu"]->toggleRender();

		return true;
	}
	return false;
}