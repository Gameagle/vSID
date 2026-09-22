#include "pch.h"

#include <format>
#include <utility>

#include "syncManager.h"
#include "logger.h"
#include "utils.h"
#include "constants.h"
#include "fplnManager.h"
#include "airportManager.h"

using FplnManager = vsid::fpln::FplnManager;
using AirportManager = vsid::apt::AirportManager;

void vsid::sync::SyncManager::add(const std::string& callsign, const std::string& newScratch, const std::string& oldScratch)
{
	auto& qcs = queue_[callsign];

	std::string trimmedNew = vsid::utils::trim(newScratch);
	std::string trimmedOld = vsid::utils::trim(oldScratch);

	if (!qcs.empty() && vsid::utils::svEqualCi(qcs.back().newScratch, newScratch)) return;

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] adding New: [{}] Old: [{}] to sync queue.",
		callsign, trimmedNew, trimmedOld), DebugLevel::Sync);

	qcs.push_back({ std::move(trimmedNew), std::move(trimmedOld)});

	if (!states_.contains(callsign))
		states_[callsign] = SyncData();
}

void vsid::sync::SyncManager::processQueue(EuroScopePlugIn::CPlugIn* plugin)
{
	static auto lastSyncRun = std::chrono::steady_clock::time_point{};
	auto now = std::chrono::steady_clock::now();

	if (now - lastSyncRun < std::chrono::milliseconds(SYNC_SLEEP_MS)) return;
	lastSyncRun = now;

	if (queue_.empty()) return;

	vsid::Logger::log(LogLevel::Debug, std::format("Started sync processing queue... Size [{}]", queue_.size()), DebugLevel::Dev);

	for (auto it = queue_.begin(); it != queue_.end();)
	{
		const std::string& callsign = it->first;
		auto& csQueue = it->second;
		auto& data = states_[callsign];

		if (csQueue.empty() && data.state == SyncState::Free)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Removing from sync queue. "
				"No more msgs and state is ::Free", callsign), DebugLevel::Sync);

			states_.erase(callsign);
			it = queue_.erase(it);
			continue;
		}

		EuroScopePlugIn::CFlightPlan FlightPlan = plugin->FlightPlanSelect(callsign.c_str());

		if (!FlightPlan.IsValid())
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Flightplan not valid. Removing from sync queue.", callsign), DebugLevel::Sync);

			states_.erase(callsign);
			it = queue_.erase(it);
			continue;
		}

		if (data.state == SyncState::Free && !csQueue.empty())
		{
			std::string& newScratch = csQueue.front().newScratch;

			data.state = SyncState::WaitOnSync;
			data.lastTriggerTime = now;
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] syncing [{}]", callsign, newScratch), DebugLevel::Sync, true);

			FlightPlan.GetControllerAssignedData().SetScratchPadString(newScratch.c_str()); // set scratch pad after state update
		}
		else if (data.state != SyncState::Free)
		{
			auto elapsed = std::chrono::duration<double>(now - data.lastTriggerTime).count();
			if (elapsed >= SYNC_TIMEOUT_SECONDS)
			{
				data.state = SyncState::Free;
				SyncMsg front;

				if (!csQueue.empty())
				{
					front = std::move(csQueue.front());
					csQueue.pop_front();
				}

				FlightPlan.GetControllerAssignedData().SetScratchPadString(front.oldScratch.c_str()); // set scratch pad after state update

				vsid::Logger::log(LogLevel::Warning, std::format("[{}] exceeded watch dog time [{} seconds]. "
					"Removing [{}]. Restoring [{}] to scratch pad",
					callsign, elapsed, front.newScratch, front.oldScratch), DebugLevel::Sync);
			}
		}

		++it;
	}

	vsid::Logger::log(LogLevel::Debug, "Finished sync processing queue...", DebugLevel::Dev);
}

