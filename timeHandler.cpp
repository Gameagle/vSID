#include "pch.h"
#include "timeHandler.h"
#include "tzif.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>

namespace
{
	enum class Provider : int { Auto = 0, Wine = 1, Windows = 2 };

	std::atomic<int> providerOverride{ static_cast<int>(Provider::Auto) };

	// bumped by setZoneinfoPath / setTimeProvider - every thread_local cache compares it on use
	std::atomic<unsigned> cacheGeneration{ 0 };

	std::mutex settingsMutex; // guards zoneinfoRelPath and flaggedZones
	std::string zoneinfoRelPath = "zoneinfo";
	std::set<std::string> flaggedZones; // error flags (ERROR_TIME_ZONE_<zone>) set by this module

	//************************************
	// Description: The UTC fallback - offset 0, calls no time zone function, cannot fail
	//************************************
	constexpr std::chrono::local_seconds utcLocal(std::chrono::sys_seconds t) noexcept
	{
		return std::chrono::local_seconds{ t.time_since_epoch() };
	}

	const char* providerName(Provider p) noexcept
	{
		switch (p)
		{
		case Provider::Wine: return "wine";
		case Provider::Windows: return "windows";
		default: return "auto";
		}
	}

	//************************************
	// Description: Logs msg and sets the error flag, if the flag is not yet set (report once per cause).
	//				Never throws.
	//************************************
	void reportOnce(const std::string& flag, vsid::LogLevel level, const std::string& msg) noexcept
	{
		try
		{
			if (vsid::messageHandler && vsid::messageHandler->genErrorsContains(flag)) return;

			vsid::Logger::log(level, msg);

			if (vsid::messageHandler) vsid::messageHandler->addGenError(flag);
		}
		catch (...) {}
	}

	//************************************
	// Description: Reports a zone specific error once (flag ERROR_TIME_ZONE_<zone>). Never throws.
	//************************************
	void reportZoneOnce(const std::string& zone, vsid::LogLevel level, const std::string& msg) noexcept
	{
		try
		{
			const std::string flag = ERROR_TIME_ZONE + "_" + zone;
			{
				std::lock_guard lock(settingsMutex);
				flaggedZones.insert(flag);
			}
			reportOnce(flag, level, msg);
		}
		catch (...) {}
	}

	//************************************
	// Description: Clears the error flag of a successfully resolved zone (only its own flag). Never throws.
	//************************************
	void clearZoneFlag(const std::string& zone) noexcept
	{
		try
		{
			if (vsid::messageHandler) vsid::messageHandler->removeGenError(ERROR_TIME_ZONE + "_" + zone);
		}
		catch (...) {}
	}

	//************************************
	// Description: Drops all caches (every thread, on next use) and resets the time zone error flags
	//************************************
	void invalidateCaches()
	{
		cacheGeneration.fetch_add(1);

		std::set<std::string> flags;
		{
			std::lock_guard lock(settingsMutex);
			flags.swap(flaggedZones);
		}

		if (!vsid::messageHandler) return;

		vsid::messageHandler->removeGenError(ERROR_TIME_ZONE);
		for (const std::string& flag : flags) vsid::messageHandler->removeGenError(flag);
	}

	//************************************
	// Description: <dll dir>/<zoneinfoPath>; empty path if the DLL path can't be determined
	//************************************
	std::filesystem::path resolveZoneinfoFolder()
	{
		char pathBuf[MAX_PATH + 1] = { 0 };
		const DWORD len = GetModuleFileNameA((HINSTANCE)&__ImageBase, pathBuf, MAX_PATH);
		if (len == 0 || len >= MAX_PATH) return {};

		std::string relPath;
		{
			std::lock_guard lock(settingsMutex);
			relPath = zoneinfoRelPath;
		}

		return std::filesystem::path(pathBuf).parent_path() / relPath;
	}

	void reportFolderMissing(const std::filesystem::path& folder) noexcept
	{
		try
		{
			reportOnce(ERROR_TIME_ZONE, vsid::LogLevel::Error,
				std::format("Wine detected but the zoneinfo folder was not found at [{}]. Install zoneinfo.zip from "
					"the vSID release next to vSID.dll. Falling back to UTC (time-restricted SIDs will be wrong).",
					folder.string()));
		}
		catch (...)
		{
			try
			{
				reportOnce(ERROR_TIME_ZONE, vsid::LogLevel::Error,
					"Wine detected but the zoneinfo folder was not found. Install zoneinfo.zip from the vSID release "
					"next to vSID.dll. Falling back to UTC (time-restricted SIDs will be wrong).");
			}
			catch (...) {}
		}
	}

