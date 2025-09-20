#include "pch.h"

#include "display.h"
#include "vSIDPlugin.h" // forward declaration
#include "constants.h"
#include "messageHandler.h"
#include "utils.h"

#include <utility>

vsid::Display::Display(int id, std::shared_ptr<vsid::VSIDPlugin> plugin, const std::string name) : EuroScopePlugIn::CRadarScreen()
{ 
	this->id = id;
	this->plugin = plugin;
	this->name = name;
}
vsid::Display::~Display() { messageHandler->writeMessage("DEBUG", "Removed display with id: " + std::to_string(this->id), vsid::MessageHandler::DebugArea::Menu); }

 void vsid::Display::OnAsrContentToBeClosed()
{
	// for (auto& [title, menu] : this->menues) delete menu; // re-evaluate
	this->menues.clear();

	if (std::shared_ptr sharedPlugin = this->plugin.lock())
	{
		messageHandler->writeMessage("DEBUG", "Removing display with id: " + std::to_string(this->id) + " from saved displays.", vsid::MessageHandler::DebugArea::Menu);
		sharedPlugin->deleteScreen(this->id);
	}
	else messageHandler->writeMessage("ERROR", "Could not remove display id: " + std::to_string(this->id) +
		" from vSID as vSID is already destroyed. Code: " + ERROR_DSP_REMOVE);
	
	//delete this;
}

