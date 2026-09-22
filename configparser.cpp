#include "pch.h"

#include "configparser.h"
#include "airportManager.h"
#include "utils.h"
#include "messageHandler.h"
#include "sid.h"
#include "constants.h"
#include "logger.h"

#include <vector>
#include <fstream>
#include <format>

using AirportManager = vsid::apt::AirportManager;

vsid::ConfigParser::ConfigParser()
{
};

vsid::ConfigParser::~ConfigParser() = default;

void vsid::ConfigParser::loadMainConfig()
{
	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;

	std::ifstream configFile(basePath.append("vSidConfig.json").string());

	try
	{
		this->vSidConfig = json::parse(configFile);
	}
	catch(const json::parse_error &e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to parse main config: " + std::string(e.what()));
	}
	catch (const json::type_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to parse main config: " + std::string(e.what()));
	}
	catch (const json::other_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to parse main config: " + std::string(e.what()));
	}

	if (this->vSidConfig.is_null())
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to parse main config. (Critical!)");
		return;
	}

	// set topsky preference

	this->preferTopsky = this->vSidConfig.value("preferTopsky", true);

	// set update notification

	this->notifyUpdate = this->vSidConfig.value("notifyUpdate", 1);

	// set hand over warning altitude

	this->hovWarningAlt = this->vSidConfig.value("hovWarningAlt", 1500);

	// set logging of dev only messages

	this->logDevOnly = this->vSidConfig.value("logDevOnly", false);

	try
	{
		// import colors or set default values

		if (this->vSidConfig.at("colors").contains("sidSuggestion"))
		{
			this->colors["sidSuggestion"] = RGB(
				this->vSidConfig.at("colors").at("sidSuggestion").value("r", 255),
				this->vSidConfig.at("colors").at("sidSuggestion").value("g", 255),
				this->vSidConfig.at("colors").at("sidSuggestion").value("b", 255)
			);
		}
		else this->colors["sidSuggestion"] = RGB(255, 255, 255);
		
		if (this->vSidConfig.at("colors").contains("suggestedSidSet"))
		{
			this->colors["suggestedSidSet"] = RGB(
				this->vSidConfig.at("colors").at("suggestedSidSet").value("r", 0),
				this->vSidConfig.at("colors").at("suggestedSidSet").value("g", 255),
				this->vSidConfig.at("colors").at("suggestedSidSet").value("b", 0)
			);
		}
		else this->colors["suggestedSidSet"] = RGB(0, 255, 0);

		if (this->vSidConfig.at("colors").contains("customSidSuggestion"))
		{
			this->colors["customSidSuggestion"] = RGB(
				this->vSidConfig.at("colors").at("customSidSuggestion").value("r", 255),
				this->vSidConfig.at("colors").at("customSidSuggestion").value("g", 255),
				this->vSidConfig.at("colors").at("customSidSuggestion").value("b", 180)
			);
		}
		else this->colors["customSidSuggestion"] = RGB(255, 255, 180);

		if (this->vSidConfig.at("colors").contains("customSidSet"))
		{
			this->colors["customSidSet"] = RGB(
				this->vSidConfig.at("colors").at("customSidSet").value("r", 255),
				this->vSidConfig.at("colors").at("customSidSet").value("g", 120),
				this->vSidConfig.at("colors").at("customSidSet").value("b", 30)
			);
		}
		else this->colors["customSidSet"] = RGB(255, 120, 30);

		if (this->vSidConfig.at("colors").contains("noSid"))
		{
			this->colors["noSid"] = RGB(
				this->vSidConfig.at("colors").at("noSid").value("r", 220),
				this->vSidConfig.at("colors").at("noSid").value("g", 30),
				this->vSidConfig.at("colors").at("noSid").value("b", 20)
			);
		}
		else this->colors["noSid"] = RGB(220, 30, 20);

		if (this->vSidConfig.at("colors").contains("sidHighlight"))
		{
			this->colors["sidHighlight"] = RGB(
				this->vSidConfig.at("colors").at("sidHighlight").value("r", 240),
				this->vSidConfig.at("colors").at("sidHighlight").value("g", 90),
				this->vSidConfig.at("colors").at("sidHighlight").value("b", 190)
			);
		}
		else this->colors["sidHighlight"] = RGB(240, 90, 190);

		if (this->vSidConfig.at("colors").contains("suggestedClmb"))
		{
			this->colors["suggestedClmb"] = RGB(
				this->vSidConfig.at("colors").at("suggestedClmb").value("r", 255),
				this->vSidConfig.at("colors").at("suggestedClmb").value("g", 255),
				this->vSidConfig.at("colors").at("suggestedClmb").value("b", 255)
			);
		}
		else this->colors["suggestedClmb"] = RGB(255, 255, 255);

		if (this->vSidConfig.at("colors").contains("customClmbSet"))
		{
			this->colors["customClmbSet"] = RGB(
				this->vSidConfig.at("colors").at("customClmbSet").value("r", 255),
				this->vSidConfig.at("colors").at("customClmbSet").value("g", 120),
				this->vSidConfig.at("colors").at("customClmbSet").value("b", 30)
			);
		}
		else this->colors["customClmbSet"] = RGB(255, 120, 30);

		if (this->vSidConfig.at("colors").contains("clmbSet"))
		{
			this->colors["clmbSet"] = RGB(
				this->vSidConfig.at("colors").at("clmbSet").value("r", 50),
				this->vSidConfig.at("colors").at("clmbSet").value("g", 240),
				this->vSidConfig.at("colors").at("clmbSet").value("b", 210)
			);
		}
		else this->colors["clmbSet"] = RGB(50, 240, 210);

		if (this->vSidConfig.at("colors").contains("clmbViaSet"))
		{
			this->colors["clmbViaSet"] = RGB(
				this->vSidConfig.at("colors").at("clmbViaSet").value("r", 0),
				this->vSidConfig.at("colors").at("clmbViaSet").value("g", 255),
				this->vSidConfig.at("colors").at("clmbViaSet").value("b", 0)
			);
		}
		else this->colors["clmbViaSet"] = RGB(0, 255, 0);

		if (this->vSidConfig.at("colors").contains("clmbHighlight"))
		{
			this->colors["clmbHighlight"] = RGB(
				this->vSidConfig.at("colors").at("clmbHighlight").value("r", 240),
				this->vSidConfig.at("colors").at("clmbHighlight").value("g", 90),
				this->vSidConfig.at("colors").at("clmbHighlight").value("b", 190)
			);
		}
		else this->colors["clmbHighlight"] = RGB(240, 90, 190);

		if (this->vSidConfig.at("colors").contains("rwyNotSet"))
		{
			this->colors["rwyNotSet"] = RGB(
				this->vSidConfig.at("colors").at("rwyNotSet").value("r", 255),
				this->vSidConfig.at("colors").at("rwyNotSet").value("g", 255),
				this->vSidConfig.at("colors").at("rwyNotSet").value("b", 255)
			);
		}
		else this->colors["rwyNotSet"] = RGB(255, 255, 255);

		if (this->vSidConfig.at("colors").contains("rwySet"))
		{
			this->colors["rwySet"] = RGB(
				this->vSidConfig.at("colors").at("rwySet").value("r", 0),
				this->vSidConfig.at("colors").at("rwySet").value("g", 255),
				this->vSidConfig.at("colors").at("rwySet").value("b", 0)
			);
		}
		else this->colors["rwySet"] = RGB(0, 255, 0);

		if (this->vSidConfig.at("colors").contains("notDepRwySet"))
		{
			this->colors["notDepRwySet"] = RGB(
				this->vSidConfig.at("colors").at("notDepRwySet").value("r", 230),
				this->vSidConfig.at("colors").at("notDepRwySet").value("g", 230),
				this->vSidConfig.at("colors").at("notDepRwySet").value("b", 60)
			);
		}
		else this->colors["notDepRwySet"] = RGB(230, 230, 60);

		if (this->vSidConfig.at("colors").contains("squawkSet"))
		{
			this->colors["squawkSet"] = RGB(
				this->vSidConfig.at("colors").at("squawkSet").value("r", 255),
				this->vSidConfig.at("colors").at("squawkSet").value("g", 255),
				this->vSidConfig.at("colors").at("squawkSet").value("b", 255)
			);
		}
		// no else case for squawk set due to special values following below

		if (this->vSidConfig.at("colors").contains("squawkNotSet"))
		{
			this->colors["squawkNotSet"] = RGB(
				this->vSidConfig.at("colors").at("squawkNotSet").value("r", 230),
				this->vSidConfig.at("colors").at("squawkNotSet").value("g", 230),
				this->vSidConfig.at("colors").at("squawkNotSet").value("b", 60)
			);
		}
		else this->colors["squawkNotSet"] = RGB(230, 230, 60);

		if (this->vSidConfig.at("colors").contains("requestNeutral"))
		{
			this->colors["requestNeutral"] = RGB(
				this->vSidConfig.at("colors").at("requestNeutral").value("r", 128),
				this->vSidConfig.at("colors").at("requestNeutral").value("g", 128),
				this->vSidConfig.at("colors").at("requestNeutral").value("b", 128)
			);
		}
		else this->colors["requestNeutral"] = RGB(128, 128, 128);

		if (this->vSidConfig.at("colors").contains("requestCaution"))
		{
			this->colors["requestCaution"] = RGB(
				this->vSidConfig.at("colors").at("requestCaution").value("r", 230),
				this->vSidConfig.at("colors").at("requestCaution").value("g", 230),
				this->vSidConfig.at("colors").at("requestCaution").value("b", 60)
			);
		}
		else this->colors["requestCaution"] = RGB(230, 230, 60);

		if (this->vSidConfig.at("colors").contains("requestWarning"))
		{
			this->colors["requestWarning"] = RGB(
				this->vSidConfig.at("colors").at("requestWarning").value("r", 220),
				this->vSidConfig.at("colors").at("requestWarning").value("g", 30),
				this->vSidConfig.at("colors").at("requestWarning").value("b", 20)
			);
		}
		else this->colors["requestWarning"] = RGB(220, 30, 20);

		if (this->vSidConfig.at("colors").contains("clrfSet"))
		{
			this->colors["clrfSet"] = RGB(
				this->vSidConfig.at("colors").at("clrfSet").value("r", 0),
				this->vSidConfig.at("colors").at("clrfSet").value("g", 160),
				this->vSidConfig.at("colors").at("clrfSet").value("b", 30)
			);
		}
		else this->colors["clrfSet"] = RGB(0, 160, 30);

		if (this->vSidConfig.at("colors").contains("clrfCaution"))
		{
			this->colors["clrfCaution"] = RGB(
				this->vSidConfig.at("colors").at("clrfCaution").value("r", 250),
				this->vSidConfig.at("colors").at("clrfCaution").value("g", 160),
				this->vSidConfig.at("colors").at("clrfCaution").value("b", 0)
			);
		}
		else this->colors["clrfCaution"] = RGB(250, 160, 0);

		if (this->vSidConfig.at("colors").contains("clrfWarning"))
		{
			this->colors["clrfWarning"] = RGB(
				this->vSidConfig.at("colors").at("clrfWarning").value("r", 200),
				this->vSidConfig.at("colors").at("clrfWarning").value("g", 10),
				this->vSidConfig.at("colors").at("clrfWarning").value("b", 10)
			);
		}
		else this->colors["clrfWarning"] = RGB(200, 10, 10);

		if (this->vSidConfig.at("colors").contains("intsecSet"))
		{
			this->colors["intsecSet"] = RGB(
				this->vSidConfig.at("colors").at("intsecSet").value("r", 0),
				this->vSidConfig.at("colors").at("intsecSet").value("g", 150),
				this->vSidConfig.at("colors").at("intsecSet").value("b", 50)
			);
		}
		else this->colors["intsecSet"] = RGB(0, 150, 50);

		if (this->vSidConfig.at("colors").contains("intsecAble"))
		{
			this->colors["intsecAble"] = RGB(
				this->vSidConfig.at("colors").at("intsecAble").value("r", 200),
				this->vSidConfig.at("colors").at("intsecAble").value("g", 150),
				this->vSidConfig.at("colors").at("intsecAble").value("b", 0)
			);
		}

		if (this->vSidConfig.at("colors").contains("intsecSetIndicator"))
		{
			this->colors["intsecSetIndicator"] = RGB(
				this->vSidConfig.at("colors").at("intsecSetIndicator").value("r", 0),
				this->vSidConfig.at("colors").at("intsecSetIndicator").value("g", 150),
				this->vSidConfig.at("colors").at("intsecSetIndicator").value("b", 50)
			);
		}

		if (this->vSidConfig.at("colors").contains("intsecAbleIndicator"))
		{
			this->colors["intsecAbleIndicator"] = RGB(
				this->vSidConfig.at("colors").at("intsecAbleIndicator").value("r", 200),
				this->vSidConfig.at("colors").at("intsecAbleIndicator").value("g", 150),
				this->vSidConfig.at("colors").at("intsecAbleIndicator").value("b", 0)
			);
		}

		else this->colors["intsecSetIndicator"] = RGB(200, 150, 0);

		if (this->vSidConfig.at("colors").contains("pbIndicator"))
		{
			this->colors["pbIndicator"] = RGB(
				this->vSidConfig.at("colors").at("pbIndicator").value("r", 0),
				this->vSidConfig.at("colors").at("pbIndicator").value("g", 255),
				this->vSidConfig.at("colors").at("pbIndicator").value("b", 0)
			);
		}
		else this->colors["pbIndicator"] = RGB(0, 255, 0);

		if (this->vSidConfig.at("colors").contains("reqIndicator"))
		{
			this->colors["reqIndicator"] = RGB(
				this->vSidConfig.at("colors").at("reqIndicator").value("r", 255),
				this->vSidConfig.at("colors").at("reqIndicator").value("g", 255),
				this->vSidConfig.at("colors").at("reqIndicator").value("b", 255)
			);
		}
		else this->colors["reqIndicator"] = RGB(255, 255, 255);

		if (this->vSidConfig.at("colors").contains("hovNeutral"))
		{
			this->colors["hovNeutral"] = RGB(
				this->vSidConfig.at("colors").at("hovNeutral").value("r", 250),
				this->vSidConfig.at("colors").at("hovNeutral").value("g", 160),
				this->vSidConfig.at("colors").at("hovNeutral").value("b", 0)
			);
		}
		else this->colors["hovNeutral"] = RGB(250, 160, 0);

		if (this->vSidConfig.at("colors").contains("hovWarning"))
		{
			this->colors["hovWarning"] = RGB(
				this->vSidConfig.at("colors").at("hovWarning").value("r", 250),
				this->vSidConfig.at("colors").at("hovWarning").value("g", 0),
				this->vSidConfig.at("colors").at("hovWarning").value("b", 0)
			);
		}
		else this->colors["hovWarning"] = RGB(250, 0, 0);

		// pseudo values for special color use cases
		if (!this->colors.contains("squawkSet")) this->colors["squawkSet"] = RGB(300, 300, 300);
	}
	catch (std::error_code& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to import colors: " + e.message());
	}
	catch (const json::parse_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "[Parse Error] in color config section: " + std::string(e.what()));
	}
	catch (const json::type_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "[Type Error] in color config section: " + std::string(e.what()));
	}
	catch (const json::other_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "[Other Error] in color config section: " + std::string(e.what()));
	}
	catch (const json::out_of_range& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "[Out of Range] in color config section: " + std::string(e.what()));
	}

	// get request times

	try
	{
		this->reqTimes.insert({ "caution", this->vSidConfig.at("requests").value("caution", 2) });
		this->reqTimes.insert({ "warning", this->vSidConfig.at("requests").value("warning", 5) });
	}
	catch (json::parse_error& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to get request timers: " + std::string(e.what()));
	}

	// get clrf min values

	try
	{
		this->clrf.altCaution = this->vSidConfig.at("clrf").value("altCaution", 1500);
		this->clrf.altWarning = this->vSidConfig.at("clrf").value("altWarning", 500);
		this->clrf.distCaution = this->vSidConfig.at("clrf").value("distCaution", 10.0);
		this->clrf.distWarning = this->vSidConfig.at("clrf").value("distWarning", 2.0);
	}
	catch (json::out_of_range& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to get clrf min values: " + std::string(e.what()));
	}

	// get indicator reference values

	try
	{
		this->indicator.offset = this->vSidConfig.at("display").value("indicatorOffset", 20);
		this->indicator.zoomScale = this->vSidConfig.at("display").value("indicatorZoomScale", 0.5);
		this->indicator.showBelowZoom = this->vSidConfig.at("display").value("indicatorShowBelowZoom", 600);
	}
	catch (json::out_of_range& e)
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Failed to get indicator default reference values: " + std::string(e.what()));
	}
}

