// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <signal.h>

#include <windows.h>
#include <process.h>
#include <userenv.h>

#include "tlocal.h"

static MTY_CrashFunc SYSTEM_CRASH_FUNC;
static void *SYSTEM_OPAQUE;

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

MTY_SO *MTY_SOLoad(const char *path)
{
	wchar_t *wpath = MTY_MultiToWideD(path);

	MTY_SO *so = (MTY_SO *) LoadLibrary(wpath);
	MTY_Free(wpath);

	if (!so)
		MTY_Log("'LoadLibrary' failed to find '%s' with error 0x%X", path, GetLastError());

	return so;
}

void *MTY_SOGetSymbol(MTY_SO *so, const char *name)
{
	void *sym = (void *) GetProcAddress((HMODULE) so, name);
	if (!sym)
		MTY_Log("'GetProcAddress' failed to find '%s' with error 0x%X", name, GetLastError());

	return sym;
}

void MTY_SOUnload(MTY_SO **so)
{
	if (!so || !*so)
		return;

	if (!FreeLibrary((HMODULE) *so))
		MTY_Log("'FreeLibrary' failed with error 0x%X", GetLastError());

	*so = NULL;
}

const char *MTY_GetSOExtension(void)
{
	return "dll";
}

const char *MTY_GetHostname(void)
{
	DWORD lenw = MAX_COMPUTERNAME_LENGTH + 1;
	WCHAR tmp[MAX_COMPUTERNAME_LENGTH + 1] = {0};

	if (!GetComputerName(tmp, &lenw)) {
		MTY_Log("'GetComputerName' failed with error 0x%X", GetLastError());
		return "noname";
	}

	return MTY_WideToMultiDL(tmp);
}

