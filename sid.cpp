#include "pch.h"
#include "sid.h"
#include "utils.h"

std::string vsid::Sid::name() const
{
	return (this->empty()) ? "" : this->base + this->number + this->designator;
}

std::string vsid::Sid::idName() const
{
	return (this->empty()) ? "" : this->base + this->number + this->designator + " (ID: " + this->id + ")";
}

std::string vsid::Sid::getRwy() const
{
	return vsid::utils::split(this->rwy, ',').at(0);
}

bool vsid::Sid::empty() const
{
	return base == "";
}

bool vsid::Sid::operator==(const Sid& sid)
{
	if (this->designator != "")
	{
		if (this->base == sid.base &&
			this->number == sid.number &&
			this->designator == sid.designator
			)
		{
			return true;
		}
		else return false;
	}
	else return this->base == sid.base;
}

bool vsid::Sid::operator!=(const Sid& sid)
{
	if (this->designator != "")
	{
		if (this->base != sid.base ||
			this->number != sid.number ||
			this->designator != sid.designator
			)
		{
			return true;
		}
		else return false;
	}
	else return this->base != sid.base;
}
