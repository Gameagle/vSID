#include "pch.h"

#include <regex>
#include <format>
#include <windows.h>

#include "versionchecker.h"
#include "messageHandler.h"
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
	if (!std::get<3>(remote))
	{
		if (std::get<0>(remote) > std::get<0>(local) ||
			std::get<1>(remote) > std::get<1>(local) ||
			std::get<2>(remote) > std::get<2>(local))
		{
			return 1; // new stable release
		}
	}
	else
	{
		if ((std::get<0>(remote) > std::get<0>(local) ||
			std::get<1>(remote) > std::get<1>(local) ||
			std::get<2>(remote) > std::get<2>(local)) &&
			std::get<3>(remote) != std::get<3>(local))
		{
			return 2; // assumed new pre-release
		}
		else if (std::get<0>(remote) == std::get<0>(local) &&
			std::get<1>(remote) == std::get<1>(local) &&
			std::get<2>(remote) == std::get<2>(local) &&
			std::get<3>(remote) != std::get<3>(local))
		{
			return 2; // assumed new pre-release, only hash differs
		}
	}

	return 0;
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
		messageHandler->writeMessage("ERROR", "Curl error: " + std::string(curl_easy_strerror(res)) + " - Curl Code: " + std::to_string(res) +
			" - Http Code: " + std::to_string(httpCode));
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
		messageHandler->writeMessage("ERROR", "Failed to parse github version: " + std::string(e.what()));
		return std::nullopt;
	}

	return std::nullopt;
}

void vsid::version::checkForUpdates(int notify, const std::optional<vsid::version::semver>& localVersion)
{
	auto ghv = parseSemVer(getRelease((notify >= 3 && notify <= 4)).value_or(""));

	if (!localVersion)
	{
		messageHandler->writeMessage("DEBUG", "Failed to get local version. Value was empty", vsid::MessageHandler::DebugArea::Dev);
		return;
	}
	if (!ghv)
	{
		messageHandler->writeMessage("DEBUG", "Failed to get github version. Value was empty", vsid::MessageHandler::DebugArea::Dev);
		return;
	}
	else
	{
		messageHandler->writeMessage("DEBUG", "Github version: " + ((ghv.has_value()) ? semverToString(ghv.value()) : " Failed to parse version."),
			vsid::MessageHandler::DebugArea::Dev);
	}

	int comp = vsid::version::compSemVer(localVersion.value(), ghv.value());

	if (comp == 0)
	{
		messageHandler->writeMessage("DEBUG", "No new version found on github", vsid::MessageHandler::DebugArea::Dev);
		return;
	}

	switch (notify)
	{
	case 0:
		return;
	case 1:
		if (comp == 2)
		{
			messageHandler->writeMessage("DEBUG", "New pre-release version found but notifying only for stable releases.",
				vsid::MessageHandler::DebugArea::Dev);
			break;
		}

		messageHandler->writeMessage("INFO", "New Update available. Local version: " + vsid::version::semverToString(localVersion.value()) +
			" | Github version: " + vsid::version::semverToString(ghv.value()));

		break;
	case 2:
		if (comp == 2)
		{
			messageHandler->writeMessage("DEBUG", "New pre-release version found but notifying only for stable releases.",
				vsid::MessageHandler::DebugArea::Dev);
			break;
		}
		goto showmsg;
	case 3:
		messageHandler->writeMessage("INFO", "New Update (pre-release) available. Local version: " + vsid::version::semverToString(localVersion.value()) +
			" | Github version: " + vsid::version::semverToString(ghv.value()));

		break;
	case 4:
		goto showmsg;
	showmsg:
		MessageBoxA(
			nullptr,
			std::string("New version: " + vsid::version::semverToString(ghv.value()) + "\nInstalled: " + vsid::version::semverToString(localVersion.value())).c_str(),
			"vSID Update available!",
			MB_OK | MB_ICONINFORMATION
		);

		break;
	default:
		messageHandler->writeMessage("WARNING", "Didn't check for updates. Improper notification value in main config (" +
			std::to_string(notify) + "). Should be 0 - 4.");
	}
}
