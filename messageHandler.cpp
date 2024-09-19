#include "pch.h"

#include "messageHandler.h"
#include "timeHandler.h"

#include <sstream>
#include <iostream> // dev

vsid::MessageHandler::MessageHandler() {}

vsid::MessageHandler::~MessageHandler() { this->closeConsole(); }

void vsid::MessageHandler::writeMessage(std::string sender, std::string msg, DebugArea debugArea)
{
	if (sender == "DEBUG" && this->getLevel() != Level::Debug) return;

	try
	{
		if (this->currentLevel == Level::Debug && (this->debugArea == debugArea || this->debugArea == DebugArea::All) && sender == "DEBUG")
		{
			std::string area;
			if (debugArea == DebugArea::Atc) area = "ATC";
			else if (debugArea == DebugArea::Conf) area = "CONF";
			else if (debugArea == DebugArea::Dev) area = "DEV";
			else if (debugArea == DebugArea::Req) area = "REQ";
			else if (debugArea == DebugArea::Rwy) area = "RWY";
			else if (debugArea == DebugArea::Sid) area = "SID";
			else if (debugArea == DebugArea::Menu) area = "MENU";
			else area = "N/A";

			std::cout << "[" << vsid::time::toTimeString(vsid::time::getUtcNow()) << "] [" << area << "] " << msg << '\n';
		}
		else if (/*this->currentLevel != Level::Debug && */sender != "DEBUG") this->msg.push_back(std::pair<std::string, std::string>(sender, msg));
	}
	catch (std::exception& e)
	{
		this->msg.push_back(std::pair<std::string, std::string>("ERROR", e.what()));
	}
	
}

std::pair<std::string, std::string> vsid::MessageHandler::getMessage()
{
	if (this->msg.size() > 0)
	{
		std::pair<std::string, std::string> tmp = { this->msg.front().first, this->msg.front().second };
		this->msg.erase(this->msg.begin());
		return tmp;
	}
	else
	{
		return { "", "" };
	}
}

void vsid::MessageHandler::dropMessage()
{
	this->msg.clear();
}

void vsid::MessageHandler::openConsole()
{
	try
	{
		AllocConsole();
		freopen_s(&this->consoleFile, "CONOUT$", "w", stdout);
	}
	catch (std::exception& e)
	{
		this->msg.push_back(std::pair<std::string, std::string>("ERROR", e.what()));
	}
}

void vsid::MessageHandler::closeConsole()
{
	if (this->consoleFile != NULL)
	{
		try
		{
			fclose(this->consoleFile);
			FreeConsole();
		}
		catch (std::exception& e)
		{
			this->msg.push_back(std::pair<std::string, std::string>("ERROR", e.what()));
		}
	}	
}

vsid::MessageHandler::Level vsid::MessageHandler::getLevel() const
{
	return this->currentLevel;
}

void vsid::MessageHandler::setLevel(std::string lvl)
{
	if (lvl == "DEBUG")
	{
		this->currentLevel = Level::Debug;
		this->openConsole();
	}
	else if (lvl == "INFO")
	{
		this->currentLevel = Level::Info;
		this->closeConsole();
	}
}

bool vsid::MessageHandler::setDebugArea(std::string debugArea)
{

	if (debugArea == "ALL")
	{
		this->debugArea = DebugArea::All;
		return true;
	}
	else if (debugArea == "SID") { 
		this->debugArea = DebugArea::Sid;
		return true;
	}
	else if (debugArea == "RWY")
	{
		this->debugArea = DebugArea::Rwy;
		return true;
	}
	else if (debugArea == "ATC")
	{
		this->debugArea = DebugArea::Atc;
		return true;
	}
	else if (debugArea == "DEV")
	{
		this->debugArea = DebugArea::Dev;
		return true;
	}
	else if (debugArea == "REQ")
	{
		this->debugArea = DebugArea::Req;
		return true;
	}
	else if (debugArea == "CONF")
	{
		this->debugArea = DebugArea::Conf;
		return true;
	}
	else if (debugArea == "MENU")
	{
		this->debugArea = DebugArea::Menu;
		return true;
	}
	else return false;
}

std::unique_ptr<vsid::MessageHandler> vsid::messageHandler(new vsid::MessageHandler()); // declaration
