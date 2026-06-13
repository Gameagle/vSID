#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <optional>
#include <fstream>
#include <filesystem>
#include <array>
#include <vector>

#include "utils.h"

namespace vsid
{
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Error
	};

	enum class DebugLevel {
		All,
		Dev,
		Menu,
		Atc,
		Sid,
		Fpln,
		Req,
		Conf,
		Rwy,
		Area,
		Ese,
		Gen,
		Func,
		Sync,
		Cmd,
		None
	};

	struct LogMessage {
		LogLevel level;
		std::string msg;
		std::optional<DebugLevel> debugLevel;
	};
	class Logger {
	public:

		//************************************
		// Description: Initializes the logger by allocating a console and starting the worker thread
		// Method:    initialize
		// FullName:  vsid::Logger::initialize
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void initialize();

		//************************************
		// Description: Shuts down the logger by stopping the worker thread and freeing the console
		// Method:    shutdown
		// FullName:  vsid::Logger::shutdown
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void shutdown();
		
		//************************************
		// Description: Logs a message by pushing it into the msg queue to be processed by the worker thread
		// Method:    log
		// FullName:  vsid::Logger::log
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		// Parameter: LogLevel level
		// Parameter: const std::string_view & msg
		// Parameter: std::optional<DebugLevel> debugLevel
		// Parameter: bool devOnly - if true, msg will only be logged if logDevOnly is true (devmode)
		//************************************
		static void log(LogLevel level, const std::string_view& msg, std::optional<DebugLevel> debugLevel = std::nullopt, bool devOnly = false);

		//************************************
		// Description: Returns if the logger is running
		// Method:    isRunning
		// FullName:  vsid::Logger::isRunning
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		//************************************
		inline static bool isRunning() { return running; };

		//************************************
		// Descriptions: Gets all messages that are pending to be sent to ES (i.e. all messages in the esQueue) to use inside ES
		// Method:    fetchEsMsgs
		// FullName:  vsid::Logger::fetchEsMsgs
		// Access:    public static 
		// Returns:   std::vector<std::string>
		// Qualifier:
		//************************************
		static std::vector<std::string> fetchEsMsgs();

		//************************************
		// Description: Enables console
		// Method:    toggleConsole
		// FullName:  vsid::Logger::toggleConsole
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void enableConsole();

		//************************************
		// Description: Disables console
		// Method:    disableConsole
		// FullName:  vsid::Logger::disableConsole
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void disableConsole();

		//************************************
		// Description: Returns the current state of console (open / closed)
		// Method:    getConsoleState
		// FullName:  vsid::Logger::getConsoleState
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		//************************************
		inline static bool getConsoleState() { return newConsole; }

		//************************************
		// Description: Flushes all pending log messages to the console immediately, used in crash situations to save pending log msgs
		// Method:    panicFlush
		// FullName:  vsid::Logger::panicFlush
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void panicFlush();

		//************************************
		// Description: Sets whether only dev-level debug messages should be logged
		// FullName:  vsid::Logger::setLogDevOnly
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		// Parameter: bool devOnly
		//************************************
		inline static void setLogDevOnly(bool devOnly) { logDevOnly = devOnly; }

		//************************************
		// Description: Return wether only dev-level debug messages are logged
		// Method:    getLogDevOnly
		// FullName:  vsid::Logger::getLogDevOnly
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		//************************************
		inline static bool getLogDevOnly() { return logDevOnly; }

		//************************************
		// Description: Toggles all debug levels contained in the list
		// Method:    setDebugLevel
		// FullName:  vsid::Logger::setDebugLevel
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::vector<std::string_view> & lvlList
		//************************************
		static void toggleDebugLevel(const std::vector<std::string_view>& lvlList);

		//************************************
		// Description: Get all current / active debug levels as array and state
		// Method:    getDebugLevel
		// FullName:  vsid::Logger::getDebugLevel
		// Access:    public static 
		// Returns:   std::array<bool, 16>
		// Qualifier:
		//************************************
		inline static std::array<bool, 16> getDebugLevel() { return currentDebugLvl; }

		//************************************
		// Description: Sets up all active debug levels in one string for logging
		// Method:    getDebugLevelString
		// FullName:  vsid::Logger::getDebugLevelString
		// Access:    public static 
		// Returns:   std::string
		// Qualifier:
		//************************************
		static std::string getDebugLevelString();

	private:

		//************************************
		// Description: Worker thread function that continuously processes log messages from the queue and outputs them to the console
		// Method:    workerThread
		// FullName:  vsid::Logger::workerThread
		// Access:    private static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void workerThread();

		//************************************
		// Description: Transform a log level into a readable string
		// Method:    logLvlToString
		// FullName:  vsid::Logger::logLvlToString
		// Access:    private static 
		// Returns:   constexpr std::string_view
		// Qualifier:
		// Parameter: LogLevel lvl
		//************************************
		inline static constexpr std::string_view logLvlToString(LogLevel lvl)
		{
			switch (lvl)
			{
			case LogLevel::Debug: return "DEBUG";
			case LogLevel::Info: return "INFO";
			case LogLevel::Warning: return "WARNING";
			case LogLevel::Error: return "ERROR";
			default: return "UNKNOWN";
			}
		};

		//************************************
		// Description: Transform a debug level into a readable string
		// Method:    debugLvlToString
		// FullName:  vsid::Logger::debugLvlToString
		// Access:    private static 
		// Returns:   constexpr std::string_view
		// Qualifier:
		// Parameter: DebugLevel lvl
		//************************************
		inline static constexpr std::string_view debugLvlToString(DebugLevel lvl)
		{
			switch (lvl)
			{
			case DebugLevel::Dev: return "DEV";
			case DebugLevel::Menu: return "MENU";
			case DebugLevel::Atc: return "ATC";
			case DebugLevel::Sid: return "SID";
			case DebugLevel::Fpln: return "FPLN";
			case DebugLevel::Req: return "REQ";
			case DebugLevel::Conf: return "CONF";
			case DebugLevel::Rwy: return "RWY";
			case DebugLevel::Area: return "AREA";
			case DebugLevel::Ese: return "ESE";
			case DebugLevel::Gen: return "GEN";
			case DebugLevel::Func: return "FUNC";
			case DebugLevel::Sync: return "SYNC";
			case DebugLevel::Cmd: return "CMD";
			default: return "N/A";
			}
		};

		//************************************
		// Description: Transform a string command into a debug level
		// Method:    stringToDebugLvl
		// FullName:  vsid::Logger::stringToDebugLvl
		// Access:    private static 
		// Returns:   constexpr vsid::DebugLevel
		// Qualifier:
		// Parameter: std::string_view lvl
		//************************************
		inline static constexpr DebugLevel stringToDebugLvl(std::string_view lvl)
		{
			if (vsid::utils::svEqualCi(lvl, "all")) return DebugLevel::All;
			if (vsid::utils::svEqualCi(lvl, "dev")) return DebugLevel::Dev;
			if (vsid::utils::svEqualCi(lvl, "menu")) return DebugLevel::Menu;
			if (vsid::utils::svEqualCi(lvl, "atc")) return DebugLevel::Atc;
			if (vsid::utils::svEqualCi(lvl, "sid")) return DebugLevel::Sid;
			if (vsid::utils::svEqualCi(lvl, "fpln")) return DebugLevel::Fpln;
			if (vsid::utils::svEqualCi(lvl, "req")) return DebugLevel::Req;
			if (vsid::utils::svEqualCi(lvl, "conf")) return DebugLevel::Conf;
			if (vsid::utils::svEqualCi(lvl, "rwy")) return DebugLevel::Rwy;
			if (vsid::utils::svEqualCi(lvl, "area")) return DebugLevel::Area;
			if (vsid::utils::svEqualCi(lvl, "ese")) return DebugLevel::Ese;
			if (vsid::utils::svEqualCi(lvl, "gen")) return DebugLevel::Gen;
			if (vsid::utils::svEqualCi(lvl, "func")) return DebugLevel::Func;
			if (vsid::utils::svEqualCi(lvl, "sync")) return DebugLevel::Sync;
			if (vsid::utils::svEqualCi(lvl, "cmd")) return DebugLevel::Cmd;

			return DebugLevel::None;
		}

		//************************************
		// Description: Checks if the given debug level is active
		// Method:    isDebugLevelActive
		// FullName:  vsid::Logger::isDebugLevelActive
		// Access:    public static 
		// Returns:   bool
		// Qualifier:
		// Parameter: DebugLevel lvl
		//************************************
		inline static bool isDebugLevelActive(DebugLevel lvl)
		{
			std::size_t idx = static_cast<std::size_t>(lvl);

			if (idx >= currentDebugLvl.size()) return false;

			return currentDebugLvl[idx];
		}

		//************************************
		// Description: Rotates log files by flushing, closing and then opening a new file
		// Method:    rotateLogs
		// FullName:  vsid::Logger::rotateLogs
		// Access:    private static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void rotateLogs();

		inline static bool logDevOnly;
		inline static std::array<bool, 16> currentDebugLvl;
		inline static std::queue<LogMessage> bgQueue;
		inline static std::queue<std::string> esQueue;
		inline static HANDLE fileLockMutex{ NULL };
		inline static std::mutex bgMutex;
		inline static std::mutex esMutex;
		inline static std::mutex consoleMutex;
		inline static std::condition_variable cv;
		inline static std::thread worker;
		inline static std::atomic<bool> running;
		inline static bool newConsole;
		inline static FILE* consoleOut;
		inline static std::ofstream logFile;
		inline static std::filesystem::path logFilePath;
	};
}
