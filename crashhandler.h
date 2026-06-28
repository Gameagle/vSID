#pragma once

#include <windows.h>

#include <dbghelp.h>
#include <iostream>
#include <fstream>
#include <string>
#include <exception>

#include "logger.h"

#include "include/es/EuroScopePlugIn.h"

// expose plug-in entry point for crash handler to identify vsid module
void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance);

namespace vsid
{
	namespace crashhandler
	{
		const int stackFrames = 100;

		enum DumpMode {
			None,
			MiniDump,
			FullDump
		};

		//************************************
		// Description: Walk the stack and write the stack trace to the log file
		// Method:    writeStackTrace
		// FullName:  vsid::crashhandler::writeStackTrace
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EXCEPTION_POINTERS * exceptionInfo
		//************************************
		void writeStackTrace(EXCEPTION_POINTERS* exceptionInfo);

		//************************************
		// Description: Vectored Exception Handler that checks if the crash happened in the vsid module and if so, writes the stack trace to the log file
		// Method:    vSIDCrashHandler
		// FullName:  vsid::crashhandler::vSIDCrashHandler
		// Access:    public 
		// Returns:   LONG WINAPI
		// Qualifier:
		// Parameter: EXCEPTION_POINTERS * exceptionInfo
		//************************************
		inline LONG WINAPI vSIDCrashHandler(EXCEPTION_POINTERS* exceptionInfo)
		{
			// early-exit in case of 0xE06D7363 (MSVC exeption code)
			if (exceptionInfo->ExceptionRecord->ExceptionCode == 0xE06D7363)
				return EXCEPTION_CONTINUE_SEARCH;

			PVOID crashAdress = exceptionInfo->ExceptionRecord->ExceptionAddress;

			// get the crashed module
			HMODULE crashedModule = NULL;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)crashAdress,
				&crashedModule
			);

			// get vsid module
			HMODULE vsidModule = NULL;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)&EuroScopePlugInInit,
				&vsidModule
			);

			if (crashedModule != NULL && crashedModule == vsidModule)
			{
				writeStackTrace(exceptionInfo);
				vsid::Logger::panicFlush(); // flush logs to file in case of crash
			}
			
			return EXCEPTION_CONTINUE_SEARCH;
		}

		//************************************
		// Description: Handles all unhandled exceptions in the plugin, logs and then force crashes ES
		// Method:    vSIDTerminateHandler
		// FullName:  vsid::crashhandler::vSIDTerminateHandler
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		inline void vSIDTerminateHandler()
		{
			try
			{
				if (auto ep = std::current_exception())
					std::rethrow_exception(ep);
			}
			catch (const std::exception& e)
			{
				vsid::Logger::log(LogLevel::Error, std::string("[CRASH DUMP] Unhandled Exception occurred in vSID!") + e.what());
			}
			catch (...)
			{
				vsid::Logger::log(LogLevel::Error, "[CRASH DUMP] Unknown Unhandled Exception occurred in vSID!");
			}

			vsid::Logger::panicFlush();

			abort(); // 
		}

		//************************************
		// Description: Initializes the crash handler
		// Method:    initCrashHandler
		// FullName:  vsid::crashhandler::initCrashHandler
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void initCrashHandler();

		//************************************
		// Description: Removes the crash handler
		// Method:    removeCrashHandler
		// FullName:  vsid::crashhandler::removeCrashHandler
		// Access:    public 
		// Returns:   void
		// Qualifier:
		//************************************
		void removeCrashHandler();

		//************************************
		// Description: create a dump file. If "create_fulldump" file is present a full dump is generated, otherwise a minidump
		// Method:    createDumpFile
		// FullName:  vsid::crashhandler::createDumpFile
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: EXCEPTION_POINTERS * exceptionInfo
		// Parameter: const std::string & directoryPath
		//************************************
		void createDumpFile(EXCEPTION_POINTERS* exceptionInfo, const std::string& directoryPath);

		//************************************
		// Description: Checks for the presence of a "create_fulldump" file in the given directory to determine the dump mode
		// Method:    getDumpMode
		// FullName:  vsid::crashhandler::getDumpMode
		// Access:    public 
		// Returns:   vsid::crashhandler::DumpMode
		// Qualifier:
		// Parameter: const std::string & directoryPath
		//************************************
		DumpMode getDumpMode(const std::string& directoryPath);
	}
}
