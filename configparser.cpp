#include "pch.h"

#include "configparser.h"
#include "utils.h"
#include "messageHandler.h"
#include "sid.h"

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

    try
    {
        for (auto &elem : this->vSidConfig.at("colors").items())
        {
            // default values are blue to signal if the import went wrong
            COLORREF rgbColor = RGB(
                this->vSidConfig.at("colors").at(elem.key()).value("r", 60),
                this->vSidConfig.at("colors").at(elem.key()).value("g", 80),
                this->vSidConfig.at("colors").at(elem.key()).value("b", 240)
                );
            this->colors[elem.key()] = rgbColor;
        }

        // pseudo values for special color use cases
        if (!this->colors.contains("squawkSet")) this->colors["squawkSet"] = RGB(300, 300, 300);
    }
    catch (std::error_code& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to import colors: " + e.message());
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
                                        std::map<std::string, std::map<std::string, std::set<std::pair<std::string, long long>, vsid::Airport::compreq>>>& savedRequests
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

   /* for (const std::filesystem::path& entry : std::filesystem::recursive_directory_iterator(basePath)) // needs further evaluation - can cause slow loading
    {
        if (!std::filesystem::is_directory(entry) && entry.extension() == ".json")
        {
            this->configPaths.insert(entry);
        }
    }*/

    for (std::pair<const std::string, vsid::Airport> &apt : activeAirports)
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

                    if (!this->parsedConfig.contains(apt.first)) continue;
                    else
                    {
                        aptConfig.insert(apt.first);

                        // general settings

                        apt.second.icao = apt.first;
                        apt.second.elevation = this->parsedConfig.at(apt.first).value("elevation", 0);
                        apt.second.equipCheck = this->parsedConfig.at(apt.first).value("equipCheck", true);
                        apt.second.enableRVSids = this->parsedConfig.at(apt.first).value("enableRVSids", true);
                        apt.second.allRwys = vsid::utils::split(this->parsedConfig.at(apt.first).value("runways", ""), ',');
                        apt.second.transAlt = this->parsedConfig.at(apt.first).value("transAlt", 0);
                        apt.second.maxInitialClimb = this->parsedConfig.at(apt.first).value("maxInitialClimb", 0);
                        apt.second.timezone = this->parsedConfig.at(apt.first).value("timezone", "Etc/UTC");
                        apt.second.requests["clearance"] = {};
                        apt.second.requests["startup"] = {};
                        apt.second.requests["pushback"] = {};
                        apt.second.requests["taxi"] = {};
                        apt.second.requests["departure"] = {};
                        apt.second.requests["vfr"] = {};                     

                        // customRules

                        std::map<std::string, bool> customRules;
                        for (auto &el : this->parsedConfig.at(apt.first).value("customRules", std::map<std::string, bool>{}))
                        {
                            std::pair<std::string, bool> rule = { vsid::utils::toupper(el.first), el.second };
                            customRules.insert(rule);
                        }
                        // overwrite loaded rule settings from config with current values at the apt

                        if (savedCustomRules.contains(apt.first))
                        {
                            for (std::pair<const std::string, bool>& rule : savedCustomRules[apt.first])
                            {
                                if (customRules.contains(rule.first))
                                {
                                    customRules[rule.first] = rule.second;
                                }
                            }
                        }
                        apt.second.customRules = customRules;                        

                        std::set<std::string> appSI;
                        int appSIPrio = 0;
                        for (std::string& si : vsid::utils::split(this->parsedConfig.at(apt.first).value("appSI", ""), ','))
                        {
                            apt.second.appSI[si] = appSIPrio;
                            appSIPrio++;
                        }
                        
                        //areas
                        if (this->parsedConfig.at(apt.first).contains("areas"))
                        {
                            for (auto& area : this->parsedConfig.at(apt.first).at("areas").items())
                            {
                                std::vector<std::pair<std::string, std::string>> coords;
                                bool isActive = false;
                                bool arrAsDep = false;
                                for (auto& coord : this->parsedConfig.at(apt.first).at("areas").at(area.key()).items())
                                {
                                    if (coord.key() == "active")
                                    {
                                        isActive = this->parsedConfig.at(apt.first).at("areas").at(area.key()).value("active", false);
                                        continue;
                                    }
                                    else if (coord.key() == "arrAsDep")
                                    {
                                        arrAsDep = this->parsedConfig.at(apt.first).at("areas").at(area.key()).value("arrAsDep", false);
                                        continue;
                                    }
                                    std::string lat = this->parsedConfig.at(apt.first).at("areas").at(area.key()).at(coord.key()).value("lat", "");
                                    std::string lon = this->parsedConfig.at(apt.first).at("areas").at(area.key()).at(coord.key()).value("lon", "");

                                    if (lat == "" || lon == "")
                                    {
                                        messageHandler->writeMessage("ERROR", "Couldn't read LAT or LON value for \"" +
                                            coord.key() + "\" in area \"" + area.key() + "\" at \"" +
                                            apt.first + "\"");
                                        break;
                                    }
                                    coords.push_back({ lat, lon });
                                }
                                if (coords.size() < 3)
                                {
                                    messageHandler->writeMessage("ERROR", "Area \"" + area.key() + "\" in \"" +
                                        apt.first + "\" has not enough points configured (less than 3).");
                                    continue;
                                }
                                if (savedAreas.contains(apt.first))
                                {
                                    if (savedAreas[apt.first].contains(vsid::utils::toupper(area.key())))
                                    {
                                        isActive = savedAreas[apt.first][vsid::utils::toupper(area.key())].isActive;
                                    }
                                }
                                apt.second.areas.insert({ vsid::utils::toupper(area.key()), vsid::Area{coords, isActive, arrAsDep} });
                            }
                        }

                        // airport settings
                        if (savedSettings.contains(apt.first))
                        {
                            apt.second.settings = savedSettings[apt.first];
                        }
                        else
                        {
                            apt.second.settings = { {"lvp", false},
                                                    {"time", this->parsedConfig.at(apt.first).value("timeMode", false)},
                                                    {"auto", false}
                            };
                        }

                        // saved requests - if not found base settings already in general settings
                        if (savedRequests.contains(apt.first))
                        {
                            apt.second.requests = savedRequests[apt.first];
                        }

                        // sids
                        for (auto &sid : this->parsedConfig.at(apt.first).at("sids").items())
                        {
                            std::string base = sid.key();

                            for (auto& sidWpt : this->parsedConfig.at(apt.first).at("sids").at(sid.key()).items())
                            {
                                // defaults to SID base if no waypoint is configured
                                std::string wpt = vsid::utils::toupper(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("wpt", base));
                                std::string id = sidWpt.key();
                                std::string desig = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("designator", "");
                                std::vector<std::string> rwys = vsid::utils::split(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("rwy", ""), ',');
                                std::map<std::string, bool> equip = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("equip", std::map<std::string, bool>{});
                                
                                // updating equipment codes to upper case if in lower case
                                for (std::map<std::string, bool>::iterator it = equip.begin(); it != equip.end();)
                                {
                                    if (it->first != vsid::utils::toupper(it->first))
                                    {
                                        std::pair<std::string, bool> cap = { vsid::utils::toupper(it->first), it->second };
                                        it = equip.erase(it);
                                        equip.insert(it, cap);
                                        continue;
                                    }
                                    ++it;
                                }

                                if (equip.empty()) equip["RNAV"] = true;

                                int initial = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("initial", 0);
                                bool via = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("climbvia", false);
                                int prio = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("prio", 99);
                                bool pilotfiled = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("pilotfiled", false);
                                std::map<std::string, std::string> actArrRwy;

                                if (this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).contains("actArrRwy"))
                                {
                                    actArrRwy["all"] = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("actArrRwy").value("all", "");
                                    actArrRwy["any"] = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("actArrRwy").value("any", "");
                                }

                                std::map<std::string, std::string> actDepRwy;

                                if (this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).contains("actDepRwy"))
                                {
                                    actDepRwy["all"] = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("actDepRwy").value("all", "");
                                    actDepRwy["any"] = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("actDepRwy").value("any", "");
                                }

                                std::string wtc = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("wtc", "");
                                std::string engineType = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("engineType", "");
                                std::map<std::string, bool> acftType =
                                    this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("acftType", std::map<std::string, bool>{});
                                int engineCount = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("engineCount", 0);
                                int mtow = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("mtow", 0);
                                std::map<std::string, bool> dest =
                                    this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("dest", std::map<std::string, bool>{});
                                std::map<std::string, std::map<std::string, std::vector<std::string>>> route = {};

                                if (this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).contains("route"))
                                {
                                    if (this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").contains("allow"))
                                    {
                                        for (const auto& id : this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").at("allow").items())
                                        {
                                            std::string routeId = id.key();
                                            std::vector<std::string> configRoute =
                                                vsid::utils::split(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").at("allow").value(routeId, ""), ',');

                                            if(!configRoute.empty()) route["allow"].insert({ routeId, configRoute });
                                        }
                                    }
                                   
									if (this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").contains("deny"))
									{
										for (const auto& id : this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").at("deny").items())
										{
											std::string routeId = id.key();
											std::vector<std::string> configRoute =
												vsid::utils::split(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).at("route").at("deny").value(routeId, ""), ',');

                                            if(!configRoute.empty()) route["deny"].insert({ routeId, configRoute });
										}
									}
                                    // #DEV
                                    messageHandler->writeMessage("DEBUG", "[" + wpt + "?" + desig + "] route found.", vsid::MessageHandler::DebugArea::Dev);

                                    if (route.contains("allow"))
                                    {
										for (auto const& [id, allowRoute] : route["allow"])
										{
											messageHandler->writeMessage("DEBUG", "[" + wpt + "?" + desig + "] allow (ID: " +
												id + "): " + vsid::utils::join(allowRoute, ' '), vsid::MessageHandler::DebugArea::Dev);
										}
                                    }

                                    if (route.contains("deny"))
                                    {
										for (auto const& [id, denyRoute] : route["deny"])
										{
											messageHandler->writeMessage("DEBUG", "[" + wpt + "?" + desig + "] deny (ID: " +
												id + "): " + vsid::utils::join(denyRoute, ' '), vsid::MessageHandler::DebugArea::Dev);
										}
                                    }
                                    // END DEV
                                }

                                std::string customRule = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("customRule", "");
                                customRule = vsid::utils::toupper(customRule);
                                std::string area = vsid::utils::toupper(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("area", ""));
                                int lvp = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("lvp", -1);
                                int timeFrom = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("timeFrom", -1);
                                int timeTo = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("timeTo", -1);
                                
                                vsid::Sid newSid = { base, wpt, id, "", desig, rwys, equip, initial, via, prio,
                                                    pilotfiled, actArrRwy, actDepRwy, wtc, engineType, acftType, engineCount,
                                                    mtow, dest, route, customRule, area, lvp,
                                                    timeFrom, timeTo };
                                apt.second.sids.push_back(newSid);
                                if (newSid.timeFrom != -1 && newSid.timeTo != -1) apt.second.timeSids.push_back(newSid);
                            }
                        }
                    }
                }
                catch (const json::parse_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Parse] Failed to load airport config (" + apt.first + "): " + std::string(e.what()));
                }
                catch (const json::type_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Type] Failed to load airport config (" + apt.first + "): " + std::string(e.what()));
                }
                catch (const json::out_of_range& e)
                {
                    messageHandler->writeMessage("ERROR", "[Range] Failed to load airport config (" + apt.first + "): " + std::string(e.what()));
                }
                catch (const json::other_error& e)
                {
                    messageHandler->writeMessage("ERROR", "[Other] Failed to load airport config (" + apt.first + "): " + std::string(e.what()));
                }
                catch (const std::exception &e)
                {
                    messageHandler->writeMessage("ERROR", "Failure in config (" + apt.first + "): " + std::string(e.what()));
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

COLORREF vsid::ConfigParser::getColor(std::string color)
{
    if (this->colors.contains(color))
    {
        return this->colors[color];
    }
    else
    {
        messageHandler->writeMessage("ERROR", "Failed to retrieve color: \"" + color + "\"");
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