void vsid::Display::OnRefresh(HDC hDC, int Phase)
{
	if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

	CDC dc;
	dc.Attach(hDC);

	
	if (std::shared_ptr sharedPlugin = this->plugin.lock())
	{
		std::string showPbOn = "";
		bool enablePbIndicator = false;

		std::string showReqOn = "";
		bool enableRequest = false;

		std::string showIntOn = "";
		bool enableIntIndicator = false;

		try
		{
			showPbOn = sharedPlugin->getConfigParser().getMainConfig().at("display").value("showPbIndicatorOn", "");
			enablePbIndicator = sharedPlugin->getConfigParser().getMainConfig().at("display").value("enablePbIndicator", true);

			showReqOn = sharedPlugin->getConfigParser().getMainConfig().at("display").value("showRequestOn", "");
			enableRequest = sharedPlugin->getConfigParser().getMainConfig().at("display").value("enableRequest", true);

			showIntOn = sharedPlugin->getConfigParser().getMainConfig().at("display").value("showIntIndicatorOn", "");
			enableIntIndicator = sharedPlugin->getConfigParser().getMainConfig().at("display").value("enableIntIndicator", true);

			messageHandler->removeGenError(ERROR_CONF_DISPLAY);
		}
		catch (const json::out_of_range &e)
		{
			if (!messageHandler->genErrorsContains(ERROR_CONF_DISPLAY))
			{
				messageHandler->writeMessage("ERROR", "[Range] Missing config section during display refresh: " +
					std::string(e.what()) + ". Code: " + ERROR_CONF_DISPLAY);
				messageHandler->addGenError(ERROR_CONF_DISPLAY);
			}
		}

		// #dev - old font order
		/*CFont font;
		LOGFONT lgfont;
		memset(&lgfont, 0, sizeof(LOGFONT));

		CFont* oldFont = dc.SelectObject(&font);

		strcpy_s(lgfont.lfFaceName, LF_FACESIZE, _TEXT("EuroScope"));
		lgfont.lfHeight = 15;
		lgfont.lfWeight = FW_BOLD;

		font.CreateFontIndirectA(&lgfont);*/
		// end dev

		LOGFONT lgfont;
		memset(&lgfont, 0, sizeof(LOGFONT));
		strcpy_s(lgfont.lfFaceName, LF_FACESIZE, _TEXT("EuroScope"));
		lgfont.lfHeight = 15;
		lgfont.lfWeight = FW_BOLD;
		lgfont.lfOutPrecision = OUT_TT_ONLY_PRECIS;

		CFont font;
		font.CreateFontIndirectA(&lgfont);
		CFont* oldFont = dc.SelectObject(&font);

		for (auto& [callsign, fplnInfo] : sharedPlugin->getProcessed())
		{
			EuroScopePlugIn::CRadarTarget target = sharedPlugin->RadarTargetSelect(callsign.c_str());

			// pushback indicator 

			if (enablePbIndicator && showPbOn.find(this->name) != std::string::npos &&
				this->getZoomLevel() <= sharedPlugin->getConfigParser().getIndicatorDefaultValues().showBelowZoom)
			{
				if (fplnInfo.gndState == "PUSH")
				{
					POINT targetPos = this->ConvertCoordFromPositionToPixel(target.GetPosition().GetPosition());

					CRect area;
					/*area.bottom = pos.y + 20; -- arrow position below target
					area.top = area.bottom - 15;
					area.left = pos.x - 5;
					area.right = area.left + 10;*/

					// arrow position left of target

					area.bottom = targetPos.y + this->getLabelOffset(); // 10; 10px fixed before
					area.top = area.bottom - 15;
					area.left = targetPos.x - 15;
					area.right = area.left + 10;

					dc.SelectObject(&font);

					dc.SetTextColor(sharedPlugin->getConfigParser().getColor("pbIndicator"));

					dc.DrawText("\x7C", &area, DT_BOTTOM);
				}
			}

			// request indicator

			if (enableRequest && showReqOn.find(this->name) != std::string::npos &&
				this->getZoomLevel() <= sharedPlugin->getConfigParser().getIndicatorDefaultValues().showBelowZoom)
			{
				std::string adep = target.GetCorrelatedFlightPlan().GetFlightPlanData().GetOrigin();
				std::string fplnRwy = target.GetCorrelatedFlightPlan().GetFlightPlanData().GetDepartureRwy();

				if (fplnInfo.request != "" && adep != "")
				{
					try
					{
						for (auto& [type, req] : sharedPlugin->getActiveApts().at(adep).requests)
						{
							if (type != fplnInfo.request) continue;

							for (std::set<std::pair<std::string, long long>>::iterator it = req.begin(); it != req.end(); ++it)
							{
								if (it->first != callsign) continue;

								size_t pos = std::distance(it, req.end());

								std::string reqPos = vsid::utils::toupper(type).at(0) + std::to_string(pos);

								POINT targetPos = this->ConvertCoordFromPositionToPixel(target.GetPosition().GetPosition());

								CRect area;

								area.bottom = targetPos.y + this->getLabelOffset(); // 10; 10px fixed before
								area.top = area.bottom - 15;
								area.right = targetPos.x + 30;
								area.left = area.right - 25;

								dc.SelectObject(&font);

								dc.SetTextColor(sharedPlugin->getConfigParser().getColor("reqIndicator"));

								dc.DrawText(reqPos.c_str(), &area, DT_BOTTOM);
							}
						}

						for (auto& [type, rwys] : sharedPlugin->getActiveApts().at(adep).rwyrequests)
						{
							if (type != fplnInfo.request) continue;

							for (auto& [rwy, rwyReq] : rwys)
							{
								if (fplnRwy == "" || fplnRwy != rwy) continue;

								for (std::set<std::pair<std::string, long long>>::iterator it = rwyReq.begin(); it != rwyReq.end(); ++it)
								{
									if (it->first != callsign) continue;

									size_t pos = std::distance(it, rwyReq.end());
									std::string reqPos = vsid::utils::toupper(type).at(0) + std::to_string(pos);
									POINT targetPos = this->ConvertCoordFromPositionToPixel(target.GetPosition().GetPosition());
									CRect area;

									area.bottom = targetPos.y + this->getLabelOffset(); // 10; 10px fixed before
									area.top = area.bottom - 15;
									area.right = targetPos.x + 30;
									area.left = area.right - 25;

									dc.SelectObject(&font);

									dc.SetTextColor(sharedPlugin->getConfigParser().getColor("reqIndicator"));

									dc.DrawText(reqPos.c_str(), &area, DT_BOTTOM);
								}
							}
						}
					}
					catch (std::out_of_range) {};
				}
			}

			// intersection indicator

			if (enableIntIndicator && showIntOn.find(this->name) != std::string::npos &&
				this->getZoomLevel() <= sharedPlugin->getConfigParser().getIndicatorDefaultValues().showBelowZoom)
			{
				if (fplnInfo.intsec.first != "")
				{
					POINT targetPos = this->ConvertCoordFromPositionToPixel(target.GetPosition().GetPosition());

					CRect area;

					// position below target
					area.bottom = targetPos.y + this->getLabelOffset();
					area.top = area.bottom - 15;
					area.left = targetPos.x - 5;
					area.right = area.left + 30; // original: 10

					// painted text
					dc.SelectObject(&font);

					if (fplnInfo.intsec.second) dc.SetTextColor(sharedPlugin->getConfigParser().getColor("intsecSetIndicator"));
					else dc.SetTextColor(sharedPlugin->getConfigParser().getColor("intsecAbleIndicator"));

					dc.DrawText(fplnInfo.intsec.first.c_str(), &area, DT_BOTTOM);
				}
			}
		}
		dc.SelectObject(oldFont);
	}

	for (auto &[title, mMenu] : this->menues) // #continue - optimization: .lock() is called above, integrate loop there
	{
		if (mMenu.getRender())
		{
			bool updateMenu = false;

			CPen borderPen = { PS_SOLID, 1, mMenu.getBorder() };
			CBrush bgBrush = { mMenu.getBg() };

			CPen* oldPen = dc.SelectObject(&borderPen);
			CBrush* oldBgBrush = dc.SelectObject(&bgBrush);

			dc.Rectangle(mMenu.getTopBar());
			dc.FillRect(mMenu.getArea(), &bgBrush);
			dc.MoveTo(mMenu.getArea().left, mMenu.getArea().top);
			dc.LineTo(mMenu.getArea().left, mMenu.getArea().bottom);
			dc.MoveTo(mMenu.getArea().right - 1, mMenu.getArea().top);
			dc.LineTo(mMenu.getArea().right - 1, mMenu.getArea().bottom);
			dc.Rectangle(mMenu.getBotBar());

			dc.SelectObject(oldPen);
			dc.SelectObject(oldBgBrush);

			this->AddScreenObject(MENU_TOP_BAR, title.c_str(), mMenu.getTopBar(), true, "");

			CFont font;
			LOGFONT lgfont;
			memset(&lgfont, 0, sizeof(LOGFONT));

			CFont* oldFont = dc.SelectObject(&font);
			
			// dev - for test rect around txt
			/*CBrush borderBrush = { RGB(255,0,0) };
			CBrush* oldBorderBrush = dc.SelectObject(&borderBrush);*/
			// end dev

			std::set<std::string> txtToRemove = {};

			for (auto &[title, txt] : mMenu.getTexts())
			{
				if (!txt.render) continue;

				if (lgfont.lfHeight != txt.height || lgfont.lfWeight != txt.weight)
				{
					font.DeleteObject();

					lgfont.lfHeight = txt.height;
					lgfont.lfWeight = txt.weight;

					font.CreateFontIndirectA(&lgfont);

					dc.SelectObject(&font);
				}
				
				dc.SetTextColor(txt.textColor);

				if (txt.title.find("depcount_") != std::string::npos)
				{
					int depCount = 0;
					try
					{
						std::string depRwy = vsid::utils::split(txt.title, '_').at(1);

						if (std::shared_ptr sharedPlugin = this->plugin.lock())
						{
							for (auto &[callsign, info] : sharedPlugin->getProcessed())
							{
								EuroScopePlugIn::CFlightPlan fpln = sharedPlugin->FlightPlanSelect(callsign.c_str());

								if(info.gndState != "")
								{
									if (sharedPlugin->RadarTargetSelect(callsign.c_str()).GetGS() >= 50) continue;

									// skip specific gnd states - also accounts for GRP gnd states
									if (info.gndState == "ARR" || info.gndState == "DE-ICE" ||
										info.gndState == "ONFREQ") continue;

									std::string fplnRwy = fpln.GetFlightPlanData().GetDepartureRwy();
									std::string adep = fpln.GetFlightPlanData().GetOrigin();
									std::string icao = "";

									try
									{
										icao = vsid::utils::split(mMenu.getTitle(), '_').at(1);
										messageHandler->removeGenError(ERROR_DSP_COUNTICAO + mMenu.getTitle());
									}
									catch (std::out_of_range)
									{
										if (!messageHandler->genErrorsContains(ERROR_DSP_COUNTICAO + mMenu.getTitle()))
										{
											messageHandler->writeMessage("ERROR", "Failed to get ICAO from menu title " +
												mMenu.getTitle() + " while counting startups. Code: " + ERROR_DSP_COUNTICAO);
											messageHandler->addGenError(ERROR_DSP_COUNTICAO + mMenu.getTitle());
										}
									}

									if (depRwy == fplnRwy && (adep == icao || icao == "")) depCount++;
									else if(!mMenu.getTexts().contains("dep_" + fplnRwy))
									{
										if (adep == icao && fplnRwy != "")
										{
											mMenu.addText(MENU_TEXT, "dep_" + fplnRwy, mMenu.getArea(), fplnRwy, 20, 20, 400, { 5,5,5,5 });
											mMenu.addText(MENU_TEXT, "depcount_" + fplnRwy, mMenu.getArea(), "", 20, 20, 400, { 5,5,5,5 }, "dep_" + fplnRwy);

											updateMenu = true;
										}
									}
								}
							}
							if (depCount == 0)
							{
								try
								{
									std::string icao = vsid::utils::split(mMenu.getTitle(), '_').at(1);

									if (!sharedPlugin->getDepRwy(icao).empty() && !sharedPlugin->getDepRwy(icao).contains(depRwy))
									{
										txtToRemove.insert("dep_" + depRwy);
										txtToRemove.insert("depcount_" + depRwy);
									}

									messageHandler->removeGenError(ERROR_DSP_COUNTRMICAO + mMenu.getTitle());
								}
								catch (std::out_of_range)
								{
									if (!messageHandler->genErrorsContains(ERROR_DSP_COUNTRMICAO + mMenu.getTitle()))
									{
										messageHandler->writeMessage("ERROR", "Failed to retrieve icao from menu " + mMenu.getTitle() +
											" during departure count check. Code: " + ERROR_DSP_COUNTRMICAO);
										messageHandler->addGenError(ERROR_DSP_COUNTRMICAO + mMenu.getTitle());
									}
								}
							}
						}

						txt.txt = std::to_string(depCount);
					}
					catch (std::out_of_range)
					{
						txt.txt = "N/A";
					}
				}

				dc.DrawText(txt.txt.c_str(), &txt.area, DT_CENTER);

				// dev - test rect around txt
				/*dc.FrameRect(&txt.area, &borderBrush);*/
				// end dev
			}

			if (!txtToRemove.empty())
			{
				for (const std::string& txt : txtToRemove)
				{
					mMenu.removeText(txt);
				}

				updateMenu = true;

				txtToRemove.clear();
			}

			for (auto& [title, btn] : mMenu.getBtns())
			{
				if (!btn.render) continue;

				if (lgfont.lfHeight != btn.height || lgfont.lfWeight != btn.weight)
				{
					font.DeleteObject();

					lgfont.lfHeight = btn.height;
					lgfont.lfWeight = btn.weight;

					font.CreateFontIndirectA(&lgfont);

					dc.SelectObject(&font);
				}

				dc.SetTextColor(btn.textColor);

				dc.DrawText(btn.label.c_str(), &btn.area, DT_CENTER);

				// dev - test rect around txt
				/*dc.FrameRect(&btn.area, &borderBrush);*/
				// end dev

				if (btn.type == MENU_BUTTON_CLOSE)
				{
					CBrush btnBg = { RGB(0, 0, 0) };
					CBrush* oldBtnBg = dc.SelectObject(&btnBg);

					dc.FillRect(btn.area, &btnBg);

					dc.SelectObject(oldBtnBg);
				}

				this->AddScreenObject(btn.type, title.c_str(), btn.area, false, "");
			}
			dc.SelectObject(oldFont);

			// dev - remove test rect around txt
			/*dc.SelectObject(oldBorderBrush);*/
			// end dev

			if (updateMenu) mMenu.update();
		}
	}
	
	dc.Detach();
}

