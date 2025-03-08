#include "pch.h"

#include "menu.h"
#include "constants.h"

#include <stdexcept>
#include <algorithm>


vsid::Menu::Menu(int type, std::string title, std::string parent, int top, int left, int minWidth, int minHeight, bool render,
				int maxCol, COLORREF border, COLORREF bg)
{
	this->type = type;
	this->title = title;
	this->parent = parent;
	this->render = render;
	this->maxCol = maxCol;
	this->border = border;
	this->bg = bg;

	this->area.top = top;
	this->area.left = left;
	this->area.right = left + minWidth;
	this->area.bottom = top + minHeight;

	this->topBar.bottom = this->area.top;
	this->topBar.top = this->topBar.bottom - 20;
	this->topBar.left = this->area.left;
	this->topBar.right = this->area.right;

	this->bottomBar.top = this->area.bottom;
	this->bottomBar.bottom = this->bottomBar.top + 20;
	this->bottomBar.left = this->area.left;
	this->bottomBar.right = this->area.right;

	this->addButton(MENU_BUTTON_CLOSE, this->title + "_close", this->topBar, "", 5, 5, 0, { 5, 5, 5, 5 });

	messageHandler->writeMessage("DEBUG", "Creating menu " + title, vsid::MessageHandler::DebugArea::Menu);
}

void vsid::Menu::addText(int type, std::string title, const CRect &base, std::string txt, int minWidth, int height, int weight, Spacing margin,
	std::string relElem, bool render, COLORREF textColor, COLORREF bg)
{
	if (this->texts.contains(title))
	{
		messageHandler->writeMessage("ERROR", "Trying to add text \"" + title + "\" to menu \"" + this->title +
			"\". Report as an error. Code: " + ERROR_MEN_TXTADD);
		return;
	}

	messageHandler->writeMessage("DEBUG", "Creating new text \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);

	vsid::Text newText = { type, title, base, txt, minWidth, height, weight, margin, relElem, render, textColor, bg };

	// calculate new table pos and insert

	if (type == MENU_TEXT)
	{
		int maxRow = this->table.size() - 1;
		int maxCol = -1;
		if (this->table.contains(maxRow)) maxCol = this->table.at(maxRow).size() - 1;

		if (maxCol < this->maxCol - 1)
		{
			this->table[(maxRow < 0) ? 0 : maxRow][maxCol + 1] = newText.title;
			newText.tablePos = { (maxRow < 0) ? 0 : maxRow, maxCol + 1 };
		}
		else
		{
			this->table[maxRow + 1][0] = newText.title;
			newText.tablePos = { maxRow + 1, 0 };
		}

		messageHandler->writeMessage("DEBUG", "Text [" + title + "] maxRow: " + std::to_string(maxRow) +
			" - maxCol: " + std::to_string(maxCol), vsid::MessageHandler::DebugArea::Menu);

		try
		{
			auto [row, col] = newText.tablePos;

			messageHandler->writeMessage("DEBUG", "Text [" + title + "] row: " + std::to_string(row) +
				" - col: " + std::to_string(col), vsid::MessageHandler::DebugArea::Menu);

			if (row == 0)
			{
				newText.area.top = base.top + newText.margin.top;

				if (col == 0) newText.area.left = base.left + newText.margin.left;
				else
				{
					std::string onLeft = this->table.at(0).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						messageHandler->writeMessage("DEBUG", "Text [" + title + "] onLeft: " + onLeft + " contained in table", vsid::MessageHandler::DebugArea::Menu);
						newText.area.left = this->texts[onLeft].area.left + newText.margin.left;
					}
				}
			}
			else
			{
				std::string onTop = (this->table.at(row - 1).contains(col)) ? this->table.at(row - 1).at(col) : this->table.at(row - 1).at(0);

				if (this->texts.contains(onTop))
				{
					newText.area.top = this->texts[onTop].area.bottom + newText.margin.top;

					messageHandler->writeMessage("DEBUG", "Text [" + title + "] .top: " + std::to_string(newText.area.top) +
						" based on [" + onTop + "] .bottom: " + std::to_string(this->texts[onTop].area.bottom), vsid::MessageHandler::DebugArea::Menu);
				}

				if (col == 0) newText.area.left = base.left + newText.margin.left;
				else
				{
					std::string onLeft = this->table.at(row).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						messageHandler->writeMessage("DEBUG", "Text [" + title + "] onLeft: " + onLeft + " contained in table", vsid::MessageHandler::DebugArea::Menu);
						newText.area.left = this->texts[onLeft].area.left + newText.margin.left;
					}
				}
			}
		}
		catch (std::out_of_range)
		{
			messageHandler->writeMessage("ERROR", "Tried to access an invalid table position in menu \"" +
				this->title + "\" working on new text: \"" + newText.title + "\". Code: " + ERROR_MENU_TXTINVTABPOS);
			return;
		}

		newText.area.bottom = newText.area.top + height;
	}
	else if (type == MENU_TOP_BAR || MENU_BOTTOM_BAR)
	{
		newText.area.left = base.left + margin.left;
		newText.area.top = base.top + margin.top;
		newText.area.right = newText.area.left + minWidth;
		newText.area.bottom = newText.area.top + height;
	}
	else newText.title = "INVALID";

	// insert into total text overview

	if (newText.title != "INVALID") this->texts.insert({ title, newText });
	else messageHandler->writeMessage("ERROR", "Trying to add text " + title + " of invalid type to menu " +
		this->title + ". Code: " + ERROR_MEN_TXTINVTYPE);
}

