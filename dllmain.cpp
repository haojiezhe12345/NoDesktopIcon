// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

void LogText(const char* format, ...) {
	char buf1[1000] = "ExplorerNoDesktopIcons: ";

	char buf2[900];
	va_list args;
	va_start(args, format);
	vsnprintf(buf2, sizeof(buf2), format, args);
	va_end(args);

	strcat_s(buf1, buf2);

	OutputDebugStringA(buf1);
}

char* extractProcessName(char* fullCmdLine) {
	char* processName = strrchr(fullCmdLine, '\\');
	if (processName) {
		processName++;
	}
	else {
		processName = fullCmdLine;
	}
	return processName;
}

// --- Signature Definition ---
// Original: 8B FA 48 8B D9 BE 01 00 00 00 75 ??
const unsigned char ORIGINAL_SIGNATURE[] = { 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0xBE, 0x01, 0x00, 0x00, 0x00, 0x75 };
const size_t SIG_LENGTH = sizeof(ORIGINAL_SIGNATURE);

// --- Patch Definition ---
// Replacement: BE 00 00 00 00 EB ??
const unsigned char PATCH_PAYLOAD[] = { 0xBE, 0x00, 0x00, 0x00, 0x00, 0xEB };
const size_t PATCH_LENGTH = sizeof(PATCH_PAYLOAD);
const size_t SIG_PATCH_OFFSET = 5;

BOOL g_canDllUnload = FALSE;

// ----------------------------------------------------------------------
// Signature Scanning Function
// ----------------------------------------------------------------------

/**
 * @brief Scans a memory region for a specific byte signature (pattern) with a wildcard.
 */
LPVOID FindPattern(LPVOID baseAddress, DWORD size, const unsigned char* pattern, size_t len) {
	const unsigned char* data = (const unsigned char*)baseAddress;

	for (DWORD i = 0; i <= size - len; ++i) {
		bool found = true;
		for (size_t j = 0; j < len; ++j) {
			// Index 10 is the jump condition byte (75) which is required for the signature to match.
			// The wildcard is actually the byte *after* the signature (the jump offset '??').
			// We check up to the '75' and ensure the next byte exists, but we don't compare it.
			if (data[i + j] != pattern[j]) {
				found = false;
				break;
			}
		}
		if (found) {
			// Found the start of the signature
			return (LPVOID)(data + i);
		}
	}
	return nullptr;
}

// ----------------------------------------------------------------------
// Patching Logic
// ----------------------------------------------------------------------

DWORD WINAPI PerformPatch(LPVOID) {
	// 1. Get a handle to shell32.dll
	HMODULE hShell32 = GetModuleHandleA("shell32.dll");
	if (!hShell32) {
		LogText("shell32.dll not found!");
		return 0;
	}

	// 2. Get information about the loaded module
	MODULEINFO mi;
	if (!GetModuleInformation(GetCurrentProcess(), hShell32, &mi, sizeof(mi))) return 1;

	LPVOID baseAddress = mi.lpBaseOfDll;
	DWORD moduleSize = mi.SizeOfImage;

	// 3. Scan for the signature
	LPVOID sigAddress = FindPattern(baseAddress, moduleSize, ORIGINAL_SIGNATURE, SIG_LENGTH);

	if (!sigAddress) {
		// Patching failed due to signature mismatch (possible Windows Update)
		LogText("Cannot find patch offset!");
		return 0;
	}

	// 4. Determine target address and jump offset
	// Target address is 5 bytes after the signature start (after 0x8B D9)
	LPVOID targetAddress = (char*)sigAddress + SIG_PATCH_OFFSET;
	LogText("Found patch offset: 0x%p", targetAddress);

	// 5. Change memory protection to allow writing
	DWORD oldProtect;
	if (!VirtualProtect(targetAddress, PATCH_LENGTH, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		LogText("Cannot change memory protection!");
		return 0;
	}

	// 6. Write the patch (6 bytes: mov esi, 0; jmp short)
	memcpy(targetAddress, PATCH_PAYLOAD, PATCH_LENGTH);

	// 7. Restore memory protection
	VirtualProtect(targetAddress, PATCH_LENGTH, oldProtect, &oldProtect);

	// Flush the instruction cache
	FlushInstructionCache(GetCurrentProcess(), targetAddress, PATCH_LENGTH);

	LogText("Patch success!");

	g_canDllUnload = TRUE;
	return 0;
}

// ----------------------------------------------------------------------
// 4. Exported COM Functions
// ----------------------------------------------------------------------

// This function is called before the desktop is initiated, and it is blocking, making it perfect to patch memory
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
	static BOOL patched = FALSE;
	do {
		if (patched) break;
		patched = TRUE;

		LPSTR lpCmdLine = GetCommandLineA();
		char* processArgs = extractProcessName(lpCmdLine);
		LogText("Command line: %s", processArgs);

		// Only patch the desktop process, skip if it's folder window process
		if (_stricmp(processArgs, "explorer.exe") &&
			_stricmp(processArgs, "explorer.exe ") &&
			_stricmp(processArgs, "explorer.exe\"") &&
			_stricmp(processArgs, "\"explorer.exe\"") &&
			_stricmp(processArgs, "explorer") &&
			_stricmp(processArgs, "explorer "))
		{
			LogText("Not a desktop process");
			g_canDllUnload = TRUE;
			break;
		}

		LogText("Start patching shell32.dll");
		PerformPatch(NULL);

	} while (0);

	return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow(void) {
	return g_canDllUnload ? S_OK : S_FALSE;
}


// ----------------------------------------------------------------------
// DLL Entry Point
// ----------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
	{
		char szProcessPath[MAX_PATH];
		GetModuleFileNameA(NULL, szProcessPath, MAX_PATH);
		char* processName = extractProcessName(szProcessPath);
		LogText("Attached to %s", processName);

		// CLSID registered in `HKEY_CLASSES_ROOT\Drive\shellex\FolderExtensions` will be loaded into a wide range of programs (e.g. taskmgr, svchost, and even Chrome)
		// We should only inject to explorer.exe
		if (_stricmp(processName, "explorer.exe")) {
			return FALSE;  // return false to immediately unload from a process
		}

		// Disable notification calls when a thread is created/destoyed
		DisableThreadLibraryCalls(hinstDLL);

		break;
	}

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
