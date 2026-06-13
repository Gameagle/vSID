#include "pch.h"

#include "logger.h"
#include "timeHandler.h"

#include <format>
#include <iterator>

void vsid::Logger::initialize()
{
	if (running) return;

	std::error_code ec;

	// get dll path
	char pathBuf[MAX_PATH];
	GetModuleFileNameA((HINSTANCE)&__ImageBase, pathBuf, MAX_PATH);
	std::filesystem::path dllPath = pathBuf;
	logFilePath = dllPath.parent_path() / "logs" / "vsid.log";	

	std::filesystem::create_directory(logFilePath.parent_path(), ec);

	if (!ec)
	{
		fileLockMutex = CreateMutexA(NULL, FALSE, "Local\\vsid_log_mutex");

		if (fileLockMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CloseHandle(fileLockMutex);
			fileLockMutex = NULL;
		}
		else if (fileLockMutex != NULL)
		{
			rotateLogs(); // rotate logs on startup to archive previous session logs
			logFile.open(logFilePath, std::ios::app);
		}
		
	}

	// create thread
	running = true;
	worker = std::thread(workerThread);

	if (!logFile.is_open())
	{
		log(LogLevel::Error, "File logging disabled: Insufficient permissions to create or access log directory.");
	}
	else
		log(LogLevel::Info, "Logger initialized successfully (File logging active).");
}

void vsid::Logger::shutdown() {
	if (!running) return;

	// signal thread stopping
	running = false;
	cv.notify_all();

	// wait on closing thread
	if (worker.joinable()) worker.join();

	if (logFile.is_open())
	{
		logFile.flush();
		logFile.close();
	}

	if(fileLockMutex)
	{
		CloseHandle(fileLockMutex);
		fileLockMutex = NULL;
	}

	if (newConsole)
	{
		if (consoleOut)
		{
			fclose(consoleOut);
			consoleOut = nullptr;
		}

		FreeConsole();
		newConsole = false;
	}	
}

void vsid::Logger::enableConsole()
{
	if (newConsole) return;

	std::lock_guard lock(consoleMutex);

	if (AllocConsole())
	{
		newConsole = true;

		// disable close button to prevent ES closing
		HWND hwnd = GetConsoleWindow();
		if (hwnd != NULL)
		{
			HMENU hMenu = GetSystemMenu(hwnd, FALSE);
			if (hMenu != NULL)
			{
				DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
			}
			SetWindowTextA(hwnd, "Console Logger");
		}

		freopen_s(&consoleOut, "CONOUT$", "w", stdout);

		log(LogLevel::Info, "Console output enabled.");
	}
}

void vsid::Logger::disableConsole()
{
	std::lock_guard lock(consoleMutex);

	if (!newConsole) return;

	log(LogLevel::Info, "Console output disabled.");

	if (consoleOut)
	{
		fclose(consoleOut);
		consoleOut = nullptr;
	}

	FreeConsole();
	newConsole = false;
}

std::vector<std::string> vsid::Logger::fetchEsMsgs()
{
	std::lock_guard lock(esMutex);
	std::vector<std::string> msgs;

	while (!esQueue.empty())
	{
		msgs.push_back(std::move(esQueue.front()));
		esQueue.pop();
	}
	return msgs;
}

void vsid::Logger::log(LogLevel level, const std::string_view& msg, std::optional<DebugLevel> debugLevel, bool devOnly)
{
	if (!running) return;
	if (devOnly && !logDevOnly) return;

	{
		std::lock_guard lock(bgMutex);
		bgQueue.push(LogMessage{ level, std::string(msg), debugLevel });
	}
	cv.notify_all();
}