void vsid::ConfigParser::loadAirportConfig()
{
	// get the current path where plugins .dll is stored
	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;

	if (this->vSidConfig.contains("airportConfigs"))
	{
		basePath.append(this->vSidConfig.value("airportConfigs", "")).make_preferred();
	}
	else
	{
		vsid::Logger::log(vsid::LogLevel::Error, "No config path for airports in main config");
		return;
	}

	if (!std::filesystem::exists(basePath))
	{
		vsid::Logger::log(vsid::LogLevel::Error, "No airport config folder found at: " + basePath.string());
		return;
	}

	std::vector<std::filesystem::path> files;
	std::set<std::string> aptConfig;

	for (auto &[icao, aptInfo] : AirportManager::getAirports())
	{
		for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
		//for (const std::filesystem::path& entry : this->configPaths)
		{
			if (!std::filesystem::is_directory(entry) && entry.extension() == ".json")
			{
				std::ifstream configFile(entry.string());

				try
				{
					this->parsedConfig = json::parse(configFile);

					if (!this->parsedConfig.contains(icao)) continue;

					aptConfig.insert(icao);

					const auto& config = this->parsedConfig.at(icao);

					const bool updated = AirportManager::update(icao, [&](vsid::apt::AirportData& data)
						{
							// general settings

							data.icao = icao;
							data.elevation = config.value("elevation", 0);
							data.equipCheck = config.value("equipCheck", true);
							data.enableRVSids = config.value("enableRVSids", true);
							data.allRwys = vsid::utils::split(config.value("runways", ""), ',');
							data.transAlt = config.value("transAlt", 0);
							data.maxInitialClimb = config.value("maxInitialClimb", 0);
							data.timezone = config.value("timezone", "Etc/UTC");
							data.autoHandoff = config.value("autoHandoff", true);

							for (const char* reqType : {
								"clearance",
								"startup",
								"pushback",
								"taxi",
								"departure",
								"vfr"
								})
							{
								data.requests.try_emplace(reqType);
							}

							data.rwyrequests.try_emplace("startup");

							data.settings.try_emplace("auto", false);
							data.settings.try_emplace("lvp", false);
							data.settings.try_emplace("time", false);

							// customRules

							const auto& customRules = config.value("customRules", std::map<std::string, bool>{});

							for (auto it = data.customRules.begin(); it != data.customRules.end();)
							{
								if (!customRules.contains(it->first)) it = data.customRules.erase(it);
								else ++it;
							}

							for (auto& [rule, enabled] : customRules)
							{
								data.customRules.try_emplace(vsid::utils::toupper(rule), enabled);
							}

							int appSIPrio = 0;
							for (std::string& si : vsid::utils::split(config.value("appSI", ""), ','))
							{
								data.appSI[si] = appSIPrio;
								appSIPrio++;
							}

							// areas

							if (config.contains("areas"))
							{
								const auto& areas = config.at("areas");

								for (auto it = data.areas.begin(); it != data.areas.end();)
								{
									if (!areas.contains(it->first)) it = data.areas.erase(it);
									else ++it;
								}

								for (auto& [areaKey, areaValue] : areas.items())
								{
									auto existingArea = data.areas.find(areaKey);
									const bool areaExists = existingArea != data.areas.end();

									std::vector<std::pair<std::string, std::string>> coords;
									bool isActive = areaExists ? existingArea->second.isActive : false;
									bool arrAsDep = false;

									for (auto& [coordKey, coordValue] : areaValue.items())
									{
										if (coordKey == "active")
										{
											if (!areaExists) coordValue.get_to<bool>(isActive);

											continue;
										}

										if (coordKey == "arrAsDep")
										{
											coordValue.get_to(arrAsDep);

											continue;
										}

										std::string lat = coordValue.value("lat", std::string{});
										std::string lon = coordValue.value("lon", std::string{});

										if (lat.empty() || lon.empty())
										{
											vsid::Logger::log(
												vsid::LogLevel::Error,
												std::format(
													"Couldn't read LAT or LON value for [{}] in area [{}] at [{}]",
													coordKey,
													areaKey,
													icao
												)
											);

											break;
										}

										coords.push_back({ lat, lon });
									}

									if (coords.size() < 3)
									{
										vsid::Logger::log(
											vsid::LogLevel::Error,
											std::format(
												"Area [{}] in [{}] has not enough points configured (less than 3).",
												areaKey,
												icao
											)
										);

										continue;
									}

									vsid::Area parsedArea{ coords, isActive, arrAsDep };

									if (areaExists) existingArea->second = std::move(parsedArea);
									else data.areas.emplace(areaKey, std::move(parsedArea));
								}
							}

							// intersections

							const auto intsecIt = config.find("intersections");

							if (intsecIt == config.end()) data.intsec.clear();
							else
							{
								const auto& intsec = *intsecIt;

								for (auto it = data.intsec.begin(); it != data.intsec.end();)
								{
									if (!intsec.contains(it->first)) it = data.intsec.erase(it);
									else ++it;
								}

								for (const auto& [rwy, intsecList] : intsec.items())
								{
									auto intsecVec = vsid::utils::split(intsecList.get_ref<const std::string&>(), ',');

									data.intsec.insert_or_assign(rwy, intsecVec);
								}
							}

							// sids
							// initialize default values

							vsid::tmpSidSettings fieldSetting;
							vsid::tmpSidSettings wptSetting;
							vsid::tmpSidSettings desSetting;
							vsid::tmpSidSettings idSetting;

							// "field level" - iterates over restrictions and sid way points / bases

							// clear existing sids and timeSids to avoid duplicates

							data.sids.clear();
							data.timeSids.clear();

							const auto& sids = this->parsedConfig.at(icao).at("sids");

							for (auto& sidField : sids.items())
							{
								std::string fixedNumber = "";

								if (sidField.key() == "allowDiffNumbers") fieldSetting.allowDiffNumbers = sids.at(sidField.key());
								else if (sidField.key() == "initial") fieldSetting.initial = sids.at(sidField.key());
								else if (sidField.key() == "climbvia") fieldSetting.via = sids.at(sidField.key());
								else if (sidField.key() == "wpt") fieldSetting.wpt = sids.at(sidField.key());
								else if (sidField.key() == "pilotfiled") fieldSetting.pilotfiled = sids.at(sidField.key());
								else if (sidField.key() == "wingType") fieldSetting.wingType = sids.at(sidField.key());
								else if (sidField.key() == "acftType") fieldSetting.acftType = sids.at(sidField.key());
								else if (sidField.key() == "dest") fieldSetting.dest = sids.at(sidField.key());
								else if (sidField.key() == "route")
								{
									if (sids.at(sidField.key()).contains("allow"))
									{
										for (const auto& id : sids.at(sidField.key()).at("allow").items())
										{
											std::string routeId = id.key();
											std::vector<std::string> configRoute =
												vsid::utils::split(sids.at(sidField.key()).at("allow").value(routeId, ""), ',');

											if (!configRoute.empty()) fieldSetting.route["allow"].insert({ routeId, configRoute });
										}
									}

									if (sids.at(sidField.key()).contains("deny"))
									{
										for (const auto& id : sids.at(sidField.key()).at("deny").items())
										{
											std::string routeId = id.key();
											std::vector<std::string> configRoute =
												vsid::utils::split(sids.at(sidField.key()).at("deny").value(routeId, ""), ',');

											if (!configRoute.empty()) fieldSetting.route["deny"].insert({ routeId, configRoute });
										}
									}
								}
								else if (sidField.key() == "wtc") fieldSetting.wtc = sids.at(sidField.key());
								else if (sidField.key() == "engineType") fieldSetting.engineType = sids.at(sidField.key());
								else if (sidField.key() == "engineCount") fieldSetting.engineCount = sids.at(sidField.key());
								else if (sidField.key() == "mtow") fieldSetting.mtow = sids.at(sidField.key());
								else if (sidField.key() == "customRule") fieldSetting.customRule = vsid::utils::toupper(sids.at(sidField.key()));
								else if (sidField.key() == "area") fieldSetting.area = vsid::utils::toupper(sids.at(sidField.key()));
								else if (sidField.key() == "equip")
								{
									fieldSetting.equip = sids.at(sidField.key());

									// updating equipment codes to upper case if in lower case

									for (std::map<std::string, bool>::iterator it = fieldSetting.equip.begin(); it != fieldSetting.equip.end();)
									{
										if (it->first != vsid::utils::toupper(it->first))
										{
											std::pair<std::string, bool> cap = { vsid::utils::toupper(it->first), it->second };
											it = fieldSetting.equip.erase(it);
											fieldSetting.equip.insert(it, cap);
											continue;
										}
										++it;
									}
								}
								else if (sidField.key() == "lvp") fieldSetting.lvp = sids.at(sidField.key());
								else if (sidField.key() == "actArrRwy")
								{
									if (sids.at(sidField.key()).contains("allow"))
									{
										fieldSetting.actArrRwy["allow"]["all"] = sids.at(sidField.key()).at("allow").value("all", "");
										fieldSetting.actArrRwy["allow"]["any"] = sids.at(sidField.key()).at("allow").value("any", "");
									}

									if (sids.at(sidField.key()).contains("deny"))
									{
										fieldSetting.actArrRwy["deny"]["all"] = sids.at(sidField.key()).at("deny").value("all", "");
										fieldSetting.actArrRwy["deny"]["any"] = sids.at(sidField.key()).at("deny").value("any", "");
									}
								}
								else if (sidField.key() == "actDepRwy")
								{
									if (sids.at(sidField.key()).contains("allow"))
									{
										fieldSetting.actDepRwy["allow"]["all"] = sids.at(sidField.key()).at("allow").value("all", "");
										fieldSetting.actDepRwy["allow"]["any"] = sids.at(sidField.key()).at("allow").value("any", "");
									}

									if (sids.at(sidField.key()).contains("deny"))
									{
										fieldSetting.actDepRwy["deny"]["all"] = sids.at(sidField.key()).at("deny").value("all", "");
										fieldSetting.actDepRwy["deny"]["any"] = sids.at(sidField.key()).at("deny").value("any", "");
									}
								}
								else if (sidField.key() == "timeFrom") fieldSetting.timeFrom = sids.at(sidField.key());
								else if (sidField.key() == "timeTo") fieldSetting.timeTo = sids.at(sidField.key());
								else if (sidField.key() == "sidHighlight") fieldSetting.sidHighlight = sids.at(sidField.key());
								else if (sidField.key() == "clmbHighlight") fieldSetting.clmbHighlight = sids.at(sidField.key());
								else if (!this->isConfigValue(sidField.key()))
								{
									// special check for possible military SIDs / OIDs (format: XY12)

									if (vsid::utils::lastIsDigit(sidField.key()) && vsid::utils::countDigits(sidField.key()) > 1 && sidField.key().length() > 2)
									{
										fieldSetting.base = sidField.key().substr(0, sidField.key().length() - 1);
										fixedNumber = sidField.key().back();

										vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] contained a number - setting as fixed SID number [{}]",
											sidField.key(), fixedNumber), vsid::DebugLevel::Conf);
									}
									else
									{
										fieldSetting.base = sidField.key();
										fieldSetting.wpt = fieldSetting.base; // #evaluate - remove from field settings and always overwrite in wptSettings?
									}

									// "waypoint / base level" - iterates over restrictions and sid designators

									wptSetting = fieldSetting;

									const auto& sidFieldKey = sids.at(sidField.key());

									for (auto& sidWpt : sidFieldKey.items())
									{
										if (sidWpt.key() == "allowDiffNumbers") wptSetting.allowDiffNumbers = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "initial") wptSetting.initial = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "climbvia") wptSetting.via = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "wpt") wptSetting.wpt = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "trans")
										{
											wptSetting.transition.clear();

											for (auto& base : sidFieldKey.at(sidWpt.key()).items())
											{
												vsid::Transition trans;

												trans.base = base.key();

												if (std::string desig = sidFieldKey.at(sidWpt.key()).at(base.key()); desig != "XXX")
													trans.designator = desig;

												wptSetting.transition.insert({ base.key(), trans });
											}
										}
										else if (sidWpt.key() == "pilotfiled") wptSetting.pilotfiled = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "wingType") wptSetting.wingType = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "acftType") wptSetting.acftType = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "dest") wptSetting.dest = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "route")
										{
											if (sidFieldKey.at(sidWpt.key()).contains("allow"))
											{
												wptSetting.route["allow"].clear();

												for (const auto& id : sidFieldKey.at(sidWpt.key()).at("allow").items())
												{
													std::string routeId = id.key();
													std::vector<std::string> configRoute =
														vsid::utils::split(sidFieldKey.at(sidWpt.key()).at("allow").value(routeId, ""), ',');

													if (!configRoute.empty()) wptSetting.route["allow"].insert({ routeId, configRoute });
												}
											}

											if (sidFieldKey.at(sidWpt.key()).contains("deny"))
											{
												wptSetting.route["deny"].clear();

												for (const auto& id : sidFieldKey.at(sidWpt.key()).at("deny").items())
												{
													std::string routeId = id.key();
													std::vector<std::string> configRoute =
														vsid::utils::split(sidFieldKey.at(sidWpt.key()).at("deny").value(routeId, ""), ',');

													if (!configRoute.empty()) wptSetting.route["deny"].insert({ routeId, configRoute });
												}
											}
										}
										else if (sidWpt.key() == "wtc") wptSetting.wtc = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "engineType") wptSetting.engineType = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "engineCount") wptSetting.engineCount = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "mtow") wptSetting.mtow = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "customRule") wptSetting.customRule = vsid::utils::toupper(sidFieldKey.at(sidWpt.key()));
										else if (sidWpt.key() == "area") wptSetting.area = vsid::utils::toupper(sidFieldKey.at(sidWpt.key()));
										else if (sidWpt.key() == "equip")
										{
											wptSetting.equip = sidFieldKey.at(sidWpt.key());

											// updating equipment codes to upper case if in lower case

											for (auto it = wptSetting.equip.begin(); it != wptSetting.equip.end();)
											{
												if (it->first != vsid::utils::toupper(it->first))
												{
													std::pair<std::string, bool> cap = { vsid::utils::toupper(it->first), it->second };
													it = wptSetting.equip.erase(it);
													wptSetting.equip.insert(it, cap);
													continue;
												}
												++it;
											}
										}
										else if (sidWpt.key() == "lvp") wptSetting.lvp = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "actArrRwy")
										{
											if (sidFieldKey.at(sidWpt.key()).contains("allow"))
											{
												wptSetting.actArrRwy["allow"]["all"] = sidFieldKey.at(sidWpt.key()).at("allow").value("all", "");
												wptSetting.actArrRwy["allow"]["any"] = sidFieldKey.at(sidWpt.key()).at("allow").value("any", "");
											}

											if (sidFieldKey.at(sidWpt.key()).contains("deny"))
											{
												wptSetting.actArrRwy["deny"]["all"] = sidFieldKey.at(sidWpt.key()).at("deny").value("all", "");
												wptSetting.actArrRwy["deny"]["any"] = sidFieldKey.at(sidWpt.key()).at("deny").value("any", "");
											}
										}
										else if (sidWpt.key() == "actDepRwy")
										{
											if (sidFieldKey.at(sidWpt.key()).contains("allow"))
											{
												wptSetting.actDepRwy["allow"]["all"] = sidFieldKey.at(sidWpt.key()).at("allow").value("all", "");
												wptSetting.actDepRwy["allow"]["any"] = sidFieldKey.at(sidWpt.key()).at("allow").value("any", "");
											}

											if (sidFieldKey.at(sidWpt.key()).contains("deny"))
											{
												wptSetting.actDepRwy["deny"]["all"] = sidFieldKey.at(sidWpt.key()).at("deny").value("all", "");
												wptSetting.actDepRwy["deny"]["any"] = sidFieldKey.at(sidWpt.key()).at("deny").value("any", "");
											}
										}
										else if (sidWpt.key() == "timeFrom") wptSetting.timeFrom = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "timeTo") wptSetting.timeTo = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "sidHighlight") wptSetting.sidHighlight = sidFieldKey.at(sidWpt.key());
										else if (sidWpt.key() == "clmbHighlight") wptSetting.clmbHighlight = sidFieldKey.at(sidWpt.key());
										else if (!this->isConfigValue(sidWpt.key()))
										{
											if (!vsid::utils::containsDigit(sidWpt.key()) && sidWpt.key() != "XXX") wptSetting.desig = sidWpt.key();

											// "designator level" - iterates over restrictions and sid ids

											desSetting = wptSetting;

											const auto& sidWptKey = sidFieldKey.at(sidWpt.key());

											for (auto& sidDes : sidWptKey.items())
											{
												if (sidDes.key() == "rwy")
													desSetting.rwys = vsid::utils::split(sidWptKey.at(sidDes.key()), ',');
												else if (sidDes.key() == "allowDiffNumbers")
													desSetting.allowDiffNumbers = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "initial")
													desSetting.initial = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "climbvia")
													desSetting.via = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "wpt")
													desSetting.wpt = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "trans")
												{
													desSetting.transition.clear();

													for (auto& base : sidWptKey.at(sidDes.key()).items())
													{
														vsid::Transition trans;

														trans.base = base.key();
														if (std::string desig = sidWptKey.at(sidDes.key()).at(base.key()); desig != "XXX")
														{
															trans.designator = desig;
														}

														desSetting.transition.insert({ base.key(), trans });
													}

													if (sidWptKey
														.at(sidDes.key()).size() == 0) desSetting.transition.clear();
												}
												else if (sidDes.key() == "pilotfiled")
													desSetting.pilotfiled = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "wingType")
													desSetting.wingType = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "acftType")
													desSetting.acftType = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "dest")
													desSetting.dest = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "route")
												{
													if (sidWptKey.at(sidDes.key()).contains("allow"))
													{
														desSetting.route["allow"].clear();

														for (const auto& id : sidWptKey.at(sidDes.key()).at("allow").items())
														{
															std::string routeId = id.key();
															std::vector<std::string> configRoute =
																vsid::utils::split(sidWptKey.at(sidDes.key()).at("allow").value(routeId, ""), ',');

															if (!configRoute.empty()) desSetting.route["allow"].insert({ routeId, configRoute });
														}
													}

													if (sidWptKey.at(sidDes.key()).contains("deny"))
													{
														desSetting.route["deny"].clear();

														for (const auto& id : sidWptKey.at(sidDes.key()).at("deny").items())
														{
															std::string routeId = id.key();
															std::vector<std::string> configRoute =
																vsid::utils::split(sidWptKey.at(sidDes.key()).at("deny").value(routeId, ""), ',');

															if (!configRoute.empty()) desSetting.route["deny"].insert({ routeId, configRoute });
														}
													}
												}
												else if (sidDes.key() == "wtc")
													desSetting.wtc = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "engineType")
													desSetting.engineType = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "engineCount")
													desSetting.engineCount = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "mtow")
													desSetting.mtow = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "customRule")
													desSetting.customRule = vsid::utils::toupper(sidWptKey.at(sidDes.key()));
												else if (sidDes.key() == "area")
													desSetting.area = vsid::utils::toupper(sidWptKey.at(sidDes.key()));
												else if (sidDes.key() == "equip")
												{
													desSetting.equip = sidWptKey.at(sidDes.key());

													// updating equipment codes to upper case if in lower case

													for (std::map<std::string, bool>::iterator it = desSetting.equip.begin(); it != desSetting.equip.end();)
													{
														if (it->first != vsid::utils::toupper(it->first))
														{
															std::pair<std::string, bool> cap = { vsid::utils::toupper(it->first), it->second };
															it = desSetting.equip.erase(it);
															desSetting.equip.insert(it, cap);
															continue;
														}
														++it;
													}
												}
												else if (sidDes.key() == "lvp")
													desSetting.lvp = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "actArrRwy")
												{
													if (sidWptKey.at(sidDes.key()).contains("allow"))
													{
														desSetting.actArrRwy["allow"]["all"] =
															sidWptKey.at(sidDes.key()).at("allow").value("all", "");
														desSetting.actArrRwy["allow"]["any"] =
															sidWptKey.at(sidDes.key()).at("allow").value("any", "");
													}

													if (sidWptKey.at(sidDes.key()).contains("deny"))
													{
														desSetting.actArrRwy["deny"]["all"] =
															sidWptKey.at(sidDes.key()).at("deny").value("all", "");
														desSetting.actArrRwy["deny"]["any"] =
															sidWptKey.at(sidDes.key()).at("deny").value("any", "");
													}
												}
												else if (sidDes.key() == "actDepRwy")
												{
													if (sidWptKey.at(sidDes.key()).contains("allow"))
													{
														desSetting.actDepRwy["allow"]["all"] =
															sidWptKey.at(sidDes.key()).at("allow").value("all", "");
														desSetting.actDepRwy["allow"]["any"] =
															sidWptKey.at(sidDes.key()).at("allow").value("any", "");
													}

													if (sidWptKey.at(sidDes.key()).contains("deny"))
													{
														desSetting.actDepRwy["deny"]["all"] =
															sidWptKey.at(sidDes.key()).at("deny").value("all", "");
														desSetting.actDepRwy["deny"]["any"] =
															sidWptKey.at(sidDes.key()).at("deny").value("any", "");
													}
												}
												else if (sidDes.key() == "timeFrom")
													desSetting.timeFrom = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "timeTo")
													desSetting.timeTo = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "sidHighlight")
													desSetting.sidHighlight = sidWptKey.at(sidDes.key());
												else if (sidDes.key() == "clmbHighlight")
													desSetting.clmbHighlight = sidWptKey.at(sidDes.key());
												else if (!this->isConfigValue(sidDes.key()))
												{
													desSetting.id = sidDes.key();

													// "id level" - iterates over restrictions on id level (highest priority)

													idSetting = desSetting;

													const auto& sidDesKey = sidWptKey.at(sidDes.key());

													for (auto& sidId : sidDesKey.items())
													{
														if (sidId.key() == "rwy")
															idSetting.rwys = vsid::utils::split(sidDesKey.at(sidId.key()), ',');
														else if (sidId.key() == "prio")
															idSetting.prio = sidDesKey.at(sidId.key());
														else if (sidId.key() == "allowDiffNumbers")
															idSetting.allowDiffNumbers = sidDesKey.at(sidId.key());
														else if (sidId.key() == "initial")
															idSetting.initial = sidDesKey.at(sidId.key());
														else if (sidId.key() == "climbvia")
															idSetting.via = sidDesKey.at(sidId.key());
														else if (sidId.key() == "wpt")
															idSetting.wpt = sidDesKey.at(sidId.key());
														else if (sidId.key() == "trans")
														{
															idSetting.transition.clear();

															for (auto& base : sidDesKey.at(sidId.key()).items())
															{
																vsid::Transition trans;

																trans.base = base.key();
																if (std::string desig = sidWptKey
																	.at(sidDes.key()).at(sidId.key()).at(base.key()); desig != "XXX")
																	trans.designator = desig;

																idSetting.transition.insert({ base.key(), trans });
															}

															if (sidWptKey
																.at(sidDes.key()).at(sidId.key()).size() == 0) idSetting.transition.clear();
														}
														else if (sidId.key() == "pilotfiled")
															idSetting.pilotfiled = sidDesKey.at(sidId.key());
														else if (sidId.key() == "wingType")
															idSetting.wingType = sidDesKey.at(sidId.key());
														else if (sidId.key() == "acftType")
															idSetting.acftType = sidDesKey.at(sidId.key());
														else if (sidId.key() == "dest")
															idSetting.dest = sidDesKey.at(sidId.key());
														else if (sidId.key() == "route")
														{
															if (sidDesKey.at(sidId.key()).contains("allow"))
															{
																idSetting.route["allow"].clear();

																for (const auto& id : sidDesKey.at(sidId.key()).at("allow").items())
																{
																	std::string routeId = id.key();
																	std::vector<std::string> configRoute =
																		vsid::utils::split(sidDesKey.at(sidId.key()).at("allow").value(routeId, ""), ',');

																	if (!configRoute.empty()) idSetting.route["allow"].insert({ routeId, configRoute });
																}
															}

															if (sidDesKey.at(sidId.key()).contains("deny"))
															{
																idSetting.route["deny"].clear();

																for (const auto& id : sidDesKey.at(sidId.key()).at("deny").items())
																{
																	std::string routeId = id.key();
																	std::vector<std::string> configRoute =
																		vsid::utils::split(sidDesKey.at(sidId.key()).at("deny").value(routeId, ""), ',');

																	if (!configRoute.empty()) idSetting.route["deny"].insert({ routeId, configRoute });
																}
															}
														}
														else if (sidId.key() == "wtc")
															idSetting.wtc = sidDesKey.at(sidId.key());
														else if (sidId.key() == "engineType")
															idSetting.engineType = sidDesKey.at(sidId.key());
														else if (sidId.key() == "engineCount")
															idSetting.engineCount = sidDesKey.at(sidId.key());
														else if (sidId.key() == "mtow")
															idSetting.mtow = sidDesKey.at(sidId.key());
														else if (sidId.key() == "customRule")
															idSetting.customRule = vsid::utils::toupper(sidDesKey.at(sidId.key()));
														else if (sidId.key() == "area")
															idSetting.area = vsid::utils::toupper(sidDesKey.at(sidId.key()));
														else if (sidId.key() == "equip")
														{
															idSetting.equip = sidDesKey.at(sidId.key());

															// updating equipment codes to upper case if in lower case

															for (std::map<std::string, bool>::iterator it = idSetting.equip.begin(); it != idSetting.equip.end();)
															{
																if (it->first != vsid::utils::toupper(it->first))
																{
																	std::pair<std::string, bool> cap = { vsid::utils::toupper(it->first), it->second };
																	it = idSetting.equip.erase(it);
																	idSetting.equip.insert(it, cap);
																	continue;
																}
																++it;
															}
														}
														else if (sidId.key() == "lvp")
															idSetting.lvp = sidDesKey.at(sidId.key());
														else if (sidId.key() == "actArrRwy")
														{
															if (sidDesKey.at(sidId.key()).contains("allow"))
															{
																idSetting.actArrRwy["allow"]["all"] =
																	sidDesKey.at(sidId.key())
																	.at("allow").value("all", "");
																idSetting.actArrRwy["allow"]["any"] =
																	sidDesKey.at(sidId.key())
																	.at("allow").value("any", "");
															}

															if (sidDesKey.at(sidId.key()).contains("deny"))
															{
																idSetting.actArrRwy["deny"]["all"] =
																	sidDesKey.at(sidId.key())
																	.at("deny").value("all", "");
																idSetting.actArrRwy["deny"]["any"] =
																	sidDesKey.at(sidId.key())
																	.at("deny").value("any", "");
															}
														}
														else if (sidId.key() == "actDepRwy")
														{
															if (sidDesKey.at(sidId.key()).contains("allow"))
															{
																idSetting.actDepRwy["allow"]["all"] =
																	sidDesKey.at(sidId.key())
																	.at("allow").value("all", "");
																idSetting.actDepRwy["allow"]["any"] =
																	sidDesKey.at(sidId.key())
																	.at("allow").value("any", "");
															}

															if (sidDesKey.at(sidId.key()).contains("deny"))
															{
																idSetting.actDepRwy["deny"]["all"] =
																	sidDesKey.at(sidId.key())
																	.at("deny").value("all", "");
																idSetting.actDepRwy["deny"]["any"] =
																	sidDesKey.at(sidId.key())
																	.at("deny").value("any", "");
															}
														}
														else if (sidId.key() == "timeFrom")
															idSetting.timeFrom = sidDesKey.at(sidId.key());
														else if (sidId.key() == "timeTo")
															idSetting.timeTo = sidDesKey.at(sidId.key());
														else if (sidId.key() == "sidHighlight")
															idSetting.sidHighlight = sidDesKey.at(sidId.key());
														else if (sidId.key() == "clmbHighlight")
															idSetting.clmbHighlight = sidDesKey.at(sidId.key());

														if (idSetting.equip.empty()) idSetting.equip["RNAV"] = true;
													}

													// save new sid

													vsid::Sid newSid = { idSetting.base, idSetting.wpt, idSetting.id, fixedNumber, idSetting.desig, idSetting.rwys, idSetting.transition,
																		idSetting.allowDiffNumbers, idSetting.equip, idSetting.initial, idSetting.via, idSetting.prio, idSetting.pilotfiled,
																		idSetting.actArrRwy, idSetting.actDepRwy, idSetting.wtc, idSetting.engineType, idSetting.wingType,
																		idSetting.acftType, idSetting.engineCount, idSetting.mtow, idSetting.dest, idSetting.route,
																		idSetting.customRule, idSetting.area, idSetting.lvp, idSetting.timeFrom, idSetting.timeTo,
																		idSetting.sidHighlight, idSetting.clmbHighlight };
													
													data.sids.push_back(newSid);
													
													if (newSid.timeFrom != -1 && newSid.timeTo != -1) data.timeSids.push_back(newSid);

													// #dev - debugging msgs for evaluation of sid restriction levels

													std::string sidName = std::format("{}{}{} (ID: {})", newSid.base, (newSid.number.empty()) ? "_" : newSid.number, newSid.designator, newSid.id);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] wpt: {}", sidName, newSid.waypoint), vsid::DebugLevel::Conf);
													for (auto& [_, trans] : newSid.transition)
													{
														vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] trans: {}", sidName, trans.base + "_" + trans.designator), vsid::DebugLevel::Conf);
													}
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] rwys: {}", sidName, vsid::utils::join(newSid.rwys, ",")), vsid::DebugLevel::Conf);
													for (auto& [sEquip, allow] : newSid.equip)
													{
														vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] equip: {} allowed {}", sidName, sEquip, (allow) ? "TRUE" : "FALSE"), vsid::DebugLevel::Conf);
													}
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] initialClimb: {}", sidName, newSid.initialClimb), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] climb via: {}", sidName, (newSid.climbvia) ? "TRUE" : "FALSE"), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] prio: {}", sidName, newSid.prio), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] pilotfiled: {}", sidName, (newSid.pilotfiled) ? "TRUE" : "FALSE"), vsid::DebugLevel::Conf);
													for (auto& [actArrList, arrType] : newSid.actArrRwy)
													{
														for (auto& [arrWhich, actArr] : arrType)
														{
															vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] actArrRwy: {} - {} - {}", sidName, actArrList, arrWhich, actArr), vsid::DebugLevel::Conf);
														}

													}
													for (auto& [actDepList, depType] : newSid.actDepRwy)
													{
														for (auto& [depWhich, actDep] : depType)
														{
															vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] actDepRwy: {} - {} - {}", sidName, actDepList, depWhich, actDep), vsid::DebugLevel::Conf);
														}

													}
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] wtc: {}", sidName, newSid.wtc), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] engType: {}", sidName, newSid.engineType), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] wingType: {}", sidName, newSid.wingType), vsid::DebugLevel::Conf);
													for (auto& [sAcftType, allow] : newSid.acftType)
													{
														vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] acftType: {} allowed {}", sidName, sAcftType, (allow) ? "TRUE" : "FALSE"), vsid::DebugLevel::Conf);
													}
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] engCount: {}", sidName, newSid.engineCount), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] mtow: {}", sidName, newSid.mtow), vsid::DebugLevel::Conf);
													for (auto& [sDest, allow] : newSid.dest)
													{
														vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] dest: {} allow {}", sidName, sDest, (allow) ? "TRUE" : "FALSE"), vsid::DebugLevel::Conf);
													}
													for (auto& [allow, routeList] : newSid.route)
													{
														for (auto& [sId, sRoute] : routeList)
														{
															vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] route [{}] id [{}] routing [{}]",
																sidName, allow, sId, vsid::utils::join(sRoute, ",")), vsid::DebugLevel::Conf);
														}
													}
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] rule: {}", sidName, newSid.customRule), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] area: {}", sidName, newSid.area), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] lvp: {}", sidName, newSid.lvp), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] timeFrom: {}", sidName, newSid.timeFrom), vsid::DebugLevel::Conf);
													vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] timeTo: {}", sidName, newSid.timeTo), vsid::DebugLevel::Conf);
													// end dev - debugging msgs for sid restriction levels
												}
											}
										}
									}
								}
							}
						});

					if (updated)
					{
						vsid::Logger::log(
							vsid::LogLevel::Debug,
							std::format("[{}] airport updated successfully", icao),
							vsid::DebugLevel::Conf
						);
					}
				}
				catch (const json::parse_error& e)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("[Parse] Failed to load airport config ({}): {}", icao, e.what()));
				}
				catch (const json::type_error& e)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("[Type] Failed to load airport config ({}): {}", icao, e.what()));
				}
				catch (const json::out_of_range& e)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("[Range] Failed to load airport config ({}): {}", icao, e.what()));
				}
				catch (const json::other_error& e)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("[Other] Failed to load airport config ({}): {}", icao, e.what()));
				}
				catch (const std::exception &e)
				{
					vsid::Logger::log(vsid::LogLevel::Error, std::format("Failure in config ({}): {}", icao, e.what()));
				}

				/* DOCUMENTATION on how to get all values below a key
				json waypoint = this->configFile.at("EDDF").at("sids").at("MARUN");
				for (auto it : waypoint.items())
				{
					vsid::messagehandler::LogMessage("JSON it:", it.value().dump());
				}*/
			}
		}
	}

	// airport health check - remove apt without config

	const auto& airports = AirportManager::getAirports();

	for (auto it = airports.begin(); it != airports.end();)
	{
		if (aptConfig.contains(it->first))
		{
			++it;
			continue;
		}

		std::string icao = it->first;
		++it; // increase iterator before removing from map

		vsid::Logger::log(vsid::LogLevel::Info, "No config found for: " + icao);

		AirportManager::remove(icao);
	}
}

