#pragma once

#include "logger.h"
#include "sid.h"

#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <chrono>
#include <optional>
#include <concepts>
#include <utility>
#include <functional>

#include <source_location>

namespace vsid {
	namespace fpln {

		//************************************
		// Description: Strip the filed route from SID/RWY and/or SID to have a bare route to populate with set SID.
		// Any SIDs or RWYs will be deleted and have to be reset.
		// Method:    clean
		// FullName:  vsid::fplnhelper::clean
		// Access:    public 
		// Returns:   std::vector<std::string>
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: std::string filedSidWpt
		//************************************
		std::vector<std::string> clean(const EuroScopePlugIn::CFlightPlan& FlightPlan, std::string filedSidWpt = "");

		//************************************
		// Description: Retrieves the "atc block" from a route (SID/RWY or ICAO/RWY)
		// Method:    getAtcBlock
		// FullName:  vsid::fplnhelper::getAtcBlock
		// Access:    public 
		// Returns:   std::pair<std::string, std::string> - first: SID or ICAO, second: RWY
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		std::pair<std::string, std::string> getAtcBlock(const EuroScopePlugIn::CFlightPlan& FlightPlan);

		//************************************
		// Description: Compares available transitions with the route and returns a matching transition
		// Method:    getTransition
		// FullName:  vsid::fpln::getTransition
		// Access:    public 
		// Returns:   std::string - found transition or empty string if not
		// Qualifier:
		// Parameter: const std::vector<std::string> & route - filed route split in single elements
		// Parameter: const std::map<std::string, vsid::Transition> & - map of all transitions for a given SID
		//************************************		
		std::string getTransition(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::map<std::string, vsid::Transition>& transition,
			const std::string& filedSidWpt);

		//************************************
		// Description: Splits the SID at 'X' for use with SIDxTRANS
		// Method:    splitTransition
		// FullName:  vsid::fpln::splitTransition
		// Access:    public 
		// Returns:   std::pair<std::string, std::string - first = SID, second = Transition
		//	 second might be empty if no transition present
		// Qualifier:
		// Parameter: std::string atcSid
		//************************************
		std::pair<std::string, std::string> splitTransition(std::string atcSid); // #refactor - std::optional as return, string_view as param

		//************************************
		// Description: Splits the SID at 'X' for use with SIDxTRANS as string_view
		// Method:    splitTransitionSV
		// FullName:  vsid::fplnhelper::splitTransitionSV
		// Access:    public 
		// Returns:   std::pair<std::string_view, std::string_view>
		// Qualifier:
		// Parameter: std::string_view atcSid
		//************************************
		std::pair<std::string_view, std::string_view> splitTransitionSV(std::string_view atcSid);

		//************************************
		// Description: Checks flight plan remarks for a given string
		// Method:    findRemarks
		// FullName:  vsid::fplnhelper::findRemarks
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& searchStr
		//************************************
		bool findRemarks(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&searchStr));

