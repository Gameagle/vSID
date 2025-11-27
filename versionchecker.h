/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flight plans.

Copyright (C) 2024 Gameagle (Philip Maier)
Repo @ https://github.com/Gameagle/vSID

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <curl/curl.h>
#include <string>
#include <optional>
#include <tuple>

namespace vsid
{
	namespace version
	{
		using semver = std::tuple<int, int, int, std::optional<std::string>>;

		//************************************
		// Description: Transform version string to tuple
		// Method:    parseSemVer
		// FullName:  vsid::version::parseSemVer
		// Access:    public 
		// Returns:   std::optional<vsid::version::semver>
		// Qualifier:
		// Parameter: std::string s
		//************************************
		std::optional<semver> parseSemVer(std::string s);

		//************************************
		// Description: Transform version tuple into string
		// Method:    semverToString
		// FullName:  vsid::version::semverToString
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: const semver & v
		//************************************
		std::string semverToString(const vsid::version::semver& v);

		//************************************
		// Description: Compare two versions (tuple)
		// Method:    compSemVer
		// FullName:  vsid::version::compSemVer
		// Access:    public 
		// Returns:   int
		// Qualifier:
		// Parameter: const semver & local
		// Parameter: const semver & remote
		//************************************
		int compSemVer(const semver& local, const semver& remote);

		//************************************
		// Get github releases as http response
		// Method:    getHttp
		// FullName:  vsid::version::getHttp
		// Access:    public 
		// Returns:   std::optional<std::string>
		// Qualifier:
		// Parameter: bool prerelease - if pre-releases should be considered
		//************************************
		std::optional<std::string> getHttp(bool prerelease);

		//************************************
		// Description: Extract version information from github response
		// Method:    getRelease
		// FullName:  vsid::version::getRelease
		// Access:    public 
		// Returns:   std::optional<std::string>
		// Qualifier:
		// Parameter: bool prerelease - if pre-releases should be considered
		//************************************
		std::optional<std::string> getRelease(bool prerelease);

		//************************************
		// Description: Checks for updates and informs user via text or msg box
		// Method:    checkForUpdates
		// FullName:  vsid::version::checkForUpdates
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: int notify - how to notify user (0 - 4)
		// Parameter: const std::optional<semver> & localVersion
		//************************************
		void checkForUpdates(int notify, const std::optional<semver>& localVersion);

		//************************************
		// Description: Callback required as per libcurl
		// Method:    writeCb
		// FullName:  vsid::version::writeCb
		// Access:    public static 
		// Returns:   size_t
		// Qualifier:
		// Parameter: void * contents
		// Parameter: size_t size
		// Parameter: size_t nmemb
		// Parameter: void * userp
		//************************************
		inline static size_t writeCb(void* contents, size_t size, size_t nmemb, void* userp)
		{
			size_t total = size * nmemb;
			static_cast<std::string*>(userp)->append((char*)contents, total);

			return total;
		}
	}	
}

