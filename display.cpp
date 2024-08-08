#include "pch.h"

#include "display.h"

vsid::Display::Display(int id, vsid::VSIDPlugin *plugin) : EuroScopePlugIn::CRadarScreen()
{ 
	this->id = id;
	this->plugin = plugin; 
}
vsid::Display::~Display() { }