void vsid::Menu::removeText(const std::string& title)
{
	if (this->texts.contains(title))
	{
		messageHandler->writeMessage("DEBUG", "Removing " + title + " from " + this->title, vsid::MessageHandler::DebugArea::Menu);
		this->texts.erase(title);

		// update table

		for (auto row = this->table.begin(); row != this->table.end();)
		{
			for (auto col = row->second.begin(); col != row->second.end();)
			{
				if (col->second == title)
				{
					col = row->second.erase(col);
					continue;
				}
				++col;
			}

			if (row->second.size() == 0)
			{
				row = this->table.erase(row);
				continue;
			}
			++row;
		}
	}
}

void vsid::Menu::addButton(int type, std::string title, const CRect& base, std::string label, int minWidth, int height, int weight, Spacing margin,
	std::string relElem, bool render, COLORREF textColor, COLORREF bg, COLORREF border)
{
	if (this->buttons.contains(title))
	{
		messageHandler->writeMessage("ERROR", "Trying to add button \"" + title + "\" to menu \"" + this->title +
			"\". Report as an error. Code: " + ERROR_MENU_BTNADD);
		return;
	}

	messageHandler->writeMessage("DEBUG", "Creating new button \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);

	vsid::Button newButton = { type, title, base, label, minWidth, height, weight, margin, relElem, render, textColor, bg, border };

	// calculate new table pos and insert

	if (type == MENU_BUTTON)
	{
		int maxRow = this->table.size() - 1;
		int maxCol = -1;

		if (this->table.contains(maxRow)) maxCol = this->table.at(maxRow).size() - 1;

		if (maxCol < this->maxCol - 1)
		{
			this->table[(maxRow < 0) ? 0 : maxRow][maxCol + 1] = newButton.title;
			newButton.tablePos = { (maxRow < 0) ? 0 : maxRow, maxCol + 1 };
		}
		else
		{
			this->table[maxRow + 1][0] = newButton.title;
			newButton.tablePos = { maxRow + 1, 0 };
		}

		messageHandler->writeMessage("DEBUG", "Button [" + title + "] maxRow: " + std::to_string(maxRow) +
			" - maxCol: " + std::to_string(maxCol), vsid::MessageHandler::DebugArea::Menu);

		try
		{
			auto [row, col] = newButton.tablePos;

			messageHandler->writeMessage("DEBUG", "Button [" + title + "] row: " + std::to_string(row) +
				" - col: " + std::to_string(col), vsid::MessageHandler::DebugArea::Menu);

			if (row == 0)
			{
				newButton.area.top = base.top + newButton.margin.top;

				if (col == 0) newButton.area.left = base.left + newButton.margin.left;
				else
				{
					std::string onLeft = this->table.at(0).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						messageHandler->writeMessage("DEBUG", "Button [" + title + "] onLeft: " + onLeft + " contained in table", vsid::MessageHandler::DebugArea::Menu);
						newButton.area.left = this->texts[onLeft].area.left + newButton.margin.left;
					}
				}
			}
			else
			{
				std::string onTop = (this->table.at(row - 1).contains(col)) ? this->table.at(row - 1).at(col) : this->table.at(row - 1).at(0);

				if (this->texts.contains(onTop))
				{
					newButton.area.top = this->texts[onTop].area.bottom + newButton.margin.top;

					messageHandler->writeMessage("DEBUG", "Button [" + title + "] .top: " + std::to_string(newButton.area.top) +
						" based on [" + onTop + "] .bottom: " + std::to_string(this->texts[onTop].area.bottom), vsid::MessageHandler::DebugArea::Menu);
				}

				if (col == 0) newButton.area.left = base.left + newButton.margin.left;
				else
				{
					std::string onLeft = this->table.at(row).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						messageHandler->writeMessage("DEBUG", "Button [" + title + "] onLeft: " + onLeft + " contained in table", vsid::MessageHandler::DebugArea::Menu);
						newButton.area.left = this->texts[onLeft].area.left + newButton.margin.left;
					}
				}
			}
		}
		catch (std::out_of_range)
		{
			messageHandler->writeMessage("ERROR", "Tried to access an invalid table position in menu \"" +
				this->title + "\" working on new text: \"" + newButton.title + "\". Code: " + ERROR_MENU_BTNINVTABPOS);
			return;
		}

		newButton.area.bottom = newButton.area.top + height;
	}
	else if (type == MENU_BUTTON_CLOSE)
	{
		newButton.area.right = base.right + margin.right;
		newButton.area.left = newButton.area.right - minWidth;
		newButton.area.bottom = base.bottom - margin.bottom;
		newButton.area.top = newButton.area.bottom - height;
	}
	else newButton.title = "INVALID";

	// insert into total text overview

	if (newButton.title != "INVALID") this->buttons.insert({ title, newButton });
	else messageHandler->writeMessage("ERROR", "Trying to add button " + title + " of invalid type to menu " +
		this->title + ". Code: " + ERROR_MEN_BTNINVTYPE);
}

