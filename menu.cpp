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

	vsid::Logger::log(vsid::LogLevel::Debug, std::format("Creating menu [{}]", title), vsid::DebugLevel::Menu, true);
}

void vsid::Menu::addText(int type, std::string title, const CRect &base, std::string txt, int minWidth, int height, int weight, Spacing margin,
	std::string relElem, bool render, COLORREF textColor, COLORREF bg)
{
	if (this->texts.contains(title))
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Trying to add text [{}] to menu [{}]. Report as an error. Code: {}",
			title, this->title, ERROR_MEN_TXTADD), vsid::DebugLevel::Menu);
		return;
	}

	vsid::Logger::log(vsid::LogLevel::Debug, std::format("Creating new text [{}]", title), vsid::DebugLevel::Menu, true);

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

		vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text [{}] maxRow: {} - maxCol: {}", title, maxRow, maxCol), vsid::DebugLevel::Menu, true);

		try
		{
			auto [row, col] = newText.tablePos;

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text [{}] row: {} - col: {}", title, row, col), vsid::DebugLevel::Menu, true);

			if (row == 0)
			{
				newText.area.top = base.top + newText.margin.top;

				if (col == 0) newText.area.left = base.left + newText.margin.left;
				else
				{
					std::string onLeft = this->table.at(0).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text [{}] in row 0 left neighbor [{}] contained in table",
							title, onLeft), vsid::DebugLevel::Menu, true);
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

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text [{}] .top [{}] based on [{}] .bottom: [{}]",
						title, newText.area.top, onTop, this->texts[onTop].area.bottom), vsid::DebugLevel::Menu, true);
				}

				if (col == 0) newText.area.left = base.left + newText.margin.left;
				else
				{
					std::string onLeft = this->table.at(row).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text [{}] left neighbor [{}] contained in table",
							title, onLeft), vsid::DebugLevel::Menu, true);
						newText.area.left = this->texts[onLeft].area.left + newText.margin.left;
					}
				}
			}
		}
		catch (std::out_of_range)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Tried to access an invalid table position in menu [{}] "
				"working on new text [{}]. Code: {}",
				this->title, newText.title, ERROR_MENU_TXTINVTABPOS));
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

	vsid::Logger::log(vsid::LogLevel::Error, std::format("Trying to add text [{}] of invalid type to menu [{}]. Code: {}",
		title, this->title, ERROR_MEN_TXTINVTYPE), vsid::DebugLevel::Menu);
}

