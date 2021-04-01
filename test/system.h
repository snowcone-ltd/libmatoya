#ifdef _WIN32
#include <windows.h>

static bool checkkey(void)
{
	HKEY userkey = NULL;
	HKEY runkey = NULL;
	LSTATUS err = RegOpenCurrentUser(KEY_QUERY_VALUE, &userkey);
	if (err != ERROR_SUCCESS)
	{
		MTY_Log("RegOpenCurrentUser failed with error 0x%X", err);
		return false;
	}

	err = RegOpenKeyEx(userkey, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &runkey);
	if (err != ERROR_SUCCESS)
	{
		MTY_Log("RegOpenKeyEx failed with error 0x%X", err);
		RegCloseKey(userkey);
		return false;
	}

	DWORD size = 0;
	err = RegQueryValueEx(runkey, "TestApp", NULL, NULL, NULL, &size);
	RegCloseKey(runkey);
	RegCloseKey(userkey);

	return err == ERROR_SUCCESS;
}

typedef DWORD (WINAPI *PGetVersion)(void);

#endif // _WIN32

static bool system_main(void)
{
	#if defined(_WIN32)
		MTY_SO* soctx = MTY_SOLoad("kernel32.dll");
		test_cmp("MTY_SOLoad", soctx != NULL);

		PGetVersion _GetVersion = (PGetVersion) MTY_SOGetSymbol(soctx, "GetVersion");
		test_cmp("MTY_SOGetSymbol", _GetVersion);

		DWORD v = _GetVersion();
		test_cmpi32("_GetVersion", v, v);

		MTY_SOUnload(&soctx);
		test_cmp("MTY_SOUnload", soctx == NULL);
	#endif

	const char* hostname = MTY_GetHostname();
	test_cmp("MTY_GetHostname", strcmp(hostname, "noname"));

	uint32_t plat = MTY_GetPlatform();
	test_cmpi32("MTY_GetPlatform", plat > 0, plat);

	plat = MTY_GetPlatformNoWeb();
	test_cmpi32("MTY_GetPlatformNoWeb", plat > 0, plat);

	bool r = MTY_IsSupported();
	test_cmp("MTY_IsSupported", r);

	const char* procpath = MTY_GetProcessPath(); //Needs checks depending on the platform
	test_cmps("MTY_GetProcessPath", procpath, procpath);

#ifdef _WIN32
	MTY_SetRunOnStartup("TestApp", procpath, NULL);
	test_cmp("MTY_SetRunOnStartup", checkkey());
	r = MTY_GetRunOnStartup("TestApp");
	test_cmp("MTY_GetRunOnStartup", r);
	MTY_SetRunOnStartup("TestApp", NULL, NULL);
	test_cmp("MTY_SetRunOnStartup", !checkkey());
	r = MTY_GetRunOnStartup("TestApp");
	test_cmp("MTY_GetRunOnStartup", !r);
#else
	r = MTY_GetRunOnStartup("Unused");
	test_cmp("MTY_GetRunOnStartup", !r);
#endif
	const char* verstring = MTY_GetPlatformString(plat);
	test_cmps("MTY_GetVersionString", verstring, verstring);

	return true;
}