void vsid::Display::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (this->menues.contains(sObjectId)) this->menues[sObjectId].move(ObjectType, Area);

	this->RequestRefresh();
}

void vsid::Display::OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	if (ObjectType == MENU_BUTTON)
	{
		bool newWindow = (Button == EuroScopePlugIn::BUTTON_RIGHT) ? true : false;
		
		if (std::string(sObjectId).find("apt") != std::string::npos)
		{
			try
			{
				std::string icao = vsid::utils::split(sObjectId, '_').at(1);

				/*if (this->menues.contains(sObjectId)) -------- IMPLEMENT BACK LATER WHEN AIRPORT MENUS ARE ACTIVE - MAINMENU FOR NOW*/ 
				if(this->menues.contains("mainmenu"))
				{
					/*this->openStartupMenu(icao, this->menues[sObjectId].getTitle()); 
					*if (!newWindow) this->menues[this->menues[sObjectId].getTitle()].toggleRender();
					*-------- SEE ABOVE*/

					if (!newWindow) this->menues["mainmenu"].toggleRender();
					this->openStartupMenu(icao, "mainmenu");
				}
				else
				{
					messageHandler->writeMessage("DEBUG", "menus doesn't contain " + std::string(sObjectId) + " elems are:", vsid::MessageHandler::DebugArea::Menu);
					for (auto &[title, _] : this->menues)
					{
						messageHandler->writeMessage("DEBUG", "menu title: " + title, vsid::MessageHandler::DebugArea::Menu);
					}
				}
				

				//if (!this->menues.contains("startupmenu_" + icao))
				//{

				//	/// - NO VALIDATION YET
				//	/// - IMPLEMENT CHECK FOR PARENT MENU
				//	/// - IMPLEMENT CHECK FOR PARENT MENU FOR SUBMENU

				//	CPoint topLeft = this->menues["mainmenu"].topLeft(); 
				//	
				//}
			}
			catch (std::out_of_range) {}
		}
	}
	else if (ObjectType == MENU_BUTTON_CLOSE)
	{
		std::string objectId = sObjectId;

		if (std::size_t pos = objectId.find("_close"); pos != std::string::npos)
		{
			this->closeMenu(objectId.erase(pos, objectId.length() - 1));
		}
		else
		{
			messageHandler->writeMessage("ERROR", "Couldn't close menu, because menu title couldn't be extracted from button " +
				std::string(sObjectId) + ". Code: " + ERROR_DSP_BTNEXTTITLE);
		}	
	}
}