void vsid::ConfigParser::loadGrpConfig()
{
	// get the current path where plugins .dll is stored
	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;

	if (!this->vSidConfig.empty())
	{
		basePath.append(this->vSidConfig.value("grp", "")).make_preferred();
	}

	if (!std::filesystem::exists(basePath))
	{
		vsid::Logger::log(vsid::LogLevel::Error, "No grp config found in: " + basePath.string());
		return;
	}
	for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
	{
		if (entry.extension() == ".json")
		{
			if (entry.filename().string() != "ICAO_Aircraft.json") continue;

			std::ifstream configFile(entry.string());

			try
			{
				this->grpConfig = json::parse(configFile);
			}
			catch (const json::parse_error& e)
			{
				vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to load grp config: {}", e.what()));
			}
			catch (const json::type_error& e)
			{
				vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to load grp config: {}", e.what()));
			}
		}
	}
}

void vsid::ConfigParser::loadRnavList()
{
	// get the current path where plugins .dll is stored
	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;

	if (!this->vSidConfig.empty())
	{
		basePath.append(this->vSidConfig.value("RNAV", "")).make_preferred();
	}

	if (!std::filesystem::exists(basePath))
	{
		vsid::Logger::log(vsid::LogLevel::Error, "Path to check for RNAV List does not exist: " + basePath.string());
		return;
	}

	for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
	{
		if (entry.extension() != ".json") continue;
		if (entry.filename().string() != "RNAV_List.json") continue;

		std::ifstream configFile(entry.string());
		json rnavConfigFile;

		try
		{
			rnavConfigFile = json::parse(configFile);
		}
		catch (const json::parse_error& e)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to load rnav list: {}", e.what()));
		}
		catch (const json::type_error& e)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to load rnav list: {}", e.what()));
		}
		
		if (rnavConfigFile.empty())
		{
			vsid::Logger::log(vsid::LogLevel::Error, "RNAV List is empty. Is it present besides the plugin DLL file?");
			return;
		}

		try
		{
			this->rnavList = rnavConfigFile.value("RNAV", std::set<std::string>{});
		}
		catch (json::type_error &e)
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to read rnav list: {}", e.what()));
		}

		return;
	}
	vsid::Logger::log(vsid::LogLevel::Error, "No RNAV capable list found at: " + basePath.string());
}

const COLORREF vsid::ConfigParser::getColor(const std::string& color)
{
	if (auto it = this->colors.find(color); it != this->colors.end())
	{
		if (!messageHandler->genErrorsEmpty()) messageHandler->removeGenError(ERROR_CONF_COLOR + "_" + color);

		return it->second;
	}
	else
	{
		if (!messageHandler->genErrorsContains(ERROR_CONF_COLOR + "_" + color))
		{
			vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to retrieve color: [{}]. Code: {}", color, ERROR_CONF_COLOR));
			messageHandler->addGenError(ERROR_CONF_COLOR + "_" + color);
		}
		// return purple if color could not be found to signal error
		COLORREF rgbColor = RGB(190, 30, 190);
		return rgbColor;
	}
}

int vsid::ConfigParser::getReqTime(const std::string& time)
{
	if (auto it = this->reqTimes.find(time); it != this->reqTimes.end())
	{
		return it->second;
	}

	vsid::Logger::log(vsid::LogLevel::Error, std::format("Failed to retrieve request time setting for key [{}]", time));
	return 0;
}
