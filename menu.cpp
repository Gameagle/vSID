#include "pch.h"

#include "menu.h"

vsid::Menu::Menu(int type, std::string title, int top, int left, int minWidth, int minHeight, bool render,
				COLORREF border, COLORREF bg)
{
	this->type = type;
	this->title = title;
	this->render = render;
	this->border = border;
	this->bg = bg;

	this->area.top = top;
	this->area.left = left;
	this->area.right = left + minWidth;
	this->area.bottom = top + minHeight;

	messageHandler->writeMessage("DEBUG", "Creating menu " + title, vsid::MessageHandler::DebugArea::Menu);
}

void vsid::Menu::addText(int type, std::string title, std::string txt, int topPad, int leftPad, int minWidth, int height, int weight,
	bool render, std::string relElem, COLORREF textColor, COLORREF bg)
{
	if (this->texts.contains(title))
	{
		messageHandler->writeMessage("ERROR", "Trying to add text \"" + title + "\" to menu \"" + this->title + "\". Report as an error.");
		return;
	}

	messageHandler->writeMessage("DEBUG", "Creating new text \"" + title + "\"", vsid::MessageHandler::DebugArea::Menu);

	vsid::Menu::Text newText;

	// initialize values

	newText.type = type;
	newText.title = title;
	newText.txt = txt;
	newText.render = render;
	newText.textColor = textColor;
	newText.bg = bg;
	newText.height = height;
	newText.weight = weight;

	// setting text area
	if (relElem != "" && this->texts.contains(relElem))
	{
		(topPad == 0) ? newText.area.top = this->texts[relElem].area.top : newText.area.top = this->texts[relElem].area.bottom + topPad;
		(leftPad == 0) ? newText.area.left = this->texts[relElem].area.left : newText.area.left = this->texts[relElem].area.right + leftPad;
	}
	else if (relElem != "" && this->buttons.contains(relElem))
	{
		(topPad == 0) ? newText.area.top = this->buttons[relElem].area.top : newText.area.top = this->buttons[relElem].area.bottom + topPad;
		(leftPad == 0) ? newText.area.left = this->buttons[relElem].area.left : newText.area.left = this->buttons[relElem].area.right + leftPad;
	}
	else
	{
		newText.area.top = this->area.top + topPad;
		newText.area.left = this->area.left + leftPad;
	}

	// resizing width if text is greater then minWidth
	(txt.size() >= minWidth) ? newText.area.right = newText.area.left + txt.size() : newText.area.right = newText.area.left + minWidth;

	newText.area.bottom = newText.area.top + height;

	this->texts.insert({ title, newText });
}

void vsid::Menu::resize(int top, int left, int right, int bottom)
{
	this->area.top = top;
	this->area.left = left;
	this->area.right = right;
	this->area.bottom = bottom;
}

void vsid::Menu::update()
{
	messageHandler->writeMessage("DEBUG", "Menu width before update: " + std::to_string(this->area.right - this->area.left), vsid::MessageHandler::DebugArea::Menu);
	messageHandler->writeMessage("DEBUG", "Menu heigth before update: " + std::to_string(this->area.bottom - this->area.top), vsid::MessageHandler::DebugArea::Menu);
	for (auto& [title, txt] : this->texts)
	{
		if (txt.area.right >= this->area.right) this->area.right = txt.area.right + 10;
		if (txt.area.bottom >= this->area.bottom) this->area.bottom = txt.area.bottom + 10;
	}

	for (auto& [title, btn] : this->buttons)
	{
		if (btn.area.right >= this->area.right) this->area.right = btn.area.right + 10;
		if (btn.area.bottom >= this->area.bottom) this->area.bottom = btn.area.bottom + 10;
	}
	messageHandler->writeMessage("DEBUG", "Menu width after update: " + std::to_string(this->area.right - this->area.left), vsid::MessageHandler::DebugArea::Menu);
	messageHandler->writeMessage("DEBUG", "Menu heigth after update: " + std::to_string(this->area.bottom - this->area.top), vsid::MessageHandler::DebugArea::Menu);
}
