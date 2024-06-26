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
            // default values are blue to signal if the import went wront
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

    try
    {
        this->reqTimes.insert({ "caution", this->vSidConfig.at("requests").value("caution", 2) });
        this->reqTimes.insert({ "warning", this->vSidConfig.at("requests").value("warning", 5) });
    }
    catch (json::parse_error& e)
    {
        messageHandler->writeMessage("ERROR", "Failed to get request timers: " + std::string(e.what()));
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
                    //configTest = json::parse(testFile);
                    this->parsedConfig = json::parse(configFile);

                    if (!this->parsedConfig.contains(apt.first))
                    {
                        continue;
                    }
                    else
                    {
                        aptConfig.insert(apt.first);

                        // general settings

                        apt.second.icao = apt.first;
                        apt.second.elevation = this->parsedConfig.at(apt.first).value("elevation", 0);
                        apt.second.allRwys = vsid::utils::split(this->parsedConfig.at(apt.first).value("runways", ""), ',');
                        apt.second.transAlt = this->parsedConfig.at(apt.first).value("transAlt", 0);
                        apt.second.maxInitialClimb = this->parsedConfig.at(apt.first).value("maxInitialClimb", 0);
                        apt.second.timezone = this->parsedConfig.at(apt.first).value("timezone", "Etc/UTC");
                        apt.second.requests["clearance"] = {};
                        apt.second.requests["startup"] = {};
                        apt.second.requests["pushback"] = {};
                        apt.second.requests["taxi"] = {};
                        apt.second.requests["departure"] = {};

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
                            std::string wpt = sid.key();

                            for (auto& sidWpt : this->parsedConfig.at(apt.first).at("sids").at(sid.key()).items())
                            {
                                std::string desig = sidWpt.key();
                                std::string rwys = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("rwy", "");
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
                                std::map<std::string, bool> acftType = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("acftType", std::map<std::string, bool>{});
                                int engineCount = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("engineCount", 0);
                                int mtow = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("mtow", 0);
                                std::string customRule = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("customRule", "");
                                customRule = vsid::utils::toupper(customRule);
                                std::string area = vsid::utils::toupper(this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("area", ""));
                                std::string equip = "";
                                int lvp = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("lvp", -1);
                                int timeFrom = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("timeFrom", -1);
                                int timeTo = this->parsedConfig.at(apt.first).at("sids").at(sid.key()).at(sidWpt.key()).value("timeTo", -1);
                                
                                vsid::Sid newSid = { wpt, ' ', desig, rwys, initial, via, prio,
                                                    pilotfiled, actArrRwy, actDepRwy, wtc, engineType, acftType, engineCount,
                                                    mtow, customRule, area, equip, lvp,
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
    /*savedCustomRules.clear(); // MONITOR - DROPPED ANYWAYS
    savedAreas.clear();
    savedSettings.clear();*/

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
                messageHandler->writeMessage("ERROR:", "Failed to load grp config : " + std::string(e.what()));
            }
            catch (const json::type_error& e)
            {
                messageHandler->writeMessage("ERROR:", "Failed to load grp config : " + std::string(e.what()));
            }
        }
    }
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
        // return if color could not be found is purple to signal error
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