bool vsid::Display::OnCompileCommand(const char* sCommandLine)
{
	std::vector<std::string> params = vsid::utils::split(vsid::utils::toupper(sCommandLine), ' ');

	if (std::string(sCommandLine).find(".vsid menu") != std::string::npos)
	{
		if (params.size() == 2)
		{
			this->openMainMenu();

			return true;
		}
		else if (params.size() == 3)
		{
			if (std::shared_ptr sharedPlugin = this->plugin.lock())
			{
				if (!sharedPlugin->getActiveApts().contains(params[2]))
				{
					messageHandler->writeMessage("INFO", "[" + params[2] + "] is not an active airport. Cannot open menu.");
					return true;
				}

				if (!this->menues.contains("mainmenu")) this->openMainMenu(-1, -1, false);

				this->openStartupMenu(params[2], "mainmenu");
			}
			return true;
		}
	}

	if (std::string(sCommandLine) == ".vsid display config")
	{
		messageHandler->writeMessage("INFO", "Zoom-Level: " + std::to_string(this->getZoomLevel()) + " | screen diagonal: " + std::to_string(this->getScreenDiagonalPx()));
		return true;
	}
	return false;
}

void vsid::Display::OnAirportRunwayActivityChanged()
{
	messageHandler->writeMessage("DEBUG", "[MENU] Activity Changed()", vsid::MessageHandler::DebugArea::Menu);

	if (this->menues.empty()) return;

	if (std::shared_ptr sharedPlugin = this->plugin.lock())
	{
		messageHandler->writeMessage("DEBUG", "[MENU] Shared Ptr valid.", vsid::MessageHandler::DebugArea::Menu);

		// re-create mainmenu

		int top = -1;
		int left = -1;
		bool render = false;

		for (const auto&[title,menu] : this->menues)
		{
			messageHandler->writeMessage("DEBUG", "[" + title + "] is checked on runway change", vsid::MessageHandler::DebugArea::Menu);
			if (title == std::string("mainmenu"))
			{
				CPoint topLeft = this->menues["mainmenu"].topLeft();
				top = topLeft.y;
				left = topLeft.x;
				render = this->menues["mainmenu"].getRender();
			}
			else if (title.find("startupmenu_") != std::string::npos)
			{
				messageHandler->writeMessage("DEBUG", "[" + title + "] storing startup menu", vsid::MessageHandler::DebugArea::Menu);
				this->reopenStartup.insert({title, { menu.topLeft(), menu.getParent(), menu.getRender() }});
			}
		}

		/*if (this->menues.contains("mainmenu"))
		{
			CPoint topLeft = this->menues["mainmenu"].topLeft();
			top = topLeft.y;
			left = topLeft.x;
			render = this->menues["mainmenu"].getRender();
		}*/

		this->removeMenu("mainmenu");

		this->openMainMenu(top, left, render);

		if (!this->reopenStartup.empty())
		{
			for (const auto& [title, config] : this->reopenStartup)
			{
				try
				{
					messageHandler->writeMessage("DEBUG", "[" + title + "] reopening startup menu", vsid::MessageHandler::DebugArea::Menu);
					std::string apt = vsid::utils::split(title, '_').at(1);

					if (!sharedPlugin->getActiveApts().contains(apt)) continue;

					this->openStartupMenu(apt, config.parent, config.render, config.topLeft.y, config.topLeft.x);
				}
				catch (std::out_of_range)
				{
					messageHandler->writeMessage("ERROR", "Failed to get apt ICAO while re-opening startup menu \"" + title +
						"\". Code: " + ERROR_DSP_REOPENICAO);
				}
				
			}

			this->reopenStartup.clear();
		}
	}
	else messageHandler->writeMessage("ERROR", "Couldn't update active airports for screen as plugin couldn't be accessed. Code: " + ERROR_DSP_PLUGACCESS);
}

