#include "pch.h"

#include <minidumpapiset.h> 

#include "crashhandler.h"
#include "messageHandler.h"
#include "timeHandler.h"

PVOID vehHandler = NULL;

void vsid::crashhandler::writeStackTrace(EXCEPTION_POINTERS* exceptionInfo)
{
	HMODULE vsidModule = NULL;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&EuroScopePlugInInit,
		&vsidModule
	);

	std::string crashLogPath = "vsid_crash.txt";
	std::string dumpPath = "";
	char pdbSearchPath[MAX_PATH] = { 0 };

	if (vsidModule != NULL)
	{
		char path[MAX_PATH];
		GetModuleFileNameA(vsidModule, path, MAX_PATH);
		std::string fullPath(path);
		size_t lastSlash = fullPath.find_last_of("\\/");
		if (lastSlash != std::string::npos)
		{
			crashLogPath = fullPath.substr(0, lastSlash + 1) + "vsid_crash.txt";
			dumpPath = fullPath.substr(0, lastSlash + 1);
			strncpy_s(pdbSearchPath, path, lastSlash);
		}
	}

	vsid::crashhandler::createDumpFile(exceptionInfo, dumpPath);

	std::ofstream logFile(crashLogPath, std::ios::app);
	if (!logFile.is_open()) return;

	HANDLE process = GetCurrentProcess();

	// check for pdb file next to dll
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
	SymInitialize(process, (pdbSearchPath[0] == '\0') ? NULL : pdbSearchPath, TRUE);

	DWORD64 crashAdress = (DWORD64)exceptionInfo->ExceptionRecord->ExceptionAddress;

	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	IMAGEHLP_LINE64	line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD displacement;
	DWORD64 symDisplacement = 0;

	logFile << "--- CRITICAL ERROR [" << vsid::time::getDate() << " - " << vsid::time::toTimeString(vsid::time::getUtcNow()) << "] ---" << std::endl;
	logFile << "Crash Address: 0x" << std::hex << crashAdress << std::dec << std::endl;

	// print the crash address symbol and line info if available otherwise print the offset
	if (SymFromAddr(process, crashAdress, &symDisplacement, symbol))
	{
		if (SymGetLineFromAddr64(process, crashAdress, &displacement, &line))
		{
			const char* shortFilename = strrchr(line.FileName, '\\');
			if (shortFilename == nullptr) shortFilename = strrchr(line.FileName, '/');
			shortFilename = (shortFilename != nullptr) ? shortFilename + 1 : line.FileName;

			logFile << "Crashed at: " << symbol->Name << " in " << shortFilename << " (line: " << line.LineNumber << ")" << std::endl;
		}
		else
		{
			logFile << "Crashed at: " << symbol->Name << " (no line info found)" << std::endl;
		}
	}
	else
	{
		DWORD64 baseAddr = (DWORD64)vsidModule;
		DWORD64 offset = crashAdress - baseAddr;
		logFile << "Crashed at: [PDB FAILED] Offset 0x" << std::hex << offset << std::dec << " from base of DLL" << std::endl;
		logFile << "If no PDB file is available [PDB FAILED] is expected, otherwise this might indicate an issue with the PDB file or symbol loading." << std::endl;
	}

	logFile << "Stacktrace:" << std::endl;

	// stack walk
	CONTEXT context = *exceptionInfo->ContextRecord;
	STACKFRAME64 frame = { 0 };

	DWORD machineType = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Esp;
	frame.AddrStack.Mode = AddrModeFlat;

	int frameCount = 0;

	// walk the stack to the end or until the set max frames
	while (StackWalk64(machineType, process, GetCurrentThread(), &frame, &context,
		NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
	{
		if (frame.AddrPC.Offset == 0) break;
		if (frameCount++ >= vsid::crashhandler::stackFrames) break;

		DWORD64 address = frame.AddrPC.Offset;

		HMODULE frameModule = NULL;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)address, &frameModule);

		if (frameModule != vsidModule) continue;

		DWORD64 lookupAdress = (address > 0) ? (address - 1) : address;

		if (SymFromAddr(process, lookupAdress, &symDisplacement, symbol))
		{
			if (SymGetLineFromAddr64(process, lookupAdress, &displacement, &line))
			{
				const char* filename = strrchr(line.FileName, '\\');
				if (filename == nullptr) filename = strrchr(line.FileName, '/');
				filename = (filename != nullptr) ? filename + 1 : line.FileName;

				logFile << "[" << frameCount - 1 << "] " << symbol->Name << " in " << filename << " (line: " << line.LineNumber << ")" << std::endl;
			}
			else
			{
				logFile << "[" << frameCount - 1 << "] " << symbol->Name << " (no line info)" << std::endl;
			}
		}
		else
		{
			DWORD64 offset = lookupAdress - (DWORD64)vsidModule;
			logFile << "[" << frameCount - 1 << "] [PDB FAILED] Offset 0x" << std::hex << offset << std::dec << std::endl;
		}
	}

	logFile << "--- CRITICAL ERROR END [" << vsid::time::getDate() << " - " << vsid::time::toTimeString(vsid::time::getUtcNow()) << "] ---\n" << std::endl;

	free(symbol);
	SymCleanup(process);
	logFile.close();
}

void vsid::crashhandler::initCrashHandler()
{
	if (vehHandler == NULL) vehHandler = AddVectoredExceptionHandler(1, vsid::crashhandler::vSIDCrashHandler);
}

void vsid::crashhandler::removeCrashHandler()
{
	if (vehHandler != NULL)
	{
		RemoveVectoredExceptionHandler(vehHandler);
		vehHandler = NULL;
	}
}

void vsid::crashhandler::createDumpFile(EXCEPTION_POINTERS* exceptionInfo, const std::string& directoryPath)
{
	// get dump mode for file name
	DumpMode dumpMode = getDumpMode(directoryPath);
	std::string dumpTypeStr = (dumpMode == vsid::crashhandler::DumpMode::FullDump) ? "FULL" : "MINI";

	// create dump file name
	std::string dumpFilePath = directoryPath + "vsid_dump_" + dumpTypeStr + ".dmp";

	// create dump file via winapi
	HANDLE hFile = CreateFileA(
		dumpFilePath.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile == INVALID_HANDLE_VALUE) return;

	// prepare and set exception info
	MINIDUMP_EXCEPTION_INFORMATION mdei;
	mdei.ThreadId = GetCurrentThreadId();
	mdei.ExceptionPointers = exceptionInfo;
	mdei.ClientPointers = FALSE;

	// get dump type
	MINIDUMP_TYPE dumpType = MiniDumpNormal;

	// get full dump
	if (dumpMode == vsid::crashhandler::DumpMode::FullDump)
		dumpType = (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo);

	// write dump
	MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		dumpType,
		(exceptionInfo != NULL) ? &mdei : NULL,
		NULL,
		NULL
	);

	CloseHandle(hFile);
}

vsid::crashhandler::DumpMode vsid::crashhandler::getDumpMode(const std::string& directoryPath)
{
	std::string fullDumpTrigger = directoryPath + "create_fulldump";

	if (GetFileAttributesA(fullDumpTrigger.c_str()) != INVALID_FILE_ATTRIBUTES)
		return vsid::crashhandler::DumpMode::FullDump;
	else
		return vsid::crashhandler::DumpMode::MiniDump;
}