		//************************************
		// Description: Removes a given remark from the flight plan
		// Method:    removeRemark
		// FullName:  vsid::fplnhelper::removeRemark
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& toRemove
		//************************************
		bool removeRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toRemove));

		//************************************
		// Description: Adds the given remark to the flight plan
		// Method:    addRemark
		// FullName:  vsid::fplnhelper::addRemark
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string& toAdd
		//************************************
		bool addRemark(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string(&toAdd));

		//************************************
		// Description: Search for a given entry in the flight plan scratchpad
		// Method:    findScratchPad
		// FullName:  vsid::fplnhelper::findScratchPad
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string & toSearch
		//************************************
		bool findScratchPad(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& toSearch);

		//************************************
		// Description: Get the filed equipment code for the flight plan. If the acft type is found in the
		// RNAV list a RNAV capable code is returned. If equipment is missing but capabilities are present
		// they are converted from FAA to ICAO, defaulting to RNAV capable code if the entry isn't found in the
		// internal list.
		// Method:    getEquip
		// FullName:  vsid::fplnhelper::getEquip
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::set<std::string> & rnav
		//************************************
		std::string getEquip(const EuroScopePlugIn::CFlightPlan& FlightPlan, const std::set<std::string>& rnav = {});

		//************************************
		// Description: Returns flight plan values for the PBN/ field if present
		// Method:    getPbn
		// FullName:  vsid::fplnhelper::getPbn
		// Access:    public 
		// Returns:   std::string - PBN/ field entry or empty string
		// Qualifier:
		// Parameter: const EuroScopePlugIn::CFlightPlan & FlightPlan
		//************************************
		std::string getPbn(const EuroScopePlugIn::CFlightPlan& FlightPlan);

		//************************************
		// Description: Returns the SID waypoint found in the route or empty if none was found. Checks ES determined SID first.
		// Considers transition waypoints
		// Method:    findSidWpt
		// FullName:  vsid::VSIDPlugin::findSidWpt
		// Access:    public 
		// Returns:   std::string
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
		//************************************
		std::string findSidWpt(EuroScopePlugIn::CFlightPlan& FlightPlan);

		struct FplnData
		{
			bool atcRWY = false;
			bool noFplnUpdate = false;
			bool remarkChecked = false;
			vsid::Sid sid = {};
			vsid::Sid customSid = {};
			std::string sidWpt = "";
			std::string transition = "";
			bool sidProcessed = false; // #continue - check where callsign was removed from processed and replace with this flag to avoid reprocessing SID
			std::pair<std::string, bool> intsec = {};
			std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> lastUpdate;
			// std::optional<std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> removalTime;
			std::optional<std::chrono::system_clock::time_point> removalTime;
			int updateCounter = 0;
			std::string request = "";
			long long reqTime = -1;
			bool validEquip = true;
			std::string gndState = "";
			bool ctl = false;
			bool ctlLocal = false;
			bool mapp = false;
			// altitude tracking during acft landing phase
			int ldgAlt = 0;
			bool hov = false;
		};

		class FplnManager {
		public:
			FplnManager() { vsid::Logger::log(LogLevel::Debug, "FplnManager initialized.", vsid::DebugLevel::Fpln); };
			~FplnManager() { vsid::Logger::log(LogLevel::Debug, "FplnManager destroyed.", vsid::DebugLevel::Fpln); };

			//************************************
			// Description: Restores initial climb if this info was lost (e.g. during a reconnect of a pilot)
			// Method:    restoreIC
			// FullName:  vsid::fplnhelper::restoreIC
			// Access:    public 
			// Returns:   bool
			// Qualifier:
			// Parameter: const vsid::Fpln & fplnInfo
			// Parameter: EuroScopePlugIn::CFlightPlan FlightPlan
			// Parameter: EuroScopePlugIn::CController atcMyself
			//************************************
			static bool restoreIC(const std::string& callsign);

			//************************************
			// Description: Moves flight plan info to processed for a given callsign. Can overwrite existing info.
			// Method:    addFpln
			// FullName:  vsid::fpln::fplnManager::addFpln
			// Access:    public 
			// Returns:   void
			// Qualifier:
			// Parameter: const std::string callsign
			// Parameter: FplnData fplnInfo
			//************************************
			inline static void add(
				const std::string& callsign,
				const std::source_location& loc = std::source_location::current()
			) // #refactor to string_view - check why hashmap is not working
			{
				/*std::string state = processed_.contains(callsign) ? "overwriting" : "adding";

				vsid::Logger::log(LogLevel::Debug, std::format("[{}] {} flight plan info to processed.", callsign, state), vsid::DebugLevel::Fpln);

				processed_[callsign] = std::move(fplnInfo);*/

				vsid::Logger::log(
					LogLevel::Debug,
					std::format("[{}] was added from func [{}], line [{}]", callsign, loc.function_name(), loc.line()),
					vsid::DebugLevel::Fpln
					);

				processed_.try_emplace(callsign);
			}

			//************************************
			// Description: Clears the entire flight plan storage
			// Method:    clear
			// FullName:  vsid::fpln::FplnManager::clear
			// Access:    public static 
			// Returns:   void
			// Qualifier:
			//************************************
			inline static void clear() { processed_.clear(); }

			//************************************
			// Description: Removes flight plan info from processed
			// Method:    removeFpln
			// FullName:  vsid::fpln::fplnManager::removeFpln
			// Access:    public 
			// Returns:   void
			// Qualifier:
			// Parameter: const std::string_view callsign
			//************************************
			inline static void remove(const std::string_view callsign)
			{ 
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] removing flight plan info from processed.", callsign), vsid::DebugLevel::Fpln);

				processed_.erase(std::string(callsign)); 
			};

			//************************************
			// Description: Removes (technically) invalid flight plans if they exist in storage
			// Method:    removeInvalid
			// FullName:  vsid::fpln::FplnManager::removeInvalid
			// Access:    public static 
			// Returns:   void
			// Qualifier:
			// Parameter: const std::string_view callsign
			//************************************
			inline static void removeInvalid(const std::string_view callsign)
			{
				if (!processed_.contains(callsign)) return;

				vsid::Logger::log(
					LogLevel::Debug,
					std::format("[{}] was reported (technically) invalid. Removed from processed.", callsign),
					vsid::DebugLevel::Fpln
				);
			};

			//************************************
			// Description: Clears SID data for a given callsign
			// Method:    clearSidData
			// FullName:  vsid::fpln::FplnManager::clearSidData
			// Access:    public static 
			// Returns:   void
			// Qualifier:
			// Parameter: const std::string_view callsign
			//************************************
			static void clearSidData(const std::string_view callsign);

			//************************************
			// Description: Updates (multiple) flight plan info for a given callsign if present
			// Method:    update
			// FullName:  vsid::fpln::FplnManager::update
			// Access:    public static 
			// Returns:   bool
			// Qualifier:
			// Parameter: std::string_view callsign
			// Parameter: Func & & func
			//************************************
			template<typename Func>
			requires std::invocable<Func&&, FplnData&>
			[[nodiscard]]
			static bool update(std::string_view callsign, Func&& func)
			{
				auto it = processed_.find(callsign);

				if (it == processed_.end())
				{
					vsid::Logger::log(vsid::LogLevel::Debug, std::format("[{}] could not update flight plan. Info not found in processed.", callsign),
						vsid::DebugLevel::Fpln);

					return false;
				}

				FplnData& data = it->second;

				std::invoke(std::forward<Func>(func), data);

				return true;

				// Usage
				/*
				fplnManager::update(callsign, [](FplnData& data) {
					data.remarkChecked = true;
					data.lastUpdate = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
					data.XYZ ...
				});
				*/
			}

			//************************************
			// Description: Sets a single flight plan info member for a given callsign if present
			// Method:    update
			// FullName:  vsid::fpln::FplnManager::update
			// Access:    public static 
			// Returns:   bool
			// Qualifier:
			// Parameter: std::string_view callsign
			// Parameter: Member FplnData:: * member
			// Parameter: Value & & value
			//************************************
			template<typename Member, typename Value>
				requires std::is_assignable_v<Member&, Value&&>
			[[nodiscard]]
			static bool update(std::string_view callsign, Member FplnData::* member, Value&& value)
			{
				return update(callsign, [member, &value](FplnData& data)
					{
						data.*member = std::forward<Value>(value);
					});

				// Usage
				/*
				FplnManager::update(icao, &FplnData::remarkChecked, true);
				FplnManager::update(icao, &FplnData::sidWpt, std::move(newSidWpt));
				*/
			}

			//************************************
			// Description: Returns flight plan info for a given callsign if present in processed
			// Method:    getFpln
			// FullName:  vsid::fpln::FplnManager::getFpln
			// Access:    public 
			// Returns:   std::optional<vsid::fpln::FplnData&>
			// Qualifier:
			// Parameter: const std::string_view callsign
			//************************************
			[[nodiscard]]
			inline static const FplnData* getData(std::string_view callsign)
			{
				if (auto it = processed_.find(callsign); it != processed_.end())
					return &it->second;

				vsid::Logger::log(
					vsid::LogLevel::Debug,
					std::format("[{}] flight plan info not found in processed.", callsign),
					vsid::DebugLevel::Fpln
				);

				return nullptr;
			}

			inline static bool contains(std::string_view callsign)
			{
				return processed_.contains(callsign);
			}

			//************************************
			// Description: Returns the processed flight plan info map
			// Method:    getProcessed
			// FullName:  vsid::fpln::fplnManager::getProcessed
			// Access:    public 
			// Returns:   std::unordered_map<std::string, vsid::fpln::FplnData, vsid::utils::StringHash, std::equal_to<>>&
			// Qualifier:
			//************************************
			inline static const std::unordered_map<std::string, FplnData, vsid::utils::StringHash, std::equal_to<>>& getProcessed() { return processed_; };

			//************************************
			// Description Tries to set a clean route without SID. SID will then be placed as first item
			// Processed flight plans are stored.
			// Method:    processFlightplan
			// FullName:  vsid::VSIDPlugin::processFlightplan
			// Access:    public 
			// Returns:   void
			// Qualifier:
			// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
			// Parameter: bool checkOnly
			// Parameter: std::string atcRwy
			// Parameter: vsid::Sid manualSid
			//************************************
			void static processFlightplan(EuroScopePlugIn::CFlightPlan& FlightPlan, bool checkOnly, std::string atcRwy = "", vsid::Sid manualSid = {});

			//************************************
			// Description: Reprocesses a flight plan for a given callsign if present
			// Method:    reprocess
			// FullName:  vsid::fpln::FplnManager::reprocess
			// Access:    public static 
			// Returns:   void
			// Qualifier:
			// Parameter: std::string_view callsign
			//************************************
			void static reprocess(std::string_view callsign);

			//************************************
			// Description: Reprocesses all flight plans in processed
			// Method:    reprocessAll
			// FullName:  vsid::fpln::FplnManager::reprocessAll
			// Access:    public static 
			// Returns:   void
			// Qualifier:
			//************************************
			void static reprocessAll();

		private:
			//************************************
			// Description: Shared reprocess logic for a callsign whose FplnData has already been looked up
			// used by reprocess and reprocessAll
			// Method:    reprocessImpl
			// FullName:  vsid::fpln::FplnManager::reprocessImpl
			// Access:    private static
			// Returns:   void
			// Qualifier:
			// Parameter: std::string_view callsign
			// Parameter: const FplnData & fplnData
			//************************************
			void static reprocessImpl(std::string_view callsign, const FplnData& fplnData);

			inline static std::unordered_map<std::string, FplnData, vsid::utils::StringHash, std::equal_to<>> processed_;
		};
	}
}