void vsid::Display::openMainMenu(int top, int left, bool render)
{
	if (this->menues.contains("mainmenu"))
	{
		this->menues["mainmenu"].toggleRender();
		return;
	}

	int initTop = 0;
	int initLeft = 0;

	if (top == -1 || left == -1)
	{
		CRect rArea = this->GetRadarArea();

		initTop = rArea.bottom - 100;
		initLeft = rArea.right - 200;
	}
	else
	{
		initTop = top;
		initLeft = left;
	}
	

	vsid::Menu newMenu = { MENU, "mainmenu", "", initTop, initLeft, 60, 50, render, 1};

	newMenu.addText(MENU_TOP_BAR, "mainmenu", newMenu.getTopBar(), "Main Menu", 20, 20, 400, { 5,5,5,5, });

	for (auto& [title, apt] : this->plugin.lock()->getActiveApts())
	{
		messageHandler->writeMessage("DEBUG", "Button add for apt: " + title);
		newMenu.addButton(MENU_BUTTON, "apt_" + title, newMenu.getArea(), title, 20, 20, 400, { 5, 5, 5, 5 });
	}

	newMenu.update();

	this->menues.insert({ newMenu.getTitle(), std::move(newMenu) });
}

void vsid::Display::openStartupMenu(const std::string apt, const std::string parent, bool render, int top, int left)
{
	std::string title = "startupmenu_" + apt;

	if (this->menues.contains(title))
	{
		this->menues[title].toggleRender();
		return;
	}

	if (std::shared_ptr sharedPlugin = this->plugin.lock())
	{
		/*int top = 0;
		int left = 0;*/

		if (this->menues.contains(parent) && top == 0 && left == 0)
		{
			top = this->menues[parent].topLeft().y;
			left = this->menues[parent].topLeft().x;
		}

		vsid::Menu newMenu = { MENU, title, parent, top, left, 60, 50, render, 2 };

		newMenu.addText(MENU_TOP_BAR, title, newMenu.getTopBar(), "Startup", 20, 20, 400, { 5,5,5,5 });

		for (const std::string& depRwy : sharedPlugin->getDepRwy(apt))
		{
			newMenu.addText(MENU_TEXT, "dep_" + depRwy, newMenu.getArea(), depRwy, 20, 20, 400, { 5,5,5,5, });
			newMenu.addText(MENU_TEXT, "depcount_" + depRwy, newMenu.getArea(), "", 20, 20, 400, { 5,5,5,5 }, "dep_" + depRwy);
		}

		newMenu.addText(MENU_BOTTOM_BAR, "startupicao", newMenu.getBotBar(), apt, 20, 20, 400, { 5,5,5,5 });

		newMenu.update();

		if (this->menues.contains(parent)) this->menues[parent].addSubmenu(title);

		this->menues.insert({ title, std::move(newMenu) });
	}
	else messageHandler->writeMessage("ERROR", "Tried to create startup menu for [" + apt +
		"] but plugin couldn't be accessed. Code: " + ERROR_DSP_MENUSUCREATE);
}