uint32_t MTY_GetPlatform(void)
{
	uint32_t v = MTY_OS_WINDOWS;

	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (ntdll) {
		NTSTATUS (WINAPI *RtlGetVersion)(RTL_OSVERSIONINFOW *info) =
			(void *) GetProcAddress(ntdll, "RtlGetVersion");

		if (RtlGetVersion) {
			RTL_OSVERSIONINFOW info = {0};
			info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

			RtlGetVersion(&info);

			// Windows "11"
			if (info.dwBuildNumber >= 22000) {
				info.dwMajorVersion = 11;
				info.dwMinorVersion = 0;
			}

			v |= (uint32_t) info.dwMajorVersion << 8;
			v |= (uint32_t) info.dwMinorVersion;
		}
	}

	return v;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

void MTY_HandleProtocol(const char *uri, void *token)
{
	VOID *env = NULL;

	if (token) {
		if (!CreateEnvironmentBlock(&env, token, FALSE)) {
			MTY_Log("'CreateEnvironmentBlock' failed with error 0x%X", GetLastError());
			return;
		}
	}

	WCHAR *wuri = MTY_MultiToWideD(uri);
	WCHAR *cmd = MTY_Alloc(MTY_PATH_MAX, sizeof(WCHAR));
	_snwprintf_s(cmd, MTY_PATH_MAX, _TRUNCATE, L"rundll32 url.dll,FileProtocolHandler %s", wuri);

	STARTUPINFO si = {0};
	si.cb = sizeof(STARTUPINFO);
	si.lpDesktop = L"winsta0\\default";

	PROCESS_INFORMATION pi = {0};
	DWORD flags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT;

	BOOL success = FALSE;

	if (token) {
		success = CreateProcessAsUser(token, NULL, cmd, NULL, NULL, FALSE, flags, env, NULL, &si, &pi);
		if (!success)
			MTY_Log("'CreateProcessAsUser' failed with error 0x%X", GetLastError());

	} else {
		success = CreateProcess(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi);
		if (!success)
			MTY_Log("'CreateProcess' failed with error 0x%X", GetLastError());
	}

	if (success) {
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}

	MTY_Free(wuri);
	MTY_Free(cmd);

	if (env)
		DestroyEnvironmentBlock(env);
}

const char *MTY_GetProcessPath(void)
{
	WCHAR tmp[MTY_PATH_MAX] = {0};

	if (GetModuleFileName(NULL, tmp, MTY_PATH_MAX) <= 0)
		MTY_Log("'GetModuleFileName' failed with error 0x%X", GetLastError());

	return MTY_WideToMultiDL(tmp);
}

bool MTY_StartInProcess(const char *path, char * const *argv, const char *dir)
{
	if (dir) {
		WCHAR dirw[MAX_PATH] = {0};
		MTY_MultiToWide(dir, dirw, MAX_PATH);
		SetCurrentDirectory(dirw);
	}

	WCHAR *pathw = MTY_MultiToWideD(path ? path : MTY_GetProcessPath());

	// Rebuild argv with wide strings
	size_t argc = 0;
	for (; argv[argc]; argc++);

	WCHAR **argvn = MTY_Alloc(argc + 1, sizeof(WCHAR *));
	argvn[argc] = NULL;
	for (uint32_t x = 0; x < argc; x++)
		argvn[x] = MTY_MultiToWideD(argv[x]);

	// flush and exec
	_flushall();
	_wexecv(pathw, argvn);

	// Reaching here means exec failed. Clean up and free
	MTY_Log("'_wexecv' failed with errno %d", errno);

	for (uint32_t x = 0; argvn && argvn[x]; x++)
		MTY_Free(argvn[x]);

	MTY_Free(argvn);
	MTY_Free(pathw);

	return false;
}

bool MTY_RestartProcess(char * const *argv)
{
	return MTY_StartInProcess(NULL, argv, NULL);
}

static LONG WINAPI system_exception_handler(EXCEPTION_POINTERS *ex)
{
	if (SYSTEM_CRASH_FUNC)
		SYSTEM_CRASH_FUNC(false, SYSTEM_OPAQUE);

	return EXCEPTION_CONTINUE_SEARCH;
}

static void system_signal_handler(int32_t sig)
{
	if (SYSTEM_CRASH_FUNC)
		SYSTEM_CRASH_FUNC(sig == SIGTERM || sig == SIGINT, SYSTEM_OPAQUE);

	signal(sig, SIG_DFL);
	raise(sig);
}

void MTY_SetCrashFunc(MTY_CrashFunc func, void *opaque)
{
	SetUnhandledExceptionFilter(system_exception_handler);

	signal(SIGINT, system_signal_handler);
	signal(SIGSEGV, system_signal_handler);
	signal(SIGABRT, system_signal_handler);
	signal(SIGTERM, system_signal_handler);

	SYSTEM_CRASH_FUNC = func;
	SYSTEM_OPAQUE = opaque;
}

void MTY_OpenConsole(const char *title)
{
	HWND console = GetConsoleWindow();

	if (console) {
		ShowWindow(console, TRUE);

	} else {
		AllocConsole();
		AttachConsole(ATTACH_PARENT_PROCESS);
		SetConsoleOutputCP(CP_UTF8);
		// SetCurrentConsoleFontEx for asain characters

		// This will cause the Application Verifier leak check to complain
		FILE *f = NULL;
		freopen_s(&f, "CONOUT$", "w", stdout);

		// Disable quickedit mode
		HANDLE h = GetStdHandle(STD_INPUT_HANDLE);

		DWORD mode = 0;
		GetConsoleMode(h, &mode);

		mode |= ENABLE_EXTENDED_FLAGS;
		mode &= ~ENABLE_QUICK_EDIT_MODE;
		SetConsoleMode(h, mode);

		// Set Console Title
		WCHAR *titlew = MTY_MultiToWideD(title);
		SetConsoleTitle(titlew);

		MTY_Free(titlew);
	}
}

void MTY_CloseConsole(void)
{
	// FreeConsole is unsafe, leave it allocated to the process and just hide it
	// https://stackoverflow.com/questions/12676312/freeconsole-behaviour-on-windows-8

	HWND console = GetConsoleWindow();

	if (console) {
		system("cls");
		ShowWindow(console, FALSE);
	}
}


// Run on startup (registry)

#define SYSTEM_REG_PATH L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

static bool system_startup_hkey(REGSAM sam, HKEY *key, HKEY *ukey)
{
	LSTATUS e = RegOpenCurrentUser(sam, ukey);
	if (e != ERROR_SUCCESS) {
		MTY_Log("'RegOpenCurrentUser' failed with error 0x%X", e);
		return false;
	}

	e = RegOpenKeyEx(*ukey, SYSTEM_REG_PATH, 0, sam, key);

	if (e != ERROR_SUCCESS) {
		MTY_Log("'RegOpenKeyEx' failed with error 0x%X", e);

		RegCloseKey(*ukey);
		*ukey = NULL;

		return false;
	}

	return true;
}

bool MTY_GetRunOnStartup(const char *name)
{
	HKEY key = NULL;
	HKEY ukey = NULL;
	if (!system_startup_hkey(KEY_QUERY_VALUE, &key, &ukey))
		return false;

	DWORD size = 0;
	WCHAR *namew = MTY_MultiToWideD(name);
	bool r = RegQueryValueEx(key, namew, NULL, NULL, NULL, &size) == ERROR_SUCCESS;

	MTY_Free(namew);
	RegCloseKey(key);
	RegCloseKey(ukey);

	return r;
}

void MTY_SetRunOnStartup(const char *name, const char *path, const char *args)
{
	HKEY key = NULL;
	HKEY ukey = NULL;
	if (!system_startup_hkey(KEY_QUERY_VALUE | KEY_SET_VALUE, &key, &ukey))
		return;

	WCHAR *namew = MTY_MultiToWideD(name);

	if (!path) {
		RegDeleteValue(key, namew);

	} else {
		char *full = args ? MTY_SprintfD("%s %s", path, args) : MTY_Strdup(path);
		WCHAR *fullw = MTY_MultiToWideD(full);

		DWORD len = ((DWORD) wcslen(fullw) + 1) * sizeof(WCHAR);

		LSTATUS e = RegSetValueEx(key, namew, 0, REG_SZ, (BYTE *) fullw, len);
		if (e != ERROR_SUCCESS)
			MTY_Log("'RegSetValueEx' failed with error 0x%X", e);

		MTY_Free(fullw);
		MTY_Free(full);
	}

	MTY_Free(namew);
	RegCloseKey(key);
	RegCloseKey(ukey);
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
