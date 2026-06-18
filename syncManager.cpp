#include "pch.h"

#include <format>
#include <utility>

#include "syncManager.h"
#include "logger.h"
#include "utils.h"
#include "constants.h"

void vsid::sync::SyncManager::add(const std::string& callsign, const std::string& newScratch, const std::string& oldScratch)
{
	auto& qcs = this->queue[callsign];

	std::string trimmedNew = vsid::utils::trim(newScratch);
	std::string trimmedOld = vsid::utils::trim(oldScratch);

	if (!qcs.empty() && vsid::utils::svEqualCi(qcs.back().newScratch, newScratch)) return;

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] adding [{}/{}] to sync queue.",
		callsign, trimmedNew, trimmedOld), DebugLevel::Sync);

	qcs.push_back({ std::move(trimmedNew), std::move(trimmedOld)});

	if (!this->states.contains(callsign))
		this->states[callsign] = SyncData();
}

void vsid::sync::SyncManager::processQueue(EuroScopePlugIn::CPlugIn* plugin)
{
	if (this->queue.empty()) return;

	auto now = std::chrono::steady_clock::now();

	vsid::Logger::log(LogLevel::Debug, "Started sync processing queue...", DebugLevel::Dev);

	for (auto it = this->queue.begin(); it != this->queue.end();)
	{
		const std::string& callsign = it->first;
		auto& csQueue = it->second;
		auto& data = this->states[callsign];

		if (csQueue.empty() && data.state == SyncState::Free)
		{
			vsid::Logger::log(LogLevel::Debug, std::format("[{}] Removing from sync queue. "
				"No more msgs and state is ::Free", callsign), DebugLevel::Sync);

			this->states.erase(callsign);
			it = this->queue.erase(it);
			continue;
		}

		EuroScopePlugIn::CFlightPlan FlightPlan = plugin->FlightPlanSelect(callsign.c_str());

		if (!FlightPlan.IsValid())
		{
			++it;
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
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - data.lastTriggerTime).count();
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
					"Removing [{}/{}]. Restoring [{}] to scratch pad",
					callsign, elapsed, front.newScratch, front.oldScratch, front.oldScratch), DebugLevel::Sync);
			}
		}

		++it;
	}

	vsid::Logger::log(LogLevel::Debug, "Finished sync processing queue...", DebugLevel::Dev);
}

void vsid::sync::SyncManager::update(EuroScopePlugIn::CFlightPlan& FlightPlan, const std::string& scratchOverwrite)
{
	std::string callsign = FlightPlan.GetCallsign();

	auto qIt = this->queue.find(callsign);

	if (!this->states.contains(callsign) || qIt == this->queue.end() || qIt->second.empty())
		return;

	vsid::Logger::log(LogLevel::Debug, std::format("[{}] Updating sync queue.", callsign), DebugLevel::Sync);

	auto& data = this->states[callsign];
	auto& msg = qIt->second.front();

	EuroScopePlugIn::CFlightPlanControllerAssignedData fplnData = FlightPlan.GetControllerAssignedData();
	std::string currScratch = vsid::utils::trim(fplnData.GetScratchPadString());
	bool synced = false;

	if (data.state == SyncState::WaitOnSync)
	{
		vsid::Logger::log(LogLevel::Debug, std::format("[{}] WaitOnSync. Current [{}] | Expected [{}] | Overwrite [{}]",
			callsign, currScratch, msg.newScratch, scratchOverwrite), DebugLevel::Sync);

		if (scratchOverwrite == "GND" && this->gndStates.contains(msg.newScratch))
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