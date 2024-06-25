#include "pch.h"
#include "sid.h"
#include "utils.h"

std::string vsid::Sid::name() const
{
	return (this->empty()) ? "" : this->waypoint + this->number + this->designator;
}

std::string vsid::Sid::idName() const
{
	return (this->empty()) ? "" : this->waypoint + this->number + this->designator + " (ID: " + this->id + ")";
}

std::string vsid::Sid::getRwy() const
{
	return vsid::utils::split(this->rwy, ',').at(0);
}

bool vsid::Sid::empty() const
{
	return waypoint == "";
}

bool vsid::Sid::operator==(const Sid& sid)
{
	if (this->waypoint == sid.waypoint &&
		this->number == sid.number &&
		this->designator == sid.designator
		)
	{
		return true;
	}
	else return false;
}

bool vsid::Sid::operator!=(const Sid& sid)
{
	if (this->waypoint != sid.waypoint ||
		this->number != sid.number ||
		this->designator != sid.designator
		)
	{
		return true;
	}
	else return false;
}
