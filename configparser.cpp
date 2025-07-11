#include "pch.h"

#include "configparser.h"
#include "utils.h"
#include "messageHandler.h"
#include "sid.h"
#include "constants.h"

#include <vector>
#include <fstream>

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
        messageHandler->writeMessage("ERROR", "Failed to parse main config: " + std::string(e.what()));
    }
    catch (const json::type_error& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to parse main config: " + std::string(e.what()));
    }
    catch (const json::other_error& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to parse main config: " + std::string(e.what()));
    }

    if (this->vSidConfig.is_null())
    {
        messageHandler->writeMessage("ERROR", "Failed to parse main config. (Critical!)");
        return;
    }

	// #dev
	this->loadEse();
	// end dev

    // set topsky preference

    this->preferTopsky = this->vSidConfig.value("preferTopsky", true);

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

        // pseudo values for special color use cases
        if (!this->colors.contains("squawkSet")) this->colors["squawkSet"] = RGB(300, 300, 300);
    }
    catch (std::error_code& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to import colors: " + e.message());
    }
    catch (const json::parse_error& e)
    {
        messageHandler->writeMessage("ERROR", "[Parse Error] in color config section: " + std::string(e.what()));
    }
    catch (const json::type_error& e)
    {
        messageHandler->writeMessage("ERROR", "[Type Error] in color config section: " + std::string(e.what()));
    }
    catch (const json::other_error& e)
    {
        messageHandler->writeMessage("ERROR", "[Other Error] in color config section: " + std::string(e.what()));
    }
    catch (const json::out_of_range& e)
    {
        messageHandler->writeMessage("ERROR", "[Out of Range] in color config section: " + std::string(e.what()));
    }

    // get request times

    try
    {
        this->reqTimes.insert({ "caution", this->vSidConfig.at("requests").value("caution", 2) });
        this->reqTimes.insert({ "warning", this->vSidConfig.at("requests").value("warning", 5) });
    }
    catch (json::parse_error& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to get request timers: " + std::string(e.what()));
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
        messageHandler->writeMessage("ERROR", "Failed to get clrf min values: " + std::string(e.what()));
    }
}

