// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>


// Interface

#if defined(__linux__)
	#define STEAM_API
	#define STEAM_SO_NAME "libsteam_api.so"
	#pragma pack(push, 4)

#elif defined(__APPLE__)
	#define STEAM_API
	#define STEAM_SO_NAME "libsteam_api.dylib"
	#pragma pack(push, 4)

#else
	#define STEAM_API __stdcall
	#if defined(_WIN64)
		#define STEAM_SO_NAME "steam_api64.dll"
	#else
		#define STEAM_SO_NAME "steam_api.dll"
	#endif

	#pragma pack(push, 8)
#endif


// Common

typedef uint64_t SteamAPICall_t;

enum {
	k_iSteamHTMLSurfaceCallbacks = 4500,
	HTML_BrowserReady_t_k_iCallback = k_iSteamHTMLSurfaceCallbacks + 1,
	HTML_NeedsPaint_t_k_iCallback = k_iSteamHTMLSurfaceCallbacks + 2,
	HTML_FinishedRequest_t_k_iCallback = k_iSteamHTMLSurfaceCallbacks + 6,
	HTML_JSAlert_t_k_iCallback = k_iSteamHTMLSurfaceCallbacks + 14,
	HTML_SetCursor_t_k_iCallback = k_iSteamHTMLSurfaceCallbacks + 22,
};

static bool (STEAM_API *SteamAPI_InitSafe)(void);
static void (STEAM_API *SteamAPI_Shutdown)(void);
static void (STEAM_API *SteamAPI_RunCallbacks)(void);
static void (STEAM_API *SteamAPI_RegisterCallback)(void *pCallback, int32_t iCallback);
static void (STEAM_API *SteamAPI_UnregisterCallback)(void *pCallback);
static void (STEAM_API *SteamAPI_RegisterCallResult)(void *pCallback, SteamAPICall_t hAPICall);
static void (STEAM_API *SteamAPI_UnregisterCallResult)(void *pCallback, SteamAPICall_t hAPICall);


// ISteamHTMLSurface

typedef struct ISteamHTMLSurface ISteamHTMLSurface;
typedef uint32_t HHTMLBrowser;
typedef uint32_t RTime32;

typedef enum {
	eHTMLMouseButton_Left = 0,
	eHTMLMouseButton_Right = 1,
	eHTMLMouseButton_Middle = 2,
} EHTMLMouseButton;

typedef enum {
	k_eHTMLKeyModifier_None = 0,
	k_eHTMLKeyModifier_AltDown = 1 << 0,
	k_eHTMLKeyModifier_CtrlDown = 1 << 1,
	k_eHTMLKeyModifier_ShiftDown = 1 << 2,
} EHTMLKeyModifiers;

typedef enum {
	dc_user = 0,
	dc_none,
	dc_arrow,
	dc_ibeam,
	dc_hourglass,
	dc_waitarrow,
	dc_crosshair,
	dc_up,
	dc_sizenw,
	dc_sizese,
	dc_sizene,
	dc_sizesw,
	dc_sizew,
	dc_sizee,
	dc_sizen,
	dc_sizes,
	dc_sizewe,
	dc_sizens,
	dc_sizeall,
	dc_no,
	dc_hand,
	dc_blank,
	dc_middle_pan,
	dc_north_pan,
	dc_north_east_pan,
	dc_east_pan,
	dc_south_east_pan,
	dc_south_pan,
	dc_south_west_pan,
	dc_west_pan,
	dc_north_west_pan,
	dc_alias,
	dc_cell,
	dc_colresize,
	dc_copycur,
	dc_verticaltext,
	dc_rowresize,
	dc_zoomin,
	dc_zoomout,
	dc_help,
	dc_custom,
	dc_last,
} EMouseCursor;

typedef struct {
	HHTMLBrowser unBrowserHandle;
} HTML_BrowserReady_t;

typedef struct {
	HHTMLBrowser unBrowserHandle;
	const char *pBGRA;
	uint32_t unWide;
	uint32_t unTall;
	uint32_t unUpdateX;
	uint32_t unUpdateY;
	uint32_t unUpdateWide;
	uint32_t unUpdateTall;
	uint32_t unScrollX;
	uint32_t unScrollY;
	float flPageScale;
	uint32_t unPageSerial;
} HTML_NeedsPaint_t;

typedef struct {
	HHTMLBrowser unBrowserHandle;
	const char *pchURL;
	const char *pchPageTitle;
} HTML_FinishedRequest_t;

typedef struct {
	HHTMLBrowser unBrowserHandle;
	const char *pchMessage;
} HTML_JSAlert_t;

typedef struct {
	HHTMLBrowser unBrowserHandle;
	uint32_t eMouseCursor;
} HTML_SetCursor_t;