	//************************************
	// Description: Windows provider (std::chrono / MSVC tzdb). The only place calling locate_zone.
	//************************************
	std::chrono::local_seconds windowsLocalTime(const std::string& zone, std::chrono::sys_seconds t)
	{
		struct Cache
		{
			unsigned generation = 0;
			bool valid = false;
			std::unordered_map<std::string, const std::chrono::time_zone*> zones;
		};
		thread_local Cache cache;

		const unsigned gen = cacheGeneration.load();
		if (!cache.valid || cache.generation != gen)
		{
			cache.zones.clear();
			cache.generation = gen;
			cache.valid = true;
		}

		auto it = cache.zones.find(zone);

		if (it == cache.zones.end())
		{
			const std::chrono::time_zone* tz = nullptr;

			try
			{
				tz = std::chrono::locate_zone(zone);
				clearZoneFlag(zone);
			}
			catch (const std::runtime_error& e)
			{
				reportZoneOnce(zone, vsid::LogLevel::Warning,
					std::format("Invalid timezone [{}]. Fallback to UTC - [{}]", zone, e.what()));
			}
			catch (const std::exception& e)
			{
				reportZoneOnce(zone, vsid::LogLevel::Error,
					std::format("Unexpected exception on timezone [{}]. Fallback to UTC - [{}]", zone, e.what()));
			}

			// also caches nullptr to avoid retrying (and throwing) on every call
			it = cache.zones.emplace(zone, tz).first;
		}

		if (it->second == nullptr) return utcLocal(t);

		return it->second->to_local(t);
	}

	//************************************
	// Description: Wine provider (shipped TZif files). Never calls a std::chrono time zone function.
	//************************************
	std::chrono::local_seconds wineLocalTime(const std::string& zone, std::chrono::sys_seconds t)
	{
		struct Cache
		{
			unsigned generation = 0;
			bool valid = false;
			std::filesystem::path folder;
			std::unordered_map<std::string, std::optional<vsid::tzif::Zone>> zones; // nullopt = failed (reported)
		};
		thread_local Cache cache;

		const unsigned gen = cacheGeneration.load();
		if (!cache.valid || cache.generation != gen)
		{
			// mark as current first - if resolving throws, the folder stays empty and is reported as missing
			cache.zones.clear();
			cache.folder.clear();
			cache.generation = gen;
			cache.valid = true;
			cache.folder = resolveZoneinfoFolder();
		}

		auto it = cache.zones.find(zone);

		if (it == cache.zones.end())
		{
			vsid::tzif::LoadResult result = vsid::tzif::loadZone(cache.folder, zone);

			switch (result.status)
			{
			case vsid::tzif::LoadStatus::Ok:
				clearZoneFlag(zone);
				break;
			case vsid::tzif::LoadStatus::FolderMissing:
				reportFolderMissing(cache.folder);
				break;
			case vsid::tzif::LoadStatus::InvalidName:
				reportZoneOnce(zone, vsid::LogLevel::Error,
					std::format("Invalid timezone name [{}] (expected e.g. Europe/Berlin). Falling back to UTC for this zone.", zone));
				break;
			case vsid::tzif::LoadStatus::NotFound:
				reportZoneOnce(zone, vsid::LogLevel::Error,
					std::format("Timezone [{}] not found in the zoneinfo folder [{}]. Falling back to UTC for this zone.",
						zone, cache.folder.string()));
				break;
			case vsid::tzif::LoadStatus::Corrupt:
			default:
				reportZoneOnce(zone, vsid::LogLevel::Error,
					std::format("Timezone file for [{}] in the zoneinfo folder [{}] is corrupt or unsupported. "
						"Falling back to UTC for this zone.", zone, cache.folder.string()));
				break;
			}

			if (result.status != vsid::tzif::LoadStatus::Ok) result.zone.reset();

			// failures are cached as well (until the next config reload) - no per call file access or error spam
			it = cache.zones.emplace(zone, std::move(result.zone)).first;
		}

		if (!it->second) return utcLocal(t);

		return std::chrono::local_seconds{ t.time_since_epoch() + it->second->utcOffset(t) };
	}
}

bool vsid::time::detail::useWinePath() noexcept
{
	switch (static_cast<Provider>(providerOverride.load()))
	{
	case Provider::Wine: return true;
	case Provider::Windows: return false;
	default: return vsid::utils::usingWine();
	}
}

std::chrono::local_seconds vsid::time::localTime(const std::string& zoneName, std::chrono::sys_seconds t) noexcept
{
	if (detail::useWinePath())
	{
		try
		{
			return wineLocalTime(zoneName, t);
		}
		catch (...)
		{
			try // building the message may throw as well (e.g. bad_alloc) - nothing may leave this noexcept function
			{
				reportZoneOnce(zoneName, vsid::LogLevel::Error,
					"Unexpected error while reading the zoneinfo data for [" + zoneName + "]. Falling back to UTC for this zone.");
			}
			catch (...) {}
			return utcLocal(t);
		}
	}

	try
	{
		return windowsLocalTime(zoneName, t);
	}
	catch (const std::exception& e)
	{
		try
		{
			reportZoneOnce(zoneName, vsid::LogLevel::Error,
				std::format("Time calculation failed [{}]. Fallback to UTC - [{}]", zoneName, e.what()));
		}
		catch (...) {}
	}
	catch (...)
	{
		try
		{
			reportZoneOnce(zoneName, vsid::LogLevel::Error, "Time calculation failed [" + zoneName + "]. Fallback to UTC");
		}
		catch (...) {}
	}

	return utcLocal(t);
}