void vsid::ConfigParser::loadAirportConfig(std::map<std::string, vsid::Airport> &activeAirports,
                                        std::map<std::string, std::map<std::string, bool>>& savedCustomRules,
                                        std::map<std::string, std::map<std::string, bool>>& savedSettings,
                                        std::map<std::string, std::map<std::string, vsid::Area>>& savedAreas,
                                        std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>& savedRequests,
                                        std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>>& savedRwyRequests
                                        )
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
        messageHandler->writeMessage("ERROR", "No config path for airports in main config");
        return;
    }

    if (!std::filesystem::exists(basePath))
    {
        messageHandler->writeMessage("ERROR", "No airport config folder found at: " + basePath.string());
        return;
    }

    std::vector<std::filesystem::path> files;
    std::set<std::string> aptConfig;

   /* for (const std::filesystem::path& entry : std::filesystem::recursive_directory_iterator(basePath)) // needs further #evaluation - can cause slow loading
    {
        if (!std::filesystem::is_directory(entry) && entry.extension() == ".json")
        {
            this->configPaths.insert(entry);
        }
    }*/

    for (auto &[icao, aptInfo] : activeAirports)
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
                    else
                    {
                        aptConfig.insert(icao);

                        // general settings

                        aptInfo.icao = icao;
                        aptInfo.elevation = this->parsedConfig.at(icao).value("elevation", 0);
                        aptInfo.equipCheck = this->parsedConfig.at(icao).value("equipCheck", true);
                        aptInfo.enableRVSids = this->parsedConfig.at(icao).value("enableRVSids", true);
                        aptInfo.allRwys = vsid::utils::split(this->parsedConfig.at(icao).value("runways", ""), ',');
                        aptInfo.transAlt = this->parsedConfig.at(icao).value("transAlt", 0);
                        aptInfo.maxInitialClimb = this->parsedConfig.at(icao).value("maxInitialClimb", 0);
                        aptInfo.timezone = this->parsedConfig.at(icao).value("timezone", "Etc/UTC");
                        aptInfo.requests["clearance"] = {};
                        aptInfo.requests["startup"] = {};
                        aptInfo.requests["pushback"] = {};
                        aptInfo.requests["taxi"] = {};
                        aptInfo.requests["departure"] = {};
                        aptInfo.requests["vfr"] = {};
                        aptInfo.rwyrequests["rwy startup"] = {};

                        // customRules

                        std::map<std::string, bool> customRules;
                        for (auto &el : this->parsedConfig.at(icao).value("customRules", std::map<std::string, bool>{}))
                        {
                            std::pair<std::string, bool> rule = { vsid::utils::toupper(el.first), el.second };
                            customRules.insert(rule);
                        }
                        // overwrite loaded rule settings from config with current values at the apt

                        if (savedCustomRules.contains(icao))
                        {
                            for (std::pair<const std::string, bool>& rule : savedCustomRules[icao])
                            {
                                if (customRules.contains(rule.first))
                                {
                                    customRules[rule.first] = rule.second;
                                }
                            }
                        }
                        aptInfo.customRules = customRules;                        

                        std::set<std::string> appSI;
                        int appSIPrio = 0;
                        for (std::string& si : vsid::utils::split(this->parsedConfig.at(icao).value("appSI", ""), ','))
                        {
                            aptInfo.appSI[si] = appSIPrio;
                            appSIPrio++;
                        }
                        
                        //areas

                        if (this->parsedConfig.at(icao).contains("areas"))
                        {
                            for (auto& area : this->parsedConfig.at(icao).at("areas").items())
                            {
                                std::vector<std::pair<std::string, std::string>> coords;
                                bool isActive = false;
                                bool arrAsDep = false;
                                for (auto& coord : this->parsedConfig.at(icao).at("areas").at(area.key()).items())
                                {
                                    if (coord.key() == "active")
                                    {
                                        isActive = this->parsedConfig.at(icao).at("areas").at(area.key()).value("active", false);
                                        continue;
                                    }
                                    else if (coord.key() == "arrAsDep")
                                    {
                                        arrAsDep = this->parsedConfig.at(icao).at("areas").at(area.key()).value("arrAsDep", false);
                                        continue;
                                    }
                                    std::string lat = this->parsedConfig.at(icao).at("areas").at(area.key()).at(coord.key()).value("lat", "");
                                    std::string lon = this->parsedConfig.at(icao).at("areas").at(area.key()).at(coord.key()).value("lon", "");

                                    if (lat == "" || lon == "")
                                    {
                                        messageHandler->writeMessage("ERROR", "Couldn't read LAT or LON value for \"" +
                                            coord.key() + "\" in area \"" + area.key() + "\" at \"" +
                                            icao + "\"");
                                        break;
                                    }
                                    coords.push_back({ lat, lon });
                                }
                                if (coords.size() < 3)
                                {
                                    messageHandler->writeMessage("ERROR", "Area \"" + area.key() + "\" in \"" +
                                        icao + "\" has not enough points configured (less than 3).");
                                    continue;
                                }
                                if (savedAreas.contains(icao))
                                {
                                    if (savedAreas[icao].contains(vsid::utils::toupper(area.key())))
                                    {
                                        isActive = savedAreas[icao][vsid::utils::toupper(area.key())].isActive;
                                    }
                                }
                                aptInfo.areas.insert({ vsid::utils::toupper(area.key()), vsid::Area{coords, isActive, arrAsDep} });
                            }
                        }

                        // airport settings
                        if (savedSettings.contains(icao))
                        {
                            aptInfo.settings = savedSettings[icao];
                        }
                        else
                        {
                            aptInfo.settings = { {"lvp", false},
                                                    {"time", this->parsedConfig.at(icao).value("timeMode", false)},
                                                    {"auto", false}
                            };
                        }

                        // saved requests - if not found base settings already in general settings

                        if (savedRequests.contains(icao)) aptInfo.requests = savedRequests[icao];

                        // saved rwy requests - if not found base settings already in general settings

                        if (savedRwyRequests.contains(icao)) aptInfo.rwyrequests = savedRwyRequests[icao];

                        // sids
						// initialize default values

                        vsid::tmpSidSettings fieldSetting;
                        vsid::tmpSidSettings wptSetting;
                        vsid::tmpSidSettings desSetting;
                        vsid::tmpSidSettings idSetting;

                        // "field level" - iterates over restrictions and sid way points / bases

                        for (auto &sidField : this->parsedConfig.at(icao).at("sids").items())
                        {
                            if (sidField.key() == "initial") fieldSetting.initial = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "climbvia") fieldSetting.via = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "wpt") fieldSetting.wpt = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "pilotfiled") fieldSetting.pilotfiled = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "wingType") fieldSetting.wingType = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "acftType") fieldSetting.acftType = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "dest") fieldSetting.dest = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "route")
                            {
								if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("allow"))
								{
									for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").items())
									{
										std::string routeId = id.key();
										std::vector<std::string> configRoute =
											vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").value(routeId, ""), ',');

										if (!configRoute.empty()) fieldSetting.route["allow"].insert({ routeId, configRoute });
									}
								}

								if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("deny"))
								{
									for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").items())
									{
										std::string routeId = id.key();
										std::vector<std::string> configRoute =
											vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").value(routeId, ""), ',');

										if (!configRoute.empty()) fieldSetting.route["deny"].insert({ routeId, configRoute });
									}
								}
                            }
                            else if (sidField.key() == "wtc") fieldSetting.wtc = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "engineType") fieldSetting.engineType = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "engineCount") fieldSetting.engineCount = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "mtow") fieldSetting.mtow = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "customRule") fieldSetting.customRule = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "area") fieldSetting.area = vsid::utils::toupper(this->parsedConfig.at(icao).at("sids").at(sidField.key()));
                            else if (sidField.key() == "equip")
                            {
                                fieldSetting.equip = this->parsedConfig.at(icao).at("sids").at(sidField.key());

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
                            else if (sidField.key() == "lvp") fieldSetting.lvp = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "actArrRwy")
                            {
                                if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("allow"))
                                {
									fieldSetting.actArrRwy["allow"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").value("all", "");
									fieldSetting.actArrRwy["allow"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").value("any", "");
                                }

								if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("deny"))
								{
									fieldSetting.actArrRwy["deny"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").value("all", "");
									fieldSetting.actArrRwy["deny"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").value("any", "");
								}
                            }
                            else if (sidField.key() == "actDepRwy")
                            {
								if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("allow"))
								{
									fieldSetting.actDepRwy["allow"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").value("all", "");
									fieldSetting.actDepRwy["allow"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("allow").value("any", "");
								}

								if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).contains("deny"))
								{
									fieldSetting.actDepRwy["deny"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").value("all", "");
									fieldSetting.actDepRwy["deny"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at("deny").value("any", "");
								}
                            }
                            else if (sidField.key() == "timeFrom") fieldSetting.timeFrom = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "timeTo") fieldSetting.timeTo = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "sidHighlight") fieldSetting.sidHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (sidField.key() == "clmbHighlight") fieldSetting.clmbHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key());
                            else if (!this->isConfigValue(sidField.key()))
                            {
                                fieldSetting.base = sidField.key();
                                fieldSetting.wpt = fieldSetting.base; // #evaluate - remove from field settings and always overwrite in wptSettings?

                                // "waypoint / base level" - iterates over restrictions and sid designators

                                wptSetting = fieldSetting;

                                for (auto& sidWpt : this->parsedConfig.at(icao).at("sids").at(sidField.key()).items())
                                {
                                    if (sidWpt.key() == "initial") wptSetting.initial = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
                                    else if(sidWpt.key() == "climbvia") wptSetting.via = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
                                    else if (sidWpt.key() == "wpt") wptSetting.wpt = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "trans")
									{
                                        wptSetting.transition.clear();

										for (auto& base : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).items())
										{
											vsid::Transition trans;

											trans.base = base.key();
											trans.designator =
												this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(base.key());

											wptSetting.transition.insert({ base.key(), trans });
										}
									}
                                    else if(sidWpt.key() == "pilotfiled") wptSetting.pilotfiled = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
                                    else if (sidWpt.key() == "wingType") wptSetting.wingType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "acftType") wptSetting.acftType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "dest") wptSetting.dest = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "route")
									{
										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("allow"))
										{
                                            wptSetting.route["allow"].clear();

											for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").items())
											{
												std::string routeId = id.key();
												std::vector<std::string> configRoute =
													vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").value(routeId, ""), ',');

												if (!configRoute.empty()) wptSetting.route["allow"].insert({ routeId, configRoute });
											}
										}

										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("deny"))
										{
                                            wptSetting.route["deny"].clear();

											for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").items())
											{
												std::string routeId = id.key();
												std::vector<std::string> configRoute =
													vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").value(routeId, ""), ',');

												if (!configRoute.empty()) wptSetting.route["deny"].insert({ routeId, configRoute });
											}
										}
									}
									else if (sidWpt.key() == "wtc") wptSetting.wtc = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "engineType") wptSetting.engineType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "engineCount") wptSetting.engineCount = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "mtow") wptSetting.mtow = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "customRule") wptSetting.customRule = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "area") wptSetting.area = vsid::utils::toupper(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()));
									else if (sidWpt.key() == "equip")
									{
                                        wptSetting.equip = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());

										// updating equipment codes to upper case if in lower case

										for (std::map<std::string, bool>::iterator it = wptSetting.equip.begin(); it != wptSetting.equip.end();)
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
									else if (sidWpt.key() == "lvp") wptSetting.lvp = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "actArrRwy")
									{
										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("allow"))
										{
                                            wptSetting.actArrRwy["allow"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").value("all", "");
                                            wptSetting.actArrRwy["allow"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").value("any", "");
										}

										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("deny"))
										{
                                            wptSetting.actArrRwy["deny"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").value("all", "");
                                            wptSetting.actArrRwy["deny"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").value("any", "");
										}
									}
									else if (sidWpt.key() == "actDepRwy")
									{
										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("allow"))
										{
											wptSetting.actDepRwy["allow"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").value("all", "");
											wptSetting.actDepRwy["allow"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("allow").value("any", "");
										}

										if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).contains("deny"))
										{
											wptSetting.actDepRwy["deny"]["all"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").value("all", "");
											wptSetting.actDepRwy["deny"]["any"] = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at("deny").value("any", "");
										}
									}
									else if (sidWpt.key() == "timeFrom") wptSetting.timeFrom = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "timeTo") wptSetting.timeTo = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "sidHighlight") wptSetting.sidHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
									else if (sidWpt.key() == "clmbHighlight") wptSetting.clmbHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key());
                                    else if (!this->isConfigValue(sidWpt.key()))
                                    {
                                        if(sidWpt.key().find_first_of("0123456789") == std::string::npos) wptSetting.desig = sidWpt.key();

                                        // "designator level" - iterates over restrictions and sid ids

                                        desSetting = wptSetting;

                                        for (auto& sidDes : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).items())
                                        {
                                            if (sidDes.key() == "rwy")
                                                desSetting.rwys = vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()), ',');
											else if (sidDes.key() == "initial")
                                                desSetting.initial = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "climbvia")
                                                desSetting.via = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
                                            else if (sidDes.key() == "wpt")
                                                desSetting.wpt = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "trans")
											{
                                                desSetting.transition.clear();

												for (auto& base : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).items())
												{
													vsid::Transition trans;

													trans.base = base.key();
													trans.designator =
														this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(base.key());

													desSetting.transition.insert({ base.key(), trans });
												}

                                                if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key())
                                                    .at(sidDes.key()).size() == 0) desSetting.transition.clear();
											}
											else if (sidDes.key() == "pilotfiled")
                                                desSetting.pilotfiled = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "wingType")
												desSetting.wingType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "acftType")
                                                desSetting.acftType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "dest")
                                                desSetting.dest = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "route")
											{
												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("allow"))
												{
                                                    desSetting.route["allow"].clear();

													for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").items())
													{
														std::string routeId = id.key();
														std::vector<std::string> configRoute =
															vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").value(routeId, ""), ',');

														if (!configRoute.empty()) desSetting.route["allow"].insert({ routeId, configRoute });
													}
												}

												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("deny"))
												{
                                                    desSetting.route["deny"].clear();

													for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").items())
													{
														std::string routeId = id.key();
														std::vector<std::string> configRoute =
															vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").value(routeId, ""), ',');

														if (!configRoute.empty()) desSetting.route["deny"].insert({ routeId, configRoute });
													}
												}
											}
											else if (sidDes.key() == "wtc")
                                                desSetting.wtc = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "engineType")
                                                desSetting.engineType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "engineCount")
                                                desSetting.engineCount = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "mtow")
                                                desSetting.mtow = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "customRule")
                                                desSetting.customRule = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "area")
                                                desSetting.area = vsid::utils::toupper(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()));
											else if (sidDes.key() == "equip")
											{
                                                desSetting.equip = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());

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
                                                desSetting.lvp = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "actArrRwy")
											{
												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("allow"))
												{
                                                    desSetting.actArrRwy["allow"]["all"] =
                                                        this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").value("all", "");
                                                    desSetting.actArrRwy["allow"]["any"] =
                                                        this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").value("any", "");
												}

												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("deny"))
												{
                                                    desSetting.actArrRwy["deny"]["all"] = 
                                                        this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").value("all", "");
                                                    desSetting.actArrRwy["deny"]["any"] =
                                                        this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").value("any", "");
												}
											}
											else if (sidDes.key() == "actDepRwy")
											{
												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("allow"))
												{
													desSetting.actDepRwy["allow"]["all"] =
														this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").value("all", "");
													desSetting.actDepRwy["allow"]["any"] =
														this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("allow").value("any", "");
												}

												if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).contains("deny"))
												{
													desSetting.actDepRwy["deny"]["all"] =
														this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").value("all", "");
													desSetting.actDepRwy["deny"]["any"] =
														this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at("deny").value("any", "");
												}
											}
											else if (sidDes.key() == "timeFrom")
                                                desSetting.timeFrom = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "timeTo")
                                                desSetting.timeTo = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "sidHighlight")
                                                desSetting.sidHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
											else if (sidDes.key() == "clmbHighlight")
                                                desSetting.clmbHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key());
                                            else if (!this->isConfigValue(sidDes.key()))
                                            {
                                                desSetting.id = sidDes.key();

                                                // "id level" - iterates over restrictions on id level (highest priority)

                                                idSetting = desSetting;

                                                for (auto& sidId : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).items())
                                                {
                                                    if (sidId.key() == "rwy")
                                                        idSetting.rwys = vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()), ',');
                                                    else if (sidId.key() == "prio")
                                                        idSetting.prio = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "initial")
                                                        idSetting.initial = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "climbvia")
                                                        idSetting.via = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
													else if (sidId.key() == "wpt")
                                                        idSetting.wpt = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "trans")
                                                    {
                                                        idSetting.transition.clear();

                                                        for (auto &base : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).items())
                                                        {
                                                            vsid::Transition trans;

                                                            trans.base = base.key();
                                                            trans.designator =
                                                                this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).at(base.key());

                                                            idSetting.transition.insert({ base.key(), trans});
                                                        }

                                                        if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key())
                                                            .at(sidDes.key()).at(sidId.key()).size() == 0) idSetting.transition.clear();
                                                    }
                                                    else if (sidId.key() == "pilotfiled")
                                                        idSetting.pilotfiled = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
													else if (sidId.key() == "wingType")
														idSetting.wingType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "acftType")
                                                        idSetting.acftType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "dest")
                                                        idSetting.dest = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "route")
                                                    {
                                                        if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("allow"))
                                                        {
                                                            idSetting.route["allow"].clear();

                                                            for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).at("allow").items())
                                                            {
                                                                std::string routeId = id.key();
                                                                std::vector<std::string> configRoute =
                                                                    vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).at("allow").value(routeId, ""), ',');

                                                                if (!configRoute.empty()) idSetting.route["allow"].insert({ routeId, configRoute });
                                                            }
                                                        }

                                                        if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("deny"))
                                                        {
                                                            idSetting.route["deny"].clear();

                                                            for (const auto& id : this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).at("deny").items())
                                                            {
                                                                std::string routeId = id.key();
                                                                std::vector<std::string> configRoute =
                                                                    vsid::utils::split(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).at("deny").value(routeId, ""), ',');

                                                                if (!configRoute.empty()) idSetting.route["deny"].insert({ routeId, configRoute });
                                                            }
                                                        }
                                                    }
                                                    else if (sidId.key() == "wtc")
                                                        idSetting.wtc = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "engineType")
                                                        idSetting.engineType = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "engineCount")
                                                        idSetting.engineCount = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "mtow")
                                                        idSetting.mtow = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "customRule")
                                                        idSetting.customRule = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "area")
                                                        idSetting.area = vsid::utils::toupper(this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()));
                                                    else if (sidId.key() == "equip")
                                                    {
                                                        idSetting.equip = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());

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
                                                        idSetting.lvp = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "actArrRwy")
                                                    {
														if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("allow"))
														{
                                                            idSetting.actArrRwy["allow"]["all"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
                                                                .at("allow").value("all", "");
                                                            idSetting.actArrRwy["allow"]["any"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
                                                                .at("allow").value("any", "");
														}

														if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("deny"))
														{
                                                            idSetting.actArrRwy["deny"]["all"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
                                                                .at("deny").value("all", "");
                                                            idSetting.actArrRwy["deny"]["any"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
                                                                .at("deny").value("any", "");
														}
                                                    }
                                                    else if (sidId.key() == "actDepRwy")
                                                    {
														if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("allow"))
														{
															idSetting.actDepRwy["allow"]["all"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
																.at("allow").value("all", "");
															idSetting.actDepRwy["allow"]["any"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
																.at("allow").value("any", "");
														}

														if (this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key()).contains("deny"))
														{
															idSetting.actDepRwy["deny"]["all"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
																.at("deny").value("all", "");
															idSetting.actDepRwy["deny"]["any"] =
																this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key())
																.at("deny").value("any", "");
														}
                                                    }
                                                    else if (sidId.key() == "timeFrom")
                                                        idSetting.timeFrom = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
                                                    else if (sidId.key() == "timeTo")
                                                        idSetting.timeTo = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
													else if (sidId.key() == "sidHighlight")
                                                        idSetting.sidHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());
													else if (sidId.key() == "clmbHighlight")
                                                        idSetting.clmbHighlight = this->parsedConfig.at(icao).at("sids").at(sidField.key()).at(sidWpt.key()).at(sidDes.key()).at(sidId.key());

                                                    if (idSetting.equip.empty()) idSetting.equip["RNAV"] = true;
                                                }

                                                // save new sid

												vsid::Sid newSid = { idSetting.base, idSetting.wpt, idSetting.id, "", idSetting.desig, idSetting.rwys, idSetting.transition, idSetting.equip,
                                                                    idSetting.initial, idSetting.via, idSetting.prio, idSetting.pilotfiled, idSetting.actArrRwy,
                                                                    idSetting.actDepRwy, idSetting.wtc, idSetting.engineType, idSetting.wingType, idSetting.acftType, idSetting.engineCount,
																	idSetting.mtow, idSetting.dest, idSetting.route, idSetting.customRule, idSetting.area, idSetting.lvp,
																	idSetting.timeFrom, idSetting.timeTo, idSetting.sidHighlight, idSetting.clmbHighlight };
												aptInfo.sids.push_back(newSid);
												if (newSid.timeFrom != -1 && newSid.timeTo != -1) aptInfo.timeSids.push_back(newSid);

												// #dev - debugging msgs for evaluation of sid restriction levels
												std::string sidName = newSid.base + "_" + newSid.designator + " (ID: " + newSid.id + ")";
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] wpt: " + newSid.waypoint, vsid::MessageHandler::DebugArea::Conf);
                                                for (auto& [_, trans] : newSid.transition)
                                                {
                                                    messageHandler->writeMessage("DEBUG", "[" + sidName + "] trans: " + trans.base + "_" + trans.designator, vsid::MessageHandler::DebugArea::Conf);
                                                }
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] rwys: " + vsid::utils::join(newSid.rwys, ','), vsid::MessageHandler::DebugArea::Conf);
												for (auto& [sEquip, allow] : newSid.equip)
												{
													messageHandler->writeMessage("DEBUG", "[" + sidName + "] equip: " + sEquip + " allowed " + ((allow) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Conf);
												}
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] initial: " + std::to_string(newSid.initialClimb), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] via: " + ((newSid.climbvia) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] prio: " + std::to_string(newSid.prio), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] pilotfiled: " + ((newSid.pilotfiled) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Conf);
												for (auto& [actArrList, arrType] : newSid.actArrRwy)
												{
                                                    for(auto& [arrWhich, actArr] : arrType)
                                                    {
                                                        messageHandler->writeMessage("DEBUG", "[" + sidName + "] actArrRwy: " + actArrList + " - " + arrWhich + " - " + actArr, vsid::MessageHandler::DebugArea::Conf);
                                                    }
													
												}
												for (auto& [actDepList, depType] : newSid.actDepRwy)
												{
                                                    for (auto& [depWhich, actDep] : depType)
                                                    {
                                                        messageHandler->writeMessage("DEBUG", "[" + sidName + "] actDepRwy: " + actDepList + " - " + depWhich + " - " + actDep, vsid::MessageHandler::DebugArea::Conf);
                                                    }
													
												}
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] wtc: " + newSid.wtc, vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] engType: " + newSid.engineType, vsid::MessageHandler::DebugArea::Conf);
                                                messageHandler->writeMessage("DEBUG", "[" + sidName + "] wingType: " + newSid.wingType, vsid::MessageHandler::DebugArea::Conf);
												for (auto& [sAcftType, allow] : newSid.acftType)
												{
													messageHandler->writeMessage("DEBUG", "[" + sidName + "] acftType: " + sAcftType + " allowed " + ((allow) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Conf);
												}
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] engCount: " + std::to_string(newSid.engineCount), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] mtow: " + std::to_string(newSid.mtow), vsid::MessageHandler::DebugArea::Conf);
												for (auto& [sDest, allow] : newSid.dest)
												{
													messageHandler->writeMessage("DEBUG", "[" + sidName + "] dest: " + sDest + " allow " + ((allow) ? "TRUE" : "FALSE"), vsid::MessageHandler::DebugArea::Conf);
												}
												for (auto& [allow, routeList] : newSid.route)
												{
													for (auto& [sId, sRoute] : routeList)
													{
														messageHandler->writeMessage("DEBUG", "[" + sidName + "] route: " + allow + " id: " +
															sId + " routing: " + vsid::utils::join(sRoute, ','), vsid::MessageHandler::DebugArea::Conf);
													}
												}
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] rule: " + newSid.customRule, vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] area: " + newSid.area, vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] lvp: " + std::to_string(newSid.lvp), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] timeFrom: " + std::to_string(newSid.timeFrom), vsid::MessageHandler::DebugArea::Conf);
												messageHandler->writeMessage("DEBUG", "[" + sidName + "] timeTo: " + std::to_string(newSid.timeTo), vsid::MessageHandler::DebugArea::Conf);
												// end dev
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                catch (const json::parse_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Parse] Failed to load airport config (" + icao + "): " + std::string(e.what()));
                }
                catch (const json::type_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Type] Failed to load airport config (" + icao + "): " + std::string(e.what()));
                }
                catch (const json::out_of_range& e)
                {
                    messageHandler->writeMessage("ERROR", "[Range] Failed to load airport config (" + icao + "): " + std::string(e.what()));
                }
                catch (const json::other_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Other] Failed to load airport config (" + icao + "): " + std::string(e.what()));
                }
                catch (const std::exception &e)
                {
                    messageHandler->writeMessage("ERROR", "Failure in config (" + icao + "): " + std::string(e.what()));
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

    for (std::map<std::string, vsid::Airport>::iterator it = activeAirports.begin(); it != activeAirports.end();)
    {
        if (aptConfig.contains(it->first)) ++it;
        else
        {
            messageHandler->writeMessage("INFO", "No config found for: " + it->first);
            it = activeAirports.erase(it);
        }
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
        messageHandler->writeMessage("ERROR", "No grp config found in: " + basePath.string());
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
                messageHandler->writeMessage("ERROR:", "Failed to load grp config: " + std::string(e.what()));
            }
            catch (const json::type_error& e)
            {
                messageHandler->writeMessage("ERROR:", "Failed to load grp config: " + std::string(e.what()));
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
        messageHandler->writeMessage("ERROR", "Path to check for RNAV List does not exist: " + basePath.string());
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
            messageHandler->writeMessage("ERROR:", "Failed to load rnav list: " + std::string(e.what()));
        }
        catch (const json::type_error& e)
        {
            messageHandler->writeMessage("ERROR:", "Failed to load rnav list: " + std::string(e.what()));
        }
        
        if (rnavConfigFile.empty())
        {
            messageHandler->writeMessage("ERROR", "RNAV List is empty. Is it present besides the plugin DLL file?");
            return;
        }

        try
        {
            this->rnavList = rnavConfigFile.value("RNAV", std::set<std::string>{});
        }
        catch (json::type_error &e)
        {
            messageHandler->writeMessage("ERROR", "Failed to read rnav list: " + std::string(e.what()));
        }

        return;
    }
    messageHandler->writeMessage("ERROR", "No RNAV capable list found at: " + basePath.string());
}