void vsid::Menu::removeText(const std::string& title)
{
	if (this->texts.contains(title))
	{
		vsid::Logger::log(vsid::LogLevel::Debug, std::format("Removing text [{}] from menu [{}]", title, this->title), vsid::DebugLevel::Menu, true);

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
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Trying to add button [{}] to menu [{}]. Report as an error. Code: {}",
			title, this->title, ERROR_MENU_BTNADD));
		return;
	}

	vsid::Logger::log(vsid::LogLevel::Debug, std::format("Creating new button [{}]", title), vsid::DebugLevel::Menu, true);

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

		vsid::Logger::log(vsid::LogLevel::Debug, std::format("Button [{}] maxRow: {} - maxCol: {}",
			title, std::to_string(maxRow), std::to_string(maxCol)), vsid::DebugLevel::Menu, true );

		try
		{
			auto [row, col] = newButton.tablePos;

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("Button [{}] row: {} - col: {}",
				title, std::to_string(row), std::to_string(col)), vsid::DebugLevel::Menu, true);

			if (row == 0)
			{
				newButton.area.top = base.top + newButton.margin.top;

				if (col == 0) newButton.area.left = base.left + newButton.margin.left;
				else
				{
					std::string onLeft = this->table.at(0).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Button [{}] in row 0 left neighbor [{}] contained in table",
							title, onLeft), vsid::DebugLevel::Menu, true);

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

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Button [{}] .top [{}] based on [{}] .bottom [{}]",
						title, std::to_string(newButton.area.top), onTop, std::to_string(this->texts[onTop].area.bottom)), vsid::DebugLevel::Menu, true);
				}

				if (col == 0) newButton.area.left = base.left + newButton.margin.left;
				else
				{
					std::string onLeft = this->table.at(row).at(col - 1);

					if (this->texts.contains(onLeft))
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Button [{}] left neighbor [{}] contained in table", title, onLeft), vsid::DebugLevel::Menu, true);

						newButton.area.left = this->texts[onLeft].area.left + newButton.margin.left;
					}
				}
			}
		}
		catch (std::out_of_range)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Tried to access an invalid table position in menu [{}] working on new text [{}]. Code: {}",
				this->title, newButton.title, ERROR_MENU_BTNINVTABPOS));
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
	else vsid::Logger::log(vsid::LogLevel::Error, std::format("Trying to add button [{}] of invalid type to menu [{}]. Code: {}",
		title, this->title, ERROR_MEN_BTNINVTYPE));
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
	vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Updating menu", this->title), vsid::DebugLevel::Menu);

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
			vsid::Logger::log(vsid::LogLevel::Debug, std::format("Text table [{}][{}]", std::to_string(row), std::to_string(col)), vsid::DebugLevel::Menu, true);

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

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("depcount update - txt size x[{}] y[{}]",
						std::to_string(txtSize.cx), std::to_string(txtSize.cy)), vsid::DebugLevel::Menu, true);
				}
				else txtSize = dc.GetTextExtent(txt.txt.c_str());

				//txtSize = dc.GetTextExtent(txt.txt.c_str());

				txt.area.right = txt.area.left + txtSize.cx;
				txt.area.bottom = txt.area.top + txtSize.cy;

				// auto [row, col] = txt.tablePos;

				vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Updating text.", txt.title), vsid::DebugLevel::Menu);

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

							vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}] based on main area",
								txt.title, std::to_string(txt.area.top)), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches main area .top [{}] + margin .top [{}] -> {} == {}",
							txt.title, txt.area.top, txt.base.top, txt.margin.top, txt.area.top, txt.base.top + txt.margin.top), vsid::DebugLevel::Menu, true);
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

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}] based on above [{}] .bottom [{}] + margin.top",
									txt.title, txt.area.top, toTop, this->texts[toTop].area.bottom), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches [{}] .bottom [{}] + margin .top [{}] -> {} == {}",
								txt.title, txt.area.top, toTop, this->texts[toTop].area.bottom, txt.margin.top,
								txt.area.top, this->texts[toTop].area.bottom + txt.margin.top), vsid::DebugLevel::Menu, true);
						}
						else if (this->buttons.contains(toTop))
						{
							if (txt.area.top != this->buttons[toTop].area.bottom + txt.margin.top)
							{
								int height = txt.area.bottom - txt.area.top;
								txt.area.top = this->buttons[toTop].area.bottom + txt.margin.top;
								txt.area.bottom = txt.area.top + height;

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}] based on above [{}] .bottom [{}] + margin.top",
									txt.title, txt.area.top, toTop, this->buttons[toTop].area.bottom), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches [{}] .bottom [{}] + margin .top [{}] -> {} == {}",
								txt.title, txt.area.top, toTop, this->buttons[toTop].area.bottom, txt.margin.top,
								txt.area.top, this->buttons[toTop].area.bottom + txt.margin.top), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] toTop text/button [{}] was not found.", txt.title, toTop), vsid::DebugLevel::Menu, true);
					}

					// adjust text position in columns

					if (col == 0)
					{
						if (txt.area.left != txt.base.left + txt.margin.left)
						{
							int width = txt.area.right - txt.area.left;
							txt.area.left = txt.base.left + txt.margin.left;
							txt.area.right = txt.area.left + width;

							vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on main",
								txt.title, txt.area.left), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches base .left [{}] + margin.left [{}]",
							txt.title, txt.area.left, txt.base.left, txt.margin.left), vsid::DebugLevel::Menu, true);
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

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on left [{}] .right [{}] + margin.left [{}]",
									txt.title, txt.area.left, toLeft, this->texts[toLeft].area.right, txt.margin.left), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches [{}] .left [{}] + margin.left [{}]",
								txt.title, txt.area.left, toLeft, txt.base.left, txt.margin.left), vsid::DebugLevel::Menu, true);
						}
						else if (this->buttons.contains(toLeft))
						{
							if (txt.area.left != this->buttons[toLeft].area.right + txt.margin.left)
							{
								int width = txt.area.right - txt.area.left;
								txt.area.left = this->buttons[toLeft].area.right + txt.margin.left;
								txt.area.right = txt.area.left + width;

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on left [{}] .right [{}] + margin.left [{}]",
									txt.title, txt.area.left, toLeft, this->buttons[toLeft].area.right, txt.margin.left), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches [{}] .left [{}] + margin.left [{}]",
								txt.title, txt.area.left, toLeft, txt.base.left, txt.margin.left), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("toLeft text/button [{}] was not found", toLeft), vsid::DebugLevel::Menu, true);
					}
				}
				catch (std::out_of_range)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to update [{}] when updating text [{}]. Code: {}",
						this->title, txt.title, ERROR_MENU_TXTUPDATE));
				}

				// adjust base area right side based on margin

				if (txt.area.right + txt.margin.right > txt.base.right)
				{
					if (txt.base == this->area)
					{
						newArea.right = txt.area.right + txt.margin.right;

						vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjusting main area to new .right [{}]",
							txt.title, std::to_string(newArea.right)), vsid::DebugLevel::Menu, true);
					}
				}

				// adjust base area bottom side based on margin

				if (txt.area.bottom + txt.margin.bottom > txt.base.bottom)
				{
					if (txt.base == this->area)
					{
						newArea.bottom = txt.area.bottom + txt.margin.bottom;

						vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjusting main area to new .bottom [{}]",
							txt.title, newArea.bottom), vsid::DebugLevel::Menu, true);
					}
					else
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] base area mismatch, base.bottom smaller", txt.title), vsid::DebugLevel::Menu, true);
					}
				}
				else if (txt.base.bottom > txt.area.bottom + txt.margin.bottom)
				{
					if (txt.base == this->area)
					{
						newArea.bottom = txt.area.bottom + txt.margin.bottom;

						vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjusting main area to new .bottom [{}]) (reducing base area)",
							txt.title, newArea.bottom), vsid::DebugLevel::Menu, true);
					}
					else
					{
						vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] base area mismatch, base.bottom greater", txt.title), vsid::DebugLevel::Menu, true);
					}
				}

				// re-adjust new top and bottom bar to new area

				if (newTopBar.bottom != newArea.top)
				{
					int height = newTopBar.bottom - newTopBar.top;

					newTopBar.bottom = newArea.top;
					newTopBar.top = newTopBar.bottom - height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting top bar area. New .bottom[{}]",
						newTopBar.bottom), vsid::DebugLevel::Menu, true);
				}

				if (newBottomBar.top != newArea.bottom)
				{
					int height = newBottomBar.bottom - newBottomBar.top;

					newBottomBar.top = newArea.bottom;
					newBottomBar.bottom = newBottomBar.top + height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting bottom bar area. New .top [{}]",
						newTopBar.top), vsid::DebugLevel::Menu, true);
				}

				// move txt area based on margin and re-adjusted base area

				if ((txt.area.top != txt.base.top + txt.margin.top) && row == 0)
				{
					int height = txt.area.bottom - txt.area.top;

					txt.area.top = txt.base.top + txt.margin.top;
					txt.area.bottom = txt.area.top + height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjusting to changed base with new .top [{}]",
						txt.title, txt.area.top), vsid::DebugLevel::Menu, true);
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

				vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Updating button. row[{}] col[{}]", btn.title, row, col), vsid::DebugLevel::Menu, true);

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

							vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}]",
								btn.title, btn.area.top), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches main area .top [{}] + margin .top [{}] -> {} == {}",
							btn.title, btn.area.top, btn.base.top, btn.margin.top,
							btn.area.top, btn.base.top + btn.margin.top), vsid::DebugLevel::Menu, true);
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

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}] based on above [{}] .bottom [{}] + margin.top [{}]",
									btn.title, btn.area.top, toTop, this->texts[toTop].area.bottom, btn.margin.top), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches [{}] .bottom [{}] + margin .top [{}] -> {} == {}",
								btn.title, btn.area.top, toTop, this->texts[toTop].area.bottom, btn.margin.top,
								btn.area.top, this->texts[toTop].area.bottom + btn.margin.top), vsid::DebugLevel::Menu, true);
						}
						else if (this->buttons.contains(toTop))
						{
							if (btn.area.top != this->buttons[toTop].area.bottom + btn.margin.top)
							{
								int height = btn.area.bottom - btn.area.top;
								btn.area.top = this->buttons[toTop].area.bottom + btn.margin.top;
								btn.area.bottom = btn.area.top + height;

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .top [{}] based on above [{}] .bottom [{}] + margin.top [{}]",
									btn.title, btn.area.top, toTop, this->buttons[toTop].area.bottom, btn.margin.top), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .top [{}] matches [{}] .bottom [{}] + margin .top [{}] -> {} == {}",
								btn.title, btn.area.top, toTop, this->buttons[toTop].area.bottom, btn.margin.top,
								btn.area.top, this->buttons[toTop].area.bottom + btn.margin.top), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("toTop text/button [{}] was not found", toTop), vsid::DebugLevel::Menu, true);
					}

					// adjust text position in columns

					if (col == 0)
					{
						if (btn.area.left != btn.base.left + btn.margin.left)
						{
							int width = btn.area.right - btn.area.left;
							btn.area.left = btn.base.left + btn.margin.left;
							btn.area.right = btn.area.left + width;

							vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on main",
								btn.title, btn.area.left), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches base .left [{}] + margin.left [{}]",
							btn.title, btn.area.left, btn.base.left, btn.margin.left), vsid::DebugLevel::Menu, true);
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

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on left [{}] .right [{}] + margin.left [{}]",
									btn.title, btn.area.left, toLeft, this->texts[toLeft].area.right, btn.margin.left), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches [{}] .left [{}] + margin.left [{}]",
								btn.title, btn.area.left, toLeft, btn.base.left, btn.margin.left), vsid::DebugLevel::Menu, true);
						}
						else if (this->buttons.contains(toLeft))
						{
							if (btn.area.left != this->buttons[toLeft].area.right + btn.margin.left)
							{
								int width = btn.area.right - btn.area.left;
								btn.area.left = this->buttons[toLeft].area.right + btn.margin.left;
								btn.area.right = btn.area.left + width;

								vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] Adjust to new .left [{}] based on left [{}] .right [{}] + margin.left [{}]",
									btn.title, btn.area.left, toLeft, this->buttons[toLeft].area.right, btn.margin.left), vsid::DebugLevel::Menu, true);
							}
							else vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left [{}] matches [{}] .left [{}] + margin.left [{}]",
								btn.title, btn.area.left, toLeft, btn.base.left, btn.margin.left), vsid::DebugLevel::Menu, true);
						}
						else vsid::Logger::log(vsid::LogLevel::Debug, std::format("toLeft text/button [{}] was not found in texts or buttons", toLeft), vsid::DebugLevel::Menu, true);
					}
				}
				catch (std::out_of_range)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to update [{}] when updating button [{}]. Code: {}",
						this->title, btn.title, ERROR_MENU_BTNUPDATE));
				}

				// adjust base area right side based on margin

				if (btn.area.right + btn.margin.right > btn.base.right)
				{
					if (btn.base == this->area)
					{
						newArea.right = btn.area.right + btn.margin.right;

						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting main area to new .right [{}]",
							newArea.right), vsid::DebugLevel::Menu, true);
					}
				}

				// adjust base area bottom side based on margin
				if (btn.area.bottom + btn.margin.bottom > btn.base.bottom)
				{
					if (btn.base == this->area)
					{
						newArea.bottom = btn.area.bottom + btn.margin.bottom;

						vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting main area to new .bottom [{}]",
							newArea.bottom), vsid::DebugLevel::Menu, true);
					}
				}

				// re-adjust new top and bottom bar to new area

				if (newTopBar.bottom != newArea.top)
				{
					int height = newTopBar.bottom - newTopBar.top;

					newTopBar.bottom = newArea.top;
					newTopBar.top = newTopBar.bottom - height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting top bar area. New .bottom [{}]",
						newTopBar.bottom), vsid::DebugLevel::Menu, true);
				}

				if (newBottomBar.top != newArea.bottom)
				{
					int height = newBottomBar.bottom - newBottomBar.top;

					newBottomBar.top = newArea.bottom;
					newBottomBar.bottom = newBottomBar.top + height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("Adjusting bottom bar area. New .top [{}]",
						newTopBar.top), vsid::DebugLevel::Menu, true);
				}

				// move btn area based on margin and re-adjusted base area

				if ((btn.area.top != btn.base.top + btn.margin.top) && row == 0)
				{
					int height = btn.area.bottom - btn.area.top;

					btn.area.top = btn.base.top + btn.margin.top;
					btn.area.bottom = btn.area.top + height;

					vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}]] Adjusting to changed base with new .top [{}]",
						btn.title, btn.area.top), vsid::DebugLevel::Menu, true);
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

		vsid::Logger::log(vsid::LogLevel::Debug, std::format("Updating top/bottom bar text {}", txt.title), vsid::DebugLevel::Menu, true);

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

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left: {} .right: {} .bottom: {} .top: {}",
				title, txt.area.left, txt.area.right, txt.area.bottom, txt.area.top), vsid::DebugLevel::Menu, true);

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}]]-BASE .left: {} .right: {} .bottom: {} .top: {}",
				title, txt.base.left, txt.base.right, txt.base.bottom, txt.base.top), vsid::DebugLevel::Menu, true);

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

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] .left: {} .right: {} .bottom: {} .top: {}",
				title, txt.area.left, txt.area.right, txt.area.bottom, txt.area.top), vsid::DebugLevel::Menu, true);

			vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}]]-BASE .left: {} .right: {} .bottom: {} .top: {}",
				title, txt.base.left, txt.base.right, txt.base.bottom, txt.base.top), vsid::DebugLevel::Menu, true);

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