void vsid::time::setTimeProvider(std::string_view value)
{
	Provider p = Provider::Auto;

	if (vsid::utils::svEqualCi(value, "wine")) p = Provider::Wine;
	else if (vsid::utils::svEqualCi(value, "windows")) p = Provider::Windows;
	else if (!vsid::utils::svEqualCi(value, "auto"))
	{
		static std::atomic<bool> warned{ false };

		if (!warned.exchange(true))
		{
			vsid::Logger::log(vsid::LogLevel::Warning,
				std::format("Unknown timeProvider [{}] in vSidConfig.json (allowed: auto, wine, windows). Using [auto].", value));
		}
	}

	if (providerOverride.exchange(static_cast<int>(p)) != static_cast<int>(p)) invalidateCaches();
}

void vsid::time::setZoneinfoPath(std::string relativePath)
{
	if (relativePath.empty()) relativePath = "zoneinfo";

	{
		std::lock_guard lock(settingsMutex);
		zoneinfoRelPath = std::move(relativePath);
	}

	// always - a reload has to re-check the folder even if the path is unchanged
	invalidateCaches();
}

bool vsid::time::isActive(const std::string& timezone, const int start, const int end)
{
	try
	{
		const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());

		const std::chrono::local_seconds localNow = vsid::time::localTime(timezone, now);
		auto day = std::chrono::floor<std::chrono::days>(localNow);

		auto ztStart = day + std::chrono::hours{ start };
		auto ztEnd = day + std::chrono::hours{ end };

		if (ztStart <= ztEnd)
		{
			return localNow >= ztStart && localNow < ztEnd;
		}

		return localNow >= ztStart || localNow < ztEnd;
	}
	catch (const std::runtime_error& e)
	{
		try
		{
			vsid::Logger::log(
				vsid::LogLevel::Error,
				std::format("Time calculation failed [{}] - {}", timezone, e.what())
			);
		}
		catch (...) {}
	}
	catch (...)
	{
		try
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Time calculation failed [{}] - unknown error", timezone));
		}
		catch (...) {}
	}
	return false;
}

void vsid::time::logTzdbVersion()
{
	try
	{
		const Provider p = static_cast<Provider>(providerOverride.load());
		const bool winePath = detail::useWinePath();

		vsid::Logger::log(
			vsid::LogLevel::Debug,
			std::format("Time provider [{}] - Wine detected [{}] - using [{}]", providerName(p),
				vsid::utils::usingWine() ? "YES" : "NO", winePath ? "zoneinfo (TZif)" : "std::chrono tzdb")
		);

		if (!winePath)
		{
			try
			{
				vsid::Logger::log(
					vsid::LogLevel::Debug,
					std::format("Timezone database version [{}]", std::chrono::get_tzdb().version)
				);
			}
			catch (const std::exception& e)
			{
				vsid::Logger::log(
					vsid::LogLevel::Debug,
					std::format("Timezone database not available - {}", e.what())
				);
			}
			return;
		}

		// Wine: never touch the std::chrono tzdb here
		const std::filesystem::path folder = resolveZoneinfoFolder();
		std::error_code ec;

		if (!std::filesystem::is_directory(folder, ec) || ec)
		{
			reportFolderMissing(folder);
		}
		else if (std::optional<std::string> version = vsid::tzif::readVersion(folder))
		{
			vsid::Logger::log(vsid::LogLevel::Info, std::format("Zoneinfo data version [{}] at [{}]", *version, folder.string()));
		}
		else
		{
			vsid::Logger::log(vsid::LogLevel::Warning,
				std::format("Zoneinfo folder found at [{}] but its +VERSION file is missing or unreadable", folder.string()));
		}

		// sample line for Wine testers: both formatting paths must show the same text
		const auto sample = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
		std::string modern;
		try
		{
			modern = detail::modernFormat(sample, "%Y-%m-%d %H:%M:%S");
		}
		catch (const std::exception& e)
		{
			modern = std::string("FAILED - ") + e.what();
		}
		catch (...)
		{
			modern = "FAILED";
		}

		vsid::Logger::log(vsid::LogLevel::Debug,
			std::format("Time format check - legacy [{}] std::format [{}] (legacy in use: {})",
				detail::legacyFormat(sample, "%Y-%m-%d %H:%M:%S"), modern, useLegacyFormatOnWine ? "YES" : "NO"));
	}
	catch (...) {}
}
