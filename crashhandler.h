#pragma once

#include <windows.h>

#include <dbghelp.h>
#include <iostream>
#include <fstream>
#include <string>

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
			}
			
			return EXCEPTION_CONTINUE_SEARCH;
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