void vsid::Display::removeMenu(const std::string &title)
{
	if (this->menues.contains(title))
	{
		for (const std::string& subTitle : this->menues[title].getSubmenues())
		{
			if (this->menues.contains(subTitle)) this->removeMenu(subTitle);
		}
		this->menues.erase(title);
	}
	else messageHandler->writeMessage("ERROR", "Called to remove menu [" + title +
		"] but it wasn't found in the menues list. Code: " + ERROR_DSP_RMMENU);
}

void vsid::Display::closeMenu(const std::string &title)
{
	if (this->menues.contains(title)) this->menues[title].toggleRender();
	else messageHandler->writeMessage("ERROR", "Couldn't close menu " + title +
		" because it is not in the menu list. Code: " + ERROR_DSP_RMMENU);
}

double vsid::Display::getLabelOffset()
{
	if (std::shared_ptr shared_plugin = this->plugin.lock())
	{
		double refPxPerNm = shared_plugin->getConfigParser().getIndicatorDefaultValues().refDiagPx / shared_plugin->getConfigParser().getIndicatorDefaultValues().refZoom;

		double gapNM = shared_plugin->getConfigParser().getIndicatorDefaultValues().refOffset / refPxPerNm;

		double pxPerNm = this->getScreenDiagonalPx() / this->getZoomLevel();

		return gapNM * pxPerNm;
	}
	return 0; // fallback state
}