// #dev
void vsid::ConfigParser::loadEse()
{
    if (this->vSidConfig.is_null())
    {
		messageHandler->writeMessage("ERROR", "Failed to parse main config. (Critical!)");
		return;
    }

	if (!this->vSidConfig.contains("esePath") || this->vSidConfig.at("esePath") == "")
	{
		messageHandler->writeMessage("ERROR", "No path to .ese file set in main config or it couldn't be loaded.");
		return;
	}

	char path[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA((HINSTANCE)&__ImageBase, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	std::filesystem::path basePath = path;

    std::string esePath = this->vSidConfig.at("esePath");

    basePath.append(esePath).make_preferred();

    messageHandler->writeMessage("DEBUG", "loadEse called, basePath: " + basePath.string(), vsid::MessageHandler::DebugArea::Dev);

    std::ofstream debugLog(basePath / "debug_ese_output.txt");
    debugLog.clear();

	for (const std::filesystem::path& entry : std::filesystem::directory_iterator(basePath))
	{
		messageHandler->writeMessage("DEBUG", "File: " + entry.string(), vsid::MessageHandler::DebugArea::Dev);
        if (entry.extension() == ".ese")
        {
            std::ifstream eseFile(entry.string());

            if (!eseFile.is_open())
            {
                messageHandler->writeMessage("WARNING", "Failed to open .ese file in: " + basePath.string());
                break;
            }

            std::string newLine = "";
            std::vector<std::string> vecLine = {};
            bool sectionPosition = false;

            while (std::getline(eseFile, newLine))
            {
                if (newLine.find("[POSITIONS]") != std::string::npos) sectionPosition = true;
                if (newLine.empty() && sectionPosition) break;
                if (!sectionPosition) continue;

                if (newLine.find("[POSITIONS]") != std::string::npos) continue; // skip begin of section

                vecLine = vsid::utils::split(newLine, ':', true);

                try
                {
                    std::string callsign = vecLine.at(0);
                    std::string freq = vecLine.at(2);
                    std::string si = vecLine.at(3);
                }
                catch (std::out_of_range& e)
                {
                    messageHandler->writeMessage("ERROR", "Failed to retrieve station from .ese file: " + std::string(e.what()));
                }

                for (auto it = vecLine.begin(); it != vecLine.end(); ++it)
                {
                    int pos = std::distance(vecLine.begin(), it);

                    debugLog << "[" << pos << "] " << *it << " | ";
                }

				debugLog << '\n';

                vecLine.clear();
            }

            eseFile.close();

            messageHandler->writeMessage("DEBUG", "File should be closed.", vsid::MessageHandler::DebugArea::Dev);

            break; // no further checks necessary
        }
	}

    debugLog.close();
}
// end dev

const COLORREF vsid::ConfigParser::getColor(std::string color)
{
    if (this->colors.contains(color))
    {
        messageHandler->removeGenError(ERROR_CONF_COLOR + "_" + color);

        return this->colors[color];
    }
    else
    {
        if (!messageHandler->genErrorsContains(ERROR_CONF_COLOR + "_" + color))
        {
            messageHandler->writeMessage("ERROR", "Failed to retrieve color: \"" + color + "\". Code: " + ERROR_CONF_COLOR);
            messageHandler->addGenError(ERROR_CONF_COLOR + "_" + color);
        }
        // return purple if color could not be found to signal error
        COLORREF rgbColor = RGB(190, 30, 190);
        return rgbColor;
    }
}

int vsid::ConfigParser::getReqTime(std::string time)
{
    if (this->reqTimes.contains(time))
    {
        return this->reqTimes[time];
    }
    else
    {
        messageHandler->writeMessage("ERROR", "Failed to retrieve request time setting for key \"" + time + "\"");
        return 0;
    }
}