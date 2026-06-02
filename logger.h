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
		Sync
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
		// Description: Toggles the console on or off
		// Method:    toggleConsole
		// FullName:  vsid::Logger::toggleConsole
		// Access:    public static 
		// Returns:   void
		// Qualifier:
		//************************************
		static void toggleConsole();

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

		inline static bool getLogDevOnly() { return logDevOnly; }

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
			default: return "N/A";
			}
		};

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