static ISteamHTMLSurface *(STEAM_API *SteamAPI_SteamHTMLSurface_v005)(void);
static bool (STEAM_API *SteamAPI_ISteamHTMLSurface_Init)(ISteamHTMLSurface *self);
static bool (STEAM_API *SteamAPI_ISteamHTMLSurface_Shutdown)(ISteamHTMLSurface *self);
static SteamAPICall_t (STEAM_API *SteamAPI_ISteamHTMLSurface_CreateBrowser)(ISteamHTMLSurface *self, const char *pchUserAgent, const char *pchUserCSS);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_RemoveBrowser)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_LoadURL)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, const char *pchURL, const char *pchPostData);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetSize)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, uint32_t unWidth, uint32_t unHeight);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_Reload)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_AddHeader)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, const char *pchKey, const char *pchValue);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_ExecuteJavascript)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, const char *pchScript);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_MouseUp)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_MouseDown)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_MouseDoubleClick)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_MouseMove)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, int32_t x, int32_t y);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_MouseWheel)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, int32_t nDelta);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_KeyDown)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, uint32_t nNativeKeyCode, EHTMLKeyModifiers eHTMLKeyModifiers, bool bIsSystemKey);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_KeyUp)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, uint32_t nNativeKeyCode, EHTMLKeyModifiers eHTMLKeyModifiers);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_KeyChar)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, uint32_t cUnicodeChar, EHTMLKeyModifiers eHTMLKeyModifiers);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetKeyFocus)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, bool bHasKeyFocus);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_CopyToClipboard)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_PasteFromClipboard)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetCookie)(ISteamHTMLSurface *self, const char *pchHostname, const char *pchKey, const char *pchValue, const char *pchPath, RTime32 nExpires, bool bSecure, bool bHTTPOnly);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetPageScaleFactor)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, float flZoom, int32_t nPointX, int32_t nPointY);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetBackgroundMode)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, bool bBackgroundMode);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, float flDPIScaling);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_OpenDeveloperTools)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_AllowStartRequest)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, bool bAllowed);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_JSDialogResponse)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, bool bResult);
static void (STEAM_API *SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse)(ISteamHTMLSurface *self, HHTMLBrowser unBrowserHandle, const char **pchSelectedFiles);

#pragma pack(pop)


// Runtime open

static MTY_SO *STEAM_SO;
static MTY_Atomic32 STEAM_LOCK;
static uint32_t STEAM_REF;

static void steam_global_destroy(void)
{
	MTY_GlobalLock(&STEAM_LOCK);

	if (STEAM_REF > 0)
		if (--STEAM_REF == 0)
			MTY_SOUnload(&STEAM_SO);

	MTY_GlobalUnlock(&STEAM_LOCK);
}

static bool steam_global_init(const char *path)
{
	MTY_GlobalLock(&STEAM_LOCK);

	bool r = true;

	if (STEAM_REF == 0) {
		STEAM_SO = MTY_SOLoad(MTY_JoinPath(path, STEAM_SO_NAME));
		if (!STEAM_SO) {
			r = false;
			goto except;
		}

		#define STEAM_LOAD_SYM(name) \
			name = MTY_SOGetSymbol(STEAM_SO, #name); \
			if (!name) {r = false; goto except;}

		STEAM_LOAD_SYM(SteamAPI_InitSafe);
		STEAM_LOAD_SYM(SteamAPI_Shutdown);
		STEAM_LOAD_SYM(SteamAPI_RunCallbacks);
		STEAM_LOAD_SYM(SteamAPI_RegisterCallback);
		STEAM_LOAD_SYM(SteamAPI_UnregisterCallback);
		STEAM_LOAD_SYM(SteamAPI_RegisterCallResult);
		STEAM_LOAD_SYM(SteamAPI_UnregisterCallResult);

		STEAM_LOAD_SYM(SteamAPI_SteamHTMLSurface_v005);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_Init);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_Shutdown);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_CreateBrowser);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_RemoveBrowser);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_LoadURL);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetSize);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_Reload);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_AddHeader);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_ExecuteJavascript);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_MouseUp);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_MouseDown);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_MouseDoubleClick);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_MouseMove);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_MouseWheel);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_KeyDown);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_KeyUp);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_KeyChar);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetKeyFocus);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_CopyToClipboard);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_PasteFromClipboard);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetCookie);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetPageScaleFactor);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetBackgroundMode);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_OpenDeveloperTools);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_AllowStartRequest);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_JSDialogResponse);
		STEAM_LOAD_SYM(SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse);

		except:

		if (!r)
			MTY_SOUnload(&STEAM_SO);
	}

	if (r)
		STEAM_REF++;

	MTY_GlobalUnlock(&STEAM_LOCK);

	return r;
}