void vsid::Menu::resize(int top, int right, int bottom, int left)
{
	this->area.top = top;
	this->area.right = right;
	this->area.bottom = bottom;
	this->area.left = left;
}

void vsid::Menu::update()
{
	messageHandler->writeMessage("DEBUG", "[" + this->title + "] Updating menu" , vsid::MessageHandler::DebugArea::Menu);

	HDC hDC = GetDC(NULL);
	CDC dc;

	dc.Attach(hDC);

	CFont font;
	LOGFONT lgfont;
	memset(&lgfont, 0, sizeof(LOGFONT));

	CSize txtSize;

	CFont* oldFont = dc.SelectObject(&font);

	CRect newArea = this->area;
	CRect newTopBar = this->topBar;
	CRect newBottomBar = this->bottomBar;

	// update table

	for (auto [row, columnMap] : this->table)
	{
		for (auto [col, elem] : columnMap)
		{
			messageHandler->writeMessage("DEBUG", "Text table [" + std::to_string(row) + "][" + std::to_string(col) + "]", vsid::MessageHandler::DebugArea::Menu);
			if (this->texts.contains(elem))
			{
				vsid::Text& txt = this->texts[elem];

				// adjust the font if different from previous font

				if (lgfont.lfHeight != txt.height || lgfont.lfWeight != txt.weight)
				{
					font.DeleteObject();

					lgfont.lfHeight = txt.height;
					lgfont.lfWeight = txt.weight;
					font.CreateFontIndirectA(&lgfont);

					dc.SelectObject(&font);
				}

				// special for depcount as size may vary during rendering to prevent need for constant re-updating the menu

				if (txt.title.find("depcount") != std::string::npos)
				{
					txtSize = dc.GetTextExtent("99");

					messageHandler->writeMessage("DEBUG", "depcount update - size: " + std::to_string(txtSize.cx) + "/" + std::to_string(txtSize.cy), vsid::MessageHandler::DebugArea::Menu);
				}
				else txtSize = dc.GetTextExtent(txt.txt.c_str());

				//txtSize = dc.GetTextExtent(txt.txt.c_str());

				txt.area.right = txt.area.left + txtSize.cx;
				txt.area.bottom = txt.area.top + txtSize.cy;

				// auto [row, col] = txt.tablePos;

				messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Updating text.", vsid::MessageHandler::DebugArea::Menu);

				// adjust text position based on neighbours

				try
				{
					// adjust text position in rows 

					if (row == 0)
					{
						if (txt.area.top != txt.base.top + txt.margin.top)
						{
							int height = txt.area.bottom - txt.area.top;
							txt.area.top = txt.base.top + txt.margin.top;
							txt.area.bottom = txt.area.top + height;

							messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .top [" +
								std::to_string(txt.area.top) + "] based on main area", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .top [" + std::to_string(txt.area.top) + "] matches main area .top [" +
							std::to_string(txt.base.top) + "] + margin .top [" + std::to_string(txt.margin.top) + "] -> " +
							std::to_string(txt.area.top) + " == " + std::to_string(txt.base.top + txt.margin.top), vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						std::string toTop = this->table.at(row - 1).at(col);

						if (this->texts.contains(toTop))
						{
							if (txt.area.top != this->texts[toTop].area.bottom + txt.margin.top)
							{
								int height = txt.area.bottom - txt.area.top;
								txt.area.top = this->texts[toTop].area.bottom + txt.margin.top;
								txt.area.bottom = txt.area.top + height;

								messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .top [" +
									std::to_string(txt.area.top) + "] based on above \"" + toTop + "\" .bottom [" +
									std::to_string(this->texts[toTop].area.bottom) + "] + margin.top", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .top [" + std::to_string(txt.area.top) + "] matches [" + toTop + "] .bottom [" +
								std::to_string(this->texts[toTop].area.bottom) + "] + margin .top [" + std::to_string(txt.margin.top) + "] -> " +
								std::to_string(txt.area.top) + " == " + std::to_string(this->texts[toTop].area.bottom + txt.margin.top), vsid::MessageHandler::DebugArea::Menu);
						}
						else if (this->buttons.contains(toTop))
						{
							if (txt.area.top != this->buttons[toTop].area.bottom + txt.margin.top)
							{
								int height = txt.area.bottom - txt.area.top;
								txt.area.top = this->buttons[toTop].area.bottom + txt.margin.top;
								txt.area.bottom = txt.area.top + height;

								messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .top [" +
									std::to_string(txt.area.top) + "] based on above \"" + toTop + "\" .bottom [" +
									std::to_string(this->buttons[toTop].area.bottom) + "] + margin.top", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .top [" + std::to_string(txt.area.top) + "] matches [" + toTop + "] .bottom [" +
								std::to_string(this->buttons[toTop].area.bottom) + "] + margin .top [" + std::to_string(txt.margin.top) + "] -> " +
								std::to_string(txt.area.top) + " == " + std::to_string(this->buttons[toTop].area.bottom + txt.margin.top), vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] toTop text/button [" + toTop + "] was not found.", vsid::MessageHandler::DebugArea::Menu);
					}

					// adjust text position in columns

					if (col == 0)
					{
						if (txt.area.left != txt.base.left + txt.margin.left)
						{
							int width = txt.area.right - txt.area.left;
							txt.area.left = txt.base.left + txt.margin.left;
							txt.area.right = txt.area.left + width;

							messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .left [" +
								std::to_string(txt.area.left) + "] based on main", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .left [" + std::to_string(txt.area.left) + "] matches base .left [" +
							std::to_string(txt.base.left) + "] + margin.left [" + std::to_string(txt.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						std::string toLeft = this->table.at(row).at(col - 1); ///////////////// changed 0 to row

						if (this->texts.contains(toLeft))
						{
							if (txt.area.left != this->texts[toLeft].area.right + txt.margin.left)
							{
								int width = txt.area.right - txt.area.left;
								txt.area.left = this->texts[toLeft].area.right + txt.margin.left;
								txt.area.right = txt.area.left + width;

								messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .left [" +
									std::to_string(txt.area.left) + "] based on left \"" + toLeft + "\" .right [" +
									std::to_string(this->texts[toLeft].area.right) + " + margin.left", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .left [" + std::to_string(txt.area.left) + "] matches [" + toLeft + "] .left [" +
								std::to_string(txt.base.left) + "] + margin .left [" + std::to_string(txt.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
						}
						else if (this->buttons.contains(toLeft))
						{
							if (txt.area.left != this->buttons[toLeft].area.right + txt.margin.left)
							{
								int width = txt.area.right - txt.area.left;
								txt.area.left = this->buttons[toLeft].area.right + txt.margin.left;
								txt.area.right = txt.area.left + width;

								messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjust to new .left [" +
									std::to_string(txt.area.left) + "] based on left \"" + toLeft + "\" .right [" +
									std::to_string(this->buttons[toLeft].area.right) + " + margin.left", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + txt.title + "] .left [" + std::to_string(txt.area.left) + "] matches [" + toLeft + "] .left [" +
								std::to_string(txt.base.left) + "] + margin .left [" + std::to_string(txt.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "toLeft text/button [" + toLeft + "] was not found", vsid::MessageHandler::DebugArea::Menu);
					}
				}
				catch (std::out_of_range)
				{
					messageHandler->writeMessage("ERROR", "Failed to update [" + this->title + "] when updating text [" +
						txt.title + "]. Code: " + ERROR_MENU_TXTUPDATE);
				}

				// adjust base area right side based on margin

				if (txt.area.right + txt.margin.right > txt.base.right)
				{
					if (txt.base == this->area)
					{
						newArea.right = txt.area.right + txt.margin.right;

						messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjusting main area to new .right [" +
							std::to_string(newArea.right) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
				}

				// adjust base area bottom side based on margin

				if (txt.area.bottom + txt.margin.bottom > txt.base.bottom)
				{
					if (txt.base == this->area)
					{
						newArea.bottom = txt.area.bottom + txt.margin.bottom;

						messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjusting main area to new .bottom [" +
							std::to_string(newArea.bottom) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						messageHandler->writeMessage("DEBUG", "[" + txt.title + "] base area mismatch, base.bottom smaller", vsid::MessageHandler::DebugArea::Menu);
					}
				}
				else if (txt.base.bottom > txt.area.bottom + txt.margin.bottom)
				{
					if (txt.base == this->area)
					{
						newArea.bottom = txt.area.bottom + txt.margin.bottom;

						messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjusting main area to new .bottom [" +
							std::to_string(newArea.bottom) + "] (reducing base area)", vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						messageHandler->writeMessage("DEBUG", "[" + txt.title + "] base area mismatch, base.bottom greater", vsid::MessageHandler::DebugArea::Menu);
					}
				}

				// re-adjust new top and bottom bar to new area

				if (newTopBar.bottom != newArea.top)
				{
					int height = newTopBar.bottom - newTopBar.top;

					newTopBar.bottom = newArea.top;
					newTopBar.top = newTopBar.bottom - height;

					messageHandler->writeMessage("DEBUG", "[topBar] Adjusting top bar area. New .bottom [" + std::to_string(newTopBar.bottom) +
						"]", vsid::MessageHandler::DebugArea::Menu);
				}

				if (newBottomBar.top != newArea.bottom)
				{
					int height = newBottomBar.bottom - newBottomBar.top;

					newBottomBar.top = newArea.bottom;
					newBottomBar.bottom = newBottomBar.top + height;

					messageHandler->writeMessage("DEBUG", "[bottomBar] Adjusting bottom bar area. New .top [" + std::to_string(newTopBar.top) +
						"]", vsid::MessageHandler::DebugArea::Menu);
				}

				// move txt area based on margin and re-adjusted base area

				if ((txt.area.top != txt.base.top + txt.margin.top) && row == 0)
				{
					int height = txt.area.bottom - txt.area.top;

					txt.area.top = txt.base.top + txt.margin.top;
					txt.area.bottom = txt.area.top + height;

					messageHandler->writeMessage("DEBUG", "[" + txt.title + "] Adjusting to changed base with new .top [" +
						std::to_string(txt.area.top) + "]", vsid::MessageHandler::DebugArea::Menu);
				}

				// adjust right side of menu to the farthest

				int farRight = std::max({ newTopBar.right, newArea.right, newBottomBar.right });

				if (newTopBar.right != farRight) newTopBar.right = farRight;
				if (newArea.right != farRight) newArea.right = farRight;
				if (newBottomBar.right != farRight) newBottomBar.right = farRight;
			}

			if (this->buttons.contains(elem))
			{
				vsid::Button& btn = this->buttons[elem];

				// adjust the font if different from previous font

				if (lgfont.lfHeight != btn.height || lgfont.lfWeight != btn.weight)
				{
					font.DeleteObject();

					lgfont.lfHeight = btn.height;
					lgfont.lfWeight = btn.weight;
					font.CreateFontIndirectA(&lgfont);

					dc.SelectObject(&font);
				}

				txtSize = dc.GetTextExtent(btn.label.c_str());

				btn.area.right = btn.area.left + txtSize.cx;
				btn.area.bottom = btn.area.top + txtSize.cy;

				messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Updating button. (" + std::to_string(row) +
					"/" + std::to_string(col) + ")", vsid::MessageHandler::DebugArea::Menu);

				// adjust text position based on neighbours

				try
				{
					// adjust text position in rows 

					if (row == 0)
					{
						if (btn.area.top != btn.base.top + btn.margin.top)
						{
							int height = btn.area.bottom - btn.area.top;
							btn.area.top = btn.base.top + btn.margin.top;
							btn.area.bottom = btn.area.top + height;

							messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .top [" +
								std::to_string(btn.area.top) + "] based on main area", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .top [" + std::to_string(btn.area.top) + "] matches main area .top [" +
							std::to_string(btn.base.top) + "] + margin .top [" + std::to_string(btn.margin.top) + "] -> " +
							std::to_string(btn.area.top) + " == " + std::to_string(btn.base.top + btn.margin.top), vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						std::string toTop = this->table.at(row - 1).at(col);

						if (this->texts.contains(toTop))
						{
							if (btn.area.top != this->texts[toTop].area.bottom + btn.margin.top)
							{
								int height = btn.area.bottom - btn.area.top;
								btn.area.top = this->texts[toTop].area.bottom + btn.margin.top;
								btn.area.bottom = btn.area.top + height;

								messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .top [" +
									std::to_string(btn.area.top) + "] based on above \"" + toTop + "\" .bottom [" +
									std::to_string(this->texts[toTop].area.bottom) + "] + margin.top", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .top [" + std::to_string(btn.area.top) + "] matches [" + toTop + "] .bottom [" +
								std::to_string(this->texts[toTop].area.bottom) + "] + margin .top [" + std::to_string(btn.margin.top) + "] -> " +
								std::to_string(btn.area.top) + " == " + std::to_string(this->texts[toTop].area.bottom + btn.margin.top), vsid::MessageHandler::DebugArea::Menu);
						}
						else if (this->buttons.contains(toTop))
						{
							if (btn.area.top != this->buttons[toTop].area.bottom + btn.margin.top)
							{
								int height = btn.area.bottom - btn.area.top;
								btn.area.top = this->buttons[toTop].area.bottom + btn.margin.top;
								btn.area.bottom = btn.area.top + height;

								messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .top [" +
									std::to_string(btn.area.top) + "] based on above \"" + toTop + "\" .bottom [" +
									std::to_string(this->buttons[toTop].area.bottom) + "] + margin.top", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .top [" + std::to_string(btn.area.top) + "] matches [" + toTop + "] .bottom [" +
								std::to_string(this->buttons[toTop].area.bottom) + "] + margin .top [" + std::to_string(btn.margin.top) + "] -> " +
								std::to_string(btn.area.top) + " == " + std::to_string(this->buttons[toTop].area.bottom + btn.margin.top), vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "toTop text/button [" + toTop + "] was not found", vsid::MessageHandler::DebugArea::Menu);
					}

					// adjust text position in columns

					if (col == 0)
					{
						if (btn.area.left != btn.base.left + btn.margin.left)
						{
							int width = btn.area.right - btn.area.left;
							btn.area.left = btn.base.left + btn.margin.left;
							btn.area.right = btn.area.left + width;

							messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .left [" +
								std::to_string(btn.area.left) + "] based on main", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .left [" + std::to_string(btn.area.left) + "] matches base .left [" +
							std::to_string(btn.base.left) + "] + margin.left [" + std::to_string(btn.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
					else
					{
						std::string toLeft = this->table.at(0).at(col - 1);

						if (this->texts.contains(toLeft))
						{
							if (btn.area.left != this->texts[toLeft].area.right + btn.margin.left)
							{
								int width = btn.area.right - btn.area.left;
								btn.area.left = this->texts[toLeft].area.right + btn.margin.left;
								btn.area.right = btn.area.left + width;

								messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .left [" +
									std::to_string(btn.area.left) + "] based on left \"" + toLeft + "\" .right [" +
									std::to_string(this->texts[toLeft].area.right) + " + margin.left", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .left [" + std::to_string(btn.area.left) + "] matches [" + toLeft + "] .left [" +
								std::to_string(btn.base.left) + "] + margin .left [" + std::to_string(btn.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
						}
						else if (this->buttons.contains(toLeft))
						{
							if (btn.area.left != this->buttons[toLeft].area.right + btn.margin.left)
							{
								int width = btn.area.right - btn.area.left;
								btn.area.left = this->buttons[toLeft].area.right + btn.margin.left;
								btn.area.right = btn.area.left + width;

								messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjust to new .left [" +
									std::to_string(btn.area.left) + "] based on left \"" + toLeft + "\" .right [" +
									std::to_string(this->buttons[toLeft].area.right) + "] + margin.left", vsid::MessageHandler::DebugArea::Menu);
							}
							else messageHandler->writeMessage("DEBUG", "[" + btn.title + "] .left [" + std::to_string(btn.area.left) + "] matches [" + toLeft + "] .left [" +
								std::to_string(btn.base.left) + "] + margin .left [" + std::to_string(btn.margin.left) + "]", vsid::MessageHandler::DebugArea::Menu);
						}
						else messageHandler->writeMessage("DEBUG", "toLeft text [" + toLeft + "] was not found in texts", vsid::MessageHandler::DebugArea::Menu);
					}
				}
				catch (std::out_of_range)
				{
					messageHandler->writeMessage("ERROR", "Failed to update [" + this->title + "] when updating button [" +
						btn.title + "]. Code: " + ERROR_MENU_BTNUPDATE);
				}

				// adjust base area right side based on margin

				if (btn.area.right + btn.margin.right > btn.base.right)
				{
					if (btn.base == this->area)
					{
						newArea.right = btn.area.right + btn.margin.right;

						messageHandler->writeMessage("DEBUG", "Adjusting main area to new .right [" +
							std::to_string(newArea.right) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
				}

				// adjust base area bottom side based on margin
				if (btn.area.bottom + btn.margin.bottom > btn.base.bottom)
				{
					if (btn.base == this->area)
					{
						newArea.bottom = btn.area.bottom + btn.margin.bottom;

						messageHandler->writeMessage("DEBUG", "Adjusting main area to new .bottom [" +
							std::to_string(newArea.bottom) + "]", vsid::MessageHandler::DebugArea::Menu);
					}
				}

				// re-adjust new top and bottom bar to new area

				if (newTopBar.bottom != newArea.top)
				{
					int height = newTopBar.bottom - newTopBar.top;

					newTopBar.bottom = newArea.top;
					newTopBar.top = newTopBar.bottom - height;

					messageHandler->writeMessage("DEBUG", "[topBar] Adjusting top bar area. New .bottom [" + std::to_string(newTopBar.bottom) +
						"]", vsid::MessageHandler::DebugArea::Menu);
				}

				if (newBottomBar.top != newArea.bottom)
				{
					int height = newBottomBar.bottom - newBottomBar.top;

					newBottomBar.top = newArea.bottom;
					newBottomBar.bottom = newBottomBar.top + height;

					messageHandler->writeMessage("DEBUG", "[bottomBar] Adjusting bottom bar area. New .top [" + std::to_string(newTopBar.top) +
						"]", vsid::MessageHandler::DebugArea::Menu);
				}

				// move btn area based on margin and re-adjusted base area

				if ((btn.area.top != btn.base.top + btn.margin.top) && row == 0)
				{
					int height = btn.area.bottom - btn.area.top;

					btn.area.top = btn.base.top + btn.margin.top;
					btn.area.bottom = btn.area.top + height;

					messageHandler->writeMessage("DEBUG", "[" + btn.title + "] Adjusting to changed base with new .top [" +
						std::to_string(btn.area.top) + "]", vsid::MessageHandler::DebugArea::Menu);
				}

				// adjust right side of menu to the farthest

				int farRight = std::max({ newTopBar.right, newArea.right, newBottomBar.right });

				if (newTopBar.right != farRight) newTopBar.right = farRight;
				if (newArea.right != farRight) newArea.right = farRight;
				if (newBottomBar.right != farRight) newBottomBar.right = farRight;

			}
		}
	}	

	// update texts in top or bottom bar

	for (auto &[title, txt] : this->texts)
	{
		if (txt.type != MENU_TOP_BAR && txt.type != MENU_BOTTOM_BAR) continue;

		if (lgfont.lfHeight != txt.height || lgfont.lfWeight != txt.weight)
		{
			font.DeleteObject();

			lgfont.lfHeight = txt.height;
			lgfont.lfWeight = txt.weight;
			font.CreateFontIndirectA(&lgfont);

			dc.SelectObject(&font);
		}

		txtSize = dc.GetTextExtent(txt.txt.c_str());

		txt.area.right = txt.area.left + txtSize.cx;
		txt.area.bottom = txt.area.top + txtSize.cy;

		messageHandler->writeMessage("DEBUG", "Updating top/bottom bar text " + txt.title, vsid::MessageHandler::DebugArea::Menu);

		if (txt.type == MENU_TOP_BAR)
		{
			int width = txt.area.right - txt.area.left;
			txt.area.left = newTopBar.left + txt.margin.left;
			txt.area.right = txt.area.left + width;

			// move text up inside base area if too close

			if (txt.area.bottom + txt.margin.bottom > newTopBar.bottom)
			{
				int height = txt.area.bottom - txt.area.top;

				txt.area.bottom = newTopBar.bottom - txt.margin.bottom;
				txt.area.top = txt.area.bottom - height;
			}

			// extend base up

			if (txt.area.top - txt.margin.top < newTopBar.top) newTopBar.top = txt.area.top - txt.margin.top;

			// extend base right 

			if (txt.area.right + txt.margin.right > newTopBar.right) newTopBar.right = txt.area.right + txt.margin.right;

			// re-adjust right side of menu

			if (newTopBar.right != newArea.right)
			{
				newArea.right = newTopBar.right;
				newBottomBar.right = newTopBar.right;
			}

			messageHandler->writeMessage("DEBUG", "[" + title + "] .left: " + std::to_string(txt.area.left) + " .right: " +
				std::to_string(txt.area.right) + " .bottom: " + std::to_string(txt.area.bottom) +
				" .top: " + std::to_string(txt.area.top), vsid::MessageHandler::DebugArea::Menu);

			messageHandler->writeMessage("DEBUG", "[" + title + "]-BASE .left: " + std::to_string(txt.base.left) + " .right: " +
				std::to_string(txt.base.right) + " .bottom: " + std::to_string(txt.base.bottom) +
				" .top: " + std::to_string(txt.base.top), vsid::MessageHandler::DebugArea::Menu);

			txt.base = newTopBar;
		}

		if (txt.type == MENU_BOTTOM_BAR)
		{
			int width = txt.area.right - txt.area.left;
			txt.area.left = newBottomBar.left + txt.margin.left;
			txt.area.right = txt.area.left + width;

			// move text down inside base area if too close

			if (txt.area.top - txt.margin.top < newBottomBar.top)
			{
				int height = txt.area.bottom - txt.area.top;

				txt.area.top = newBottomBar.top + txt.margin.top;
				txt.area.bottom = txt.area.top + height;
			}
			// move text up inside base area if too far
			else if (newBottomBar.bottom < txt.area.bottom + txt.margin.bottom)
			{
				int height = txt.area.bottom - txt.area.top;

				txt.area.top = newBottomBar.top + txt.margin.top;
				txt.area.bottom = txt.area.top + height;
			}

			// extend base down

			if (txt.area.bottom + txt.margin.bottom > newBottomBar.bottom) newBottomBar.bottom = txt.area.bottom + txt.margin.bottom;

			// extend base right 

			if (txt.area.right + txt.margin.right > newBottomBar.right) newBottomBar.right = txt.area.right + txt.margin.right;

			// re-adjust right side of menu

			if (newBottomBar.right != newArea.right)
			{
				newArea.right = newTopBar.right;
				newTopBar.right = newTopBar.right;
			}

			messageHandler->writeMessage("DEBUG", "[" + title + "] .left: " + std::to_string(txt.area.left) + " .right: " +
				std::to_string(txt.area.right) + " .bottom: " + std::to_string(txt.area.bottom) +
				" .top: " + std::to_string(txt.area.top), vsid::MessageHandler::DebugArea::Menu);

			messageHandler->writeMessage("DEBUG", "[" + title + "]-BASE .left: " + std::to_string(txt.base.left) + " .right: " +
				std::to_string(txt.base.right) + " .bottom: " + std::to_string(txt.base.bottom) +
				" .top: " + std::to_string(txt.base.top), vsid::MessageHandler::DebugArea::Menu);

			txt.base = newBottomBar;

		}
	}

	// re-iterate over texts to center top and bottom bar

	for (auto& [title, txt] : this->texts)
	{
		if (txt.type != MENU_BOTTOM_BAR && txt.type != MENU_TOP_BAR) continue;

		if ((txt.area.right + txt.margin.right) - (txt.area.left - txt.margin.left) < (newBottomBar.right - newBottomBar.left))
		{
			txt.area.NormalizeRect();
			int width = txt.area.Width();
			txt.area.left = newBottomBar.right - newBottomBar.Width() / 2 - width / 2;
			txt.area.right = txt.area.left + width;
		}
	}

	// update close button

	if (this->buttons.contains(this->title + "_close"))
	{
		vsid::Button& btn = this->buttons[this->title + "_close"];
		CRect topText;

		for (auto& [title, txt] : this->texts)
		{
			if (txt.type == MENU_TOP_BAR)
			{
				topText = txt.area;
				break;
			}
		}

		int width = btn.area.Width();
		int height = btn.area.Height();

		btn.area.right = newTopBar.right - btn.margin.right;
		btn.area.left = btn.area.right - width;
		btn.area.top = newTopBar.top + btn.margin.top;
		btn.area.bottom = btn.area.top + height;

		if (!topText.IsRectEmpty() && (btn.area.left - btn.margin.left < topText.right))
		{
			btn.area.left = topText.right + btn.margin.left;
			btn.area.right = btn.area.left + width;
		}

		if (btn.area.right + btn.margin.right > newTopBar.right) newTopBar.right = btn.area.right + btn.margin.right;

		// adjust right side of menu to the farthest

		int farRight = std::max({ newTopBar.right, newArea.right, newBottomBar.right });

		if (newTopBar.right != farRight) newTopBar.right = farRight;
		if (newArea.right != farRight) newArea.right = farRight;
		if (newBottomBar.right != farRight) newBottomBar.right = farRight;

		btn.base = newTopBar;
	}

	// final re-iteration over texts to change for new base

	for (auto& [title, txt] : this->texts)
	{
		if (txt.type == MENU_TOP_BAR || txt.type == MENU_BOTTOM_BAR) continue;
		if (txt.base == this->area)	txt.base = newArea;
	}

	// final re-iteration over btns to change for new base

	for (auto& [title, btn] : this->buttons)
	{
		if (btn.type == MENU_TOP_BAR || btn.type == MENU_BOTTOM_BAR) continue;
		if (btn.base == this->area)	btn.base = newArea;
	}

	this->area = newArea;
	this->topBar = newTopBar;
	this->bottomBar = newBottomBar;

	dc.SelectObject(oldFont);
	dc.Detach();
	ReleaseDC(NULL, hDC);
}

void vsid::Menu::move(int type, RECT &Area)
{
	RECT typeArea;
	if (type == MENU) typeArea = this->area;
	else if (type == MENU_TOP_BAR) typeArea = this->topBar;

	int difLeft = Area.left - typeArea.left;
	int difTop = Area.top - typeArea.top;
	int difBot = Area.bottom - typeArea.bottom;
	int difRight = Area.right - typeArea.right;

	if (type == MENU) this->resize(Area.top, Area.right, Area.bottom, Area.left);
	else
	{
		area.left += difLeft;
		area.top += difTop;
		area.right += difRight;
		area.bottom += difBot;
	}

	for (auto& [title, txt] : this->texts)
	{
		txt.area.left += difLeft;
		txt.area.top += difTop;
		txt.area.right += difRight;
		txt.area.bottom += difBot;

		txt.base = this->getArea();
	}

	for (auto& [title, btn] : this->buttons)
	{
		btn.area.left += difLeft;
		btn.area.top += difTop;
		btn.area.right += difRight;
		btn.area.bottom += difBot;

		btn.base = this->getArea();
	}

	// update topBar

	this->topBar.top += difTop;
	this->topBar.right += difRight;
	this->topBar.bottom += difBot;
	this->topBar.left += difLeft;

	// update bottomBar

	this->bottomBar.top += difTop;
	this->bottomBar.right += difRight;
	this->bottomBar.bottom += difBot;
	this->bottomBar.left += difLeft;
}