void vsid::sync::SyncManager::update(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& scratchOverwrite)
{
	std::string callsign = FlightPlan.GetCallsign();

	auto qIt = queue_.find(callsign);
	auto stateIt = states_.find(callsign);

	if (stateIt == states_.end() || qIt == queue_.end() || qIt->second.empty())
		return;

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] Updating sync queue.", callsign), DebugLevel::Sync);

	auto& data = stateIt->second;
	auto& msg = qIt->second.front();

	EuroScopePlugIn::CFlightPlanControllerAssignedData fplnData = FlightPlan.GetControllerAssignedData();
	std::string currScratch = vsid::utils::trim(fplnData.GetScratchPadString());
	bool synced = false;

	if (data.state == SyncState::WaitOnSync)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] WaitOnSync. Current [{}] | Expected [{}]{}",
			callsign, currScratch, msg.newScratch,
			(scratchOverwrite.empty()) ? "" : std::format(" | Overwrite [{}]", scratchOverwrite)),
			DebugLevel::Sync);

		if (scratchOverwrite == "GND" && gndStates_.contains(msg.newScratch))
		{
			synced = true;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] overwritten 'GND'. sync set true", callsign), DebugLevel::Sync);
		}
			
		else if (vsid::utils::svEqualCi(currScratch, msg.newScratch))
		{
			synced = true;

			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Current [{}] equals [{}]. sync set true",
				callsign, currScratch, msg.newScratch), DebugLevel::Sync);
		}
		else if (vsid::utils::svEqualCi(scratchOverwrite, msg.newScratch))
		{
			synced = true;
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] overwritten [{}] equals [{}]. sync set true",
				callsign, scratchOverwrite, msg.newScratch), DebugLevel::Sync);
		}

		if (synced)
		{
			if (vsid::utils::svEqualCi(currScratch, msg.oldScratch))
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Current [{}] already matches old [{}]. Skipping ::WaitOnRestore.",
					callsign, currScratch, msg.oldScratch), DebugLevel::Sync);

				qIt->second.pop_front();

				data.state = SyncState::Free;
			}
			else
			{
				vsid::Logger::log(LogLevel::Debug, std::format("[{}] Setting ::WaitOnRestore", callsign), DebugLevel::Sync);

				data.state = SyncState::WaitOnRestore;
				data.lastTriggerTime = std::chrono::steady_clock::now();

				fplnData.SetScratchPadString(msg.oldScratch.c_str()); // set scratch pad after state update
			}			
		}
	}
	else if (data.state == SyncState::WaitOnRestore)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] WaitOnRestore. Current [{}] | Expected [{}]",
			callsign, currScratch, msg.oldScratch), DebugLevel::Sync);

		if (vsid::utils::svEqualCi(currScratch, msg.oldScratch))
		{
			qIt->second.pop_front();

			data.state = SyncState::Free;			
		}
	}
}

void vsid::sync::SyncManager::syncReq(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	std::string callsign = FlightPlan.GetCallsign();
	std::string adep = FlightPlan.GetFlightPlanData().GetOrigin();
	auto& processed = FplnManager::getProcessed();

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] calling request sync.", callsign), DebugLevel::Req);

	const auto activeApt = AirportManager::getData(adep);

	if (activeApt == nullptr) return;

	if (auto it = processed.find(callsign); it != processed.end())
	{
		auto& fpln = it->second;

		if (!fpln.request.empty())
		{
			// sync rwy requests - parallel req lists are already managed on scratchpad updates

			if (activeApt->rwyrequests.contains(fpln.request))
			{
				bool stop = false;

				for (auto& [rwy, reqRwy] : activeApt->rwyrequests.at(fpln.request))
				{
					for (auto& [reqCallsign, reqTime] : reqRwy)
					{
						if (reqCallsign != callsign) continue;

						std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(reqTime);

						SyncManager::add(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());

						stop = true;
						break;
					}
					if (stop) break;
				}
			}
			// sync normal requests

			else if (activeApt->requests.contains(fpln.request))
			{
				for (auto& [reqCallsign, reqTime] : activeApt->requests.at(fpln.request))
				{
					if (reqCallsign != callsign) continue;

					std::string newScratch = ".VSID_REQ_" + fpln.request + "/" + std::to_string(reqTime);

					SyncManager::add(callsign, newScratch, FlightPlan.GetControllerAssignedData().GetScratchPadString());

					break;
				}
			}
		}
	}

	
}

void vsid::sync::SyncManager::syncStates(EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	if (!FlightPlan.IsValid()) return;

	std::string callsign = FlightPlan.GetCallsign();
	auto& processed = FplnManager::getProcessed();

	if (auto it = processed.find(callsign); it != processed.end())
	{
		if (FlightPlan.GetClearenceFlag())
		{
			SyncManager::add(callsign, "CLEA", FlightPlan.GetControllerAssignedData().GetScratchPadString());
		}

		if (!it->second.gndState.empty())
		{
			SyncManager::add(callsign, it->second.gndState, FlightPlan.GetControllerAssignedData().GetScratchPadString());
		}
	}
}