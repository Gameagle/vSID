#include "pch.h"

#include <regex>
#include <format>
#include <windows.h>

#include "versionchecker.h"
#include "messageHandler.h"
#include "logger.h"
#include "include/nlohmann/json.hpp"

using json = nlohmann::ordered_json;

std::optional<vsid::version::semver> vsid::version::parseSemVer(std::string s)
{
	if (s.length() == 0) return std::nullopt;

	if (s[0] == 'v' || s[0] == 'V') s.erase(0, 1);

	std::smatch m;
	if (std::regex_search(s, m, std::regex(R"(^\s*(\d+)\.(\d+)\.(\d+)\-(\S+))")))
	{
		return std::make_tuple(std::stoi(m[1]), std::stoi(m[2]), std::stoi(m[3]), m[4]);
	}
	else if (std::regex_search(s, m, std::regex(R"(^\s*(\d+)\.(\d+)\.(\d+))")))
	{
		return std::make_tuple(std::stoi(m[1]), std::stoi(m[2]), std::stoi(m[3]), std::nullopt);
	}
	return std::nullopt;
}

std::string vsid::version::semverToString(const vsid::version::semver& v)
{
	auto& [major, minor, patch, hash] = v;

	std::string stringVer = std::format("{}.{}.{}", major, minor, patch);

	if (hash) stringVer.append("-" + hash.value());

	return stringVer;
}

int vsid::version::compSemVer(const vsid::version::semver& local, const vsid::version::semver& remote)
{
	// get base version (major, minor, patch)
	auto baseLocal = std::tie(std::get<0>(local), std::get<1>(local), std::get<2>(local));
	auto baseRemote = std::tie(std::get<0>(remote), std::get<1>(remote), std::get<2>(remote));

	bool remoteIsStable = !std::get<3>(remote);
	bool localIsStable = !std::get<3>(local);

	if (baseRemote > baseLocal) return remoteIsStable ? 1 : 2; // remote is newer, 1 = stable, 2 = pre-release

	if (baseRemote == baseLocal)
	{
		if (!remoteIsStable && !localIsStable)
		{
			if (std::get<3>(remote) != std::get<3>(local)) return 2; // assumed new pre-release
		}
		else if (remoteIsStable && !localIsStable)
			return 1; // new stable release
	}

	return 0; // default state - no update
}

std::optional<std::string> vsid::version::getHttp(bool prerelease)
{
	CURL* curl = curl_easy_init();
	if (!curl) return std::nullopt;

	std::string response;
	std::string url = "https://api.github.com/repos/Gameagle/vsid/releases";
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "vsid/1.0");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vsid::version::writeCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

	CURLcode res = curl_easy_perform(curl);
	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK || httpCode < 200 || httpCode >= 300)
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Curl error [{}] - Curl Code [{}] - Http Code [{}]", curl_easy_strerror(res), std::to_string(res), httpCode));
		return std::nullopt;
	}
	return response;
}

std::optional<std::string> vsid::version::getRelease(bool prerelease)
{
	std::optional<std::string> body = getHttp(prerelease);
	if (!body) return std::nullopt;

	try
	{
		auto j = json::parse(*body);

		for (const auto& r : j)
		{
			if (!prerelease && r.value("prerelease", false)) continue;

			if (std::string tag = r.value("tag_name", ""); !tag.empty()) return tag;
			else return std::nullopt;
		}

		
	}
	catch (json::parse_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to parse github version: {}", e.what()));
		return std::nullopt;
	}

	return std::nullopt;
}

void vsid::version::checkForUpdates(int notify, const std::optional<vsid::version::semver>& localVersion)
{
	auto ghv = parseSemVer(getRelease((notify >= 3 && notify <= 4)).value_or(""));

	if (!localVersion)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to get local version. Value was empty");
		return;
	}
	if (!ghv)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to get github version. Value was empty");
		return;
	}
	else
	{
		vsid::Logger::log(vsid::LogLevel::Debug, std::format("Github version [{}]",
			(ghv.has_value() ? vsid::version::semverToString(ghv.value()) : " Failed to parse version.")), vsid::DebugLevel::Dev);
	}

	int comp = vsid::version::compSemVer(localVersion.value(), ghv.value());

	if (comp == 0)
	{
		vsid::Logger::log(vsid::LogLevel::Debug, "No new version found on github", vsid::DebugLevel::Dev);
		return;
	}

	switch (notify)
	{
	case 0:
		return;
	case 1:
		if (comp == 2)
		{
			vsid::Logger::log(vsid::LogLevel::Debug, "New pre-release version found but notifying only for stable releases.", vsid::DebugLevel::Dev);
			break;
		}

		vsid::Logger::log(vsid::LogLevel::Info, std::format("New Update available. Local version [{}] | Github version [{}]",
			vsid::version::semverToString(localVersion.value()), vsid::version::semverToString(ghv.value())));

		break;
	case 2:
		if (comp == 2)
		{
			vsid::Logger::log(vsid::LogLevel::Debug, "New pre-release version found but notifying only for stable releases.", vsid::DebugLevel::Dev);
			break;
		}
		goto showmsg;
	case 3:
		vsid::Logger::log(vsid::LogLevel::Info, std::format("New Update (pre-release) available. Local version: {} | Github version: {}", 
			vsid::version::semverToString(localVersion.value()), vsid::version::semverToString(ghv.value())));

		break;
	case 4:
		goto showmsg;
	showmsg:
		MessageBoxA(
			nullptr,
			std::string(std::format("New version: {}\nInstalled: {}", vsid::version::semverToString(ghv.value()), vsid::version::semverToString(localVersion.value()))).c_str(),
			"vSID Update available!",
			MB_OK | MB_ICONINFORMATION
		);

		break;
	default:
		vsid::Logger::log(vsid::LogLevel::Warning, std::format("Didn't check for updates. Improper notification value in main config ({}). Should be 0 - 4.", notify));
	}
}
