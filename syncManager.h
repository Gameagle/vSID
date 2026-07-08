#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <chrono>

#include "include/es/EuroScopePlugIn.h"

namespace vsid::sync {

	struct SyncMsg {
		std::string newScratch;
		std::string oldScratch;
	};

	enum class SyncState {
		Free,
		WaitOnSync,
		WaitOnRestore
	};

	struct SyncData {
		SyncState state = SyncState::Free;
		std::chrono::steady_clock::time_point lastTriggerTime; // #evaluate - which clock to choose, utc clock?
	};

	class SyncManager
	{
	public:
		SyncManager() = default;
		~SyncManager() = default;

		//************************************
		// Description: Adds a callsign with two messages to the sync queue
		// Method:    add
		// FullName:  vsid::sync::SyncManager::add
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: const std::string & newScratch - the new msgs to be synced
		// Parameter: const std::string & oldScratch - the old msgs to be restored after syncing
		//************************************
		void add(const std::string& callsign, const std::string& newScratch, const std::string& oldScratch);

		//************************************
		// Description: Iterates through the queue to remove empty msgs and start syncing for new msgs
		// Method:    processQueue
		// FullName:  vsid::sync::SyncManager::processQueue
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CPlugIn * plugin
		//************************************
		void processQueue(EuroScopePlugIn::CPlugIn* plugin);

		//************************************
		// Description: Restores old sratch pad values or frees SyncState
		// Method:    update
		// FullName:  vsid::sync::SyncManager::update
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EuroScopePlugIn::CFlightPlan & FlightPlan
		// Parameter: const std::string & scratchOverwrite
		//************************************
		void update(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& scratchOverwrite = "");

		//************************************
		// Description: Clears queue and sync state list
		// Method:    clear
		// FullName:  vsid::sync::SyncManager::clear
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		inline void clear()
		{
			this->queue.clear();
			this->states.clear();
		}

	private:
		std::unordered_map<std::string, std::deque<SyncMsg>> queue; // queu with callsign and sync msgs
		std::unordered_map<std::string, SyncData> states; // sync states for callsigns
		std::unordered_set<std::string> gndStates = {"PUSH", "TAXI", "DEPA"}; // default ES gnd states to overwrite
	};
}