void vsid::Logger::workerThread()
{
	while (running || !bgQueue.empty())
	{
		LogMessage msg;

		// wait if queue is not empty or if worker is not running
		{
			std::unique_lock lock(bgMutex);
			cv.wait(lock, [] { return !bgQueue.empty() || !running; });

			if (!running && bgQueue.empty()) break;

			msg = std::move(bgQueue.front());
			bgQueue.pop();
		}

		// write log file

		if (logFile.is_open())
		{
			// format file log message 
			std::string fout = std::format("[{:%Y-%m-%d %H:%M:%S}] [{}]",
				vsid::time::getUtcNow(), logLvlToString(msg.level));

			if (msg.level == LogLevel::Debug && msg.debugLevel.has_value())
				std::format_to(std::back_inserter(fout), " [{}]", debugLvlToString(msg.debugLevel.value()));

			std::format_to(std::back_inserter(fout), " {}\n", msg.msg);

			logFile << fout;

			if (logFile.fail()) // revive stream if log file was opened on the disk
				logFile.clear();

			if(msg.level >= LogLevel::Warning)
				logFile.flush();

			if (logFile.tellp() >= 5 * 1024 * 1024) rotateLogs(); // rotate if log file exceeds 5 MB
		}

		// console output
		if (msg.level == LogLevel::Debug && msg.debugLevel.has_value()) // skip console output if debug level is not active
		{
			if (!isDebugLevelActive(msg.debugLevel.value())) continue;
		}

		{
			std::lock_guard lock(consoleMutex);

			if (consoleOut)
			{
				// format console log message
				std::string out = std::format("[{}] [{}]",
					vsid::time::toTimeString(vsid::time::getUtcNow()),
					logLvlToString(msg.level));

				if (msg.level == LogLevel::Debug && msg.debugLevel.has_value())
				{
					out += std::format(" [{}]", debugLvlToString(msg.debugLevel.value()));
				}

				out += std::format(" {}\n", msg.msg);

				fputs(out.c_str(), consoleOut);
				fflush(consoleOut);
			}
		}
		
		// push to ES queue
		if(msg.level > LogLevel::Debug) {
			std::lock_guard lock(esMutex);
			esQueue.push(std::format("[{}] {}", logLvlToString(msg.level), msg.msg));
		}
	}
} 

void vsid::Logger::rotateLogs()
{
	if (logFile.is_open())
	{
		logFile.flush();
		logFile.close();
	}

	std::error_code ec;

	// shift logs by one index backwards to not overwrite files before moving
	for (int i = 3; i >= 1; --i)
	{
		std::filesystem::path src = logFilePath.parent_path() / std::format("{}.{}{}", logFilePath.stem().string(), i, logFilePath.extension().string());
		std::filesystem::path dst = logFilePath.parent_path() / std::format("{}.{}{}", logFilePath.stem().string(), i + 1, logFilePath.extension().string());

		if (std::filesystem::exists(src, ec))
		{
			std::filesystem::remove(dst, ec);
			std::filesystem::rename(src, dst, ec);
		}
	}

	std::filesystem::path firstLog = logFilePath.parent_path() / std::format("{}.1{}", logFilePath.stem().string(), logFilePath.extension().string());
	
	// main log file is rotated to index .1
	if (std::filesystem::exists(logFilePath, ec))
	{
		std::filesystem::remove(firstLog, ec);
		std::filesystem::rename(logFilePath, firstLog, ec);
	}

	logFile.open(logFilePath, std::ios::trunc);
}

void vsid::Logger::panicFlush()
{
	if (!running) return;

	if (logFile.is_open())
		logFile.flush();

	if (bgMutex.try_lock())
	{
		while (!bgQueue.empty())
		{
			LogMessage& msg = bgQueue.front();

			if (logFile.is_open())
			{
				std::string fout = std::format("[{:%Y-%m-%d %H:%M:%S}] [CRASH DUMP] [{}]",
					vsid::time::getUtcNow(), logLvlToString(msg.level));

				if (msg.level == LogLevel::Debug && msg.debugLevel.has_value())
					fout += std::format(" [{}]", debugLvlToString(msg.debugLevel.value()));

				fout += std::format(" {}\n", msg.msg);

				logFile << fout;
			}
			bgQueue.pop();
		}

		if (logFile.is_open()) logFile.flush();

		bgMutex.unlock();
	}
}

void vsid::Logger::toggleDebugLevel(const std::vector<std::string_view>& lvlList)
{
	for (std::string_view svLvl : lvlList)
	{
		DebugLevel lvl = stringToDebugLvl(svLvl);

		if (lvl == DebugLevel::None)
		{
			currentDebugLvl.fill(false);
			continue;
		}

		if (lvl == DebugLevel::All)
		{
			currentDebugLvl.fill(true);
			continue; // activates all debug level but continues to allow disabling of some levels (as further params)
		}

		std::size_t idx = static_cast<std::size_t>(lvl);

		currentDebugLvl[idx] = !currentDebugLvl[idx];
	}
}

std::string vsid::Logger::getDebugLevelString()
{
	std::string result;
	bool first = true;

	for (std::size_t i = 0; i < currentDebugLvl.size(); ++i)
	{
		if (!currentDebugLvl[i]) continue;

		DebugLevel lvl = static_cast<DebugLevel>(i);

		if (lvl != DebugLevel::All && lvl != DebugLevel::None)
		{
			if (!first) result += " | ";
			result += debugLvlToString(lvl);

			first = false;
		}
	}

	return result.empty() ? "NONE" : result;
}