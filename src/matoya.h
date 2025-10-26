// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

/// @file matoya.h
//- #fsupport Windows macOS Android Linux Web

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#if !defined(MTY_EXPORT)
	#define MTY_EXPORT
#endif

#if defined(__GNUC__)
	#define MTY_FMT(a, b) __attribute__((format(printf, a, b)))
#else
	#define MTY_FMT(a, b)
#endif

#ifdef __cplusplus
extern "C" {
#endif


//- #module App
//- #mbrief Application, window, and input management.
//- #mdetails Use these function to create a "libmatoya app", which handles window
//-   creation and input event handling via an MTY_EventFunc. This module wrangles
//-   many different dependencies under the hood responsible for input handling,
//-   graphics API context creation, window creation, and event loop processing.

#define MTY_WINDOW_MAX 8  ///< Maximum number of windows that can be created.
#define MTY_SCREEN_MAX 32 ///< Maximum size of a screen identifier.

typedef struct MTY_App MTY_App;
typedef int8_t MTY_Window;

/// @brief Function called in a loop by MTY_RunAndYield until it returns false.
/// @param opaque Pointer set via MTY_RunAndYield.
/// @returns Return true to continue iterating, false to break.
typedef bool (*MTY_IterFunc)(void *opaque);

/// @brief Function called once per message cycle.
/// @details A "message cycle" can be thought of as one iteration through all of the
///   available messages that the OS has accumulated. libmatoya loops through each
///   OS level message, fires your MTY_EventFunc for each message of interest, then
///   calls this function. By default, this process will repeat with no delay, but
///   one can be added by blocking on MTY_WindowPresent or using MTY_AppSetTimeout.
/// @param opaque Pointer set via MTY_AppCreate.
/// @returns Return true to continue running the app, false to stop it.
typedef bool (*MTY_AppFunc)(void *opaque);

/// @brief Function called to test if a tray menu item should be checked.
/// @param opaque Pointer set via MTY_AppCreate.
/// @returns Return true to show the menu item as checked, false to show it as unchecked.
typedef bool (*MTY_MenuItemCheckedFunc)(void *opaque);

/// @brief Function called to add custom processing to window messages.
/// @details This function is called before any internal processing, and if `shouldReturn` is set
///   to true, no internal handling of the message will take place. This may affect internal state
///   tracking for certain message types.
/// @param app The MTY_App.
/// @param window The MTY_Window associated with the message.
/// @param hwnd `HWND hwnd` value passed as the first argument to the `WNDPROC` callback.
/// @param msg `UINT uMsg` value passed as the second argument to the `WNDPROC` callback.
/// @param wparam `WPARAM wParam` value passed as the third argument to the `WNDPROC` callback.
/// @param lparam `LPARAM lParam` value passed as the fourth argument to the `WNDPROC` callback.
/// @param shouldReturn If set to true, no internal processing of this message will take place
///   and the return value from this callback will be returned by the internal `WNDPROC` callback.
/// @param opaque Pointer set via MTY_AppCreate.
//- #support Windows
typedef intptr_t (*MTY_WMsgFunc)(MTY_App *app, MTY_Window window, void *hwnd, uint32_t msg,
	intptr_t wparam, uintptr_t lparam, bool *shouldReturn, void *opaque);

/// @brief 3D graphics APIs.
typedef enum {
	MTY_GFX_NONE    = 0, ///< No 3D graphics API.
	MTY_GFX_GL      = 1, ///< OpenGL/GLES. Linux, Android, and Web only.
	MTY_GFX_VK      = 2, ///< Vulkan. Not available on Apple OS's.
	MTY_GFX_D3D11   = 3, ///< Direct3D 11. Windows only.
	MTY_GFX_D3D12   = 4, ///< Direct3D 12. Windows only.
	MTY_GFX_METAL   = 5, ///< Metal. Apple only.
	MTY_GFX_MAX     = 6, ///< Maximum number of 3D graphics APIs.
	MTY_GFX_MAKE_32 = INT32_MAX,
} MTY_GFX;

/// @brief Raw image color formats.
typedef enum {
	MTY_COLOR_FORMAT_UNKNOWN    = 0,  ///< Unknown color format.
	MTY_COLOR_FORMAT_BGRA       = 1,  ///< 32-bit BGRA, 8 bits per channel.
	MTY_COLOR_FORMAT_RGBA       = 2,  ///< 32-bit RGBA, 8 bits per channel.
	MTY_COLOR_FORMAT_BGR565     = 3,  ///< 16-bit BGR, 5 bits B, 6 bits G, 5 bits R.
	MTY_COLOR_FORMAT_BGRA5551   = 4,  ///< 16-bit BGRA, 5 bits per BGR, 1 bit A.
	MTY_COLOR_FORMAT_AYUV       = 5,  ///< 32-bit 4:4:4 AYUV, 8 bits per channel.
	MTY_COLOR_FORMAT_Y410       = 6,  ///< 32-bit 4:4:4 YUVA, 10 bits per YUV, 2 bits A.
	MTY_COLOR_FORMAT_Y416       = 7,  ///< 64-bit 4:4:4 YUVA, 16 bits per channel.
	MTY_COLOR_FORMAT_2PLANES    = 8,  ///< 1 plane Y, 1 plane interleaved UV, 8 bits per channel.
	MTY_COLOR_FORMAT_3PLANES    = 9,  ///< 3 consecutive planes of YUV, 8 bits per channel.
	MTY_COLOR_FORMAT_2PLANES_16 = 10, ///< 1 plane Y, 1 plane interleaved UV, 16 bits per channel.
	MTY_COLOR_FORMAT_3PLANES_16 = 11, ///< 3 consecutive planes of YUV, 16 bits per channel.
	MTY_COLOR_FORMAT_MAX        = 12, ///< Maximum number of color formats.
	MTY_COLOR_FORMAT_MAKE_32    = INT32_MAX,
} MTY_ColorFormat;

/// @brief Quad texture filtering.
typedef enum {
	MTY_FILTER_NEAREST = 0, ///< Nearest neighbor filter, can cause shimmering.
	MTY_FILTER_LINEAR  = 1, ///< Bilinear interpolation filter, can cause noticeable blurring.
	MTY_FILTER_MAKE_32 = INT32_MAX,
} MTY_Filter;

/// @brief Quad texture effects.
typedef enum {
	MTY_EFFECT_NONE      = 0, ///< No effect applied.
	MTY_EFFECT_SCANLINES = 1, ///< A scanline effect simulating a 480i CRT television.
	MTY_EFFECT_SHARPEN   = 2, ///< Sharpens bilinear filter.
	MTY_EFFECT_MAKE_32   = INT32_MAX,
} MTY_Effect;

/// @brief Quad rotation.
typedef enum {
	MTY_ROTATION_NONE    = 0, ///< No rotation.
	MTY_ROTATION_90      = 1, ///< Rotate 90 degrees.
	MTY_ROTATION_180     = 2, ///< Rotate 180 degrees.
	MTY_ROTATION_270     = 3, ///< Rotate 270 degrees.
	MTY_ROTATION_MAKE_32 = INT32_MAX,
} MTY_Rotation;

/// @brief Chroma subsampling for planar color formats.
typedef enum {
	MTY_CHROMA_444 = 0, ///< Full width, full height UV.
	MTY_CHROMA_422 = 1, ///< Half width, full height UV.
	MTY_CHROMA_420 = 2, ///< Half width, half height UV.
	MTY_CHROMA_MAKE_32 = INT32_MAX,
} MTY_Chroma;

/// @brief Alternative MTY_App behaviors.
typedef enum {
	MTY_APP_FLAG_HID_EVENTS     = 0x01, ///< If enabled, all controllers except XInput controllers
	                                    ///<   will generate input report events.
	MTY_APP_FLAG_HID_KEYBOARD   = 0x02, ///< On macOS, use the IOKit HID library to make detecting
	                                    ///<   key events more robust. Doing so will request highly
                                        ///<   privileged permissions for the calling application.
	MTY_APP_FLAG_ALT_FULLSCREEN = 0x04, ///< On macOS, make fullscreen mode feel more like
	                                    ///<   borderless windowed mode on Windows.
	MTY_APP_FLAG_MAKE_32        = INT32_MAX,
} MTY_AppFlag;

/// @brief App events.
/// @details See MTY_Event for details on how to respond to these values.
typedef enum {
	MTY_EVENT_NONE           = 0,  ///< No event.
	MTY_EVENT_CLOSE          = 1,  ///< Window close request.
	MTY_EVENT_QUIT           = 2,  ///< Application quit request.
	MTY_EVENT_SHUTDOWN       = 3,  ///< System is shutting down.
	MTY_EVENT_FOCUS          = 4,  ///< Window has gained/lost focus.
	MTY_EVENT_KEY            = 5,  ///< Key pressed/released.
	MTY_EVENT_HOTKEY         = 6,  ///< Hotkey pressed.
	MTY_EVENT_TEXT           = 7,  ///< Visible text input has occurred.
	MTY_EVENT_SCROLL         = 8,  ///< Scroll from a mouse wheel or touch.
	MTY_EVENT_BUTTON         = 9,  ///< Mouse button pressed/released.
	MTY_EVENT_MOTION         = 10, ///< Mouse or touch motion has occurred.
	MTY_EVENT_CONTROLLER     = 11, ///< Game controller state has been updated.
	MTY_EVENT_CONNECT        = 12, ///< Game controller has been connected.
	MTY_EVENT_DISCONNECT     = 13, ///< Game controller has been disconnected.
	MTY_EVENT_PEN            = 14, ///< Drawing tablet pen input.
	MTY_EVENT_DROP           = 15, ///< File drag and drop event.
	MTY_EVENT_CLIPBOARD      = 16, ///< Clipboard has been updated.
	MTY_EVENT_TRAY           = 17, ///< Tray menu item has been selected.
	MTY_EVENT_REOPEN         = 18, ///< The application has been relaunched with a new argument.
	MTY_EVENT_BACK           = 19, ///< The mobile back command has been triggered.
	MTY_EVENT_SIZE           = 20, ///< The size of a window has changed.
	MTY_EVENT_MOVE           = 21, ///< The window's top left corner has moved.
	MTY_EVENT_WEBVIEW_READY  = 22, ///< WebView has loaded and is listening for text. This will
	                               ///<   also fire if the WebView is reloaded.
	MTY_EVENT_WEBVIEW_TEXT   = 23, ///< Text sent from the WebView's JavaScript environment.
	MTY_EVENT_WEBVIEW_KEY    = 24, ///< Key event from the WebView if input passthrough is enabled.
	MTY_EVENT_WEBVIEW_HOTKEY = 25, ///< Hotkey event from the WebView if input passthrough is
	                               ///<   enabled.
	MTY_EVENT_HID            = 26, ///< An HID input report generated by certain controllers.
	MTY_EVENT_MAKE_32        = INT32_MAX,
} MTY_EventType;

/// @brief Keyboard keys.
/// @details These values are based off of the Windows scancode values OR'd with `0x100`
///   for extended keys. These values are not affected by the current locale.
typedef enum {
	MTY_KEY_NONE           = 0x000, ///< No key has been pressed.
	MTY_KEY_ESCAPE         = 0x001, ///< Escape
	MTY_KEY_1              = 0x002, ///< 1
	MTY_KEY_2              = 0x003, ///< 2
	MTY_KEY_3              = 0x004, ///< 3
	MTY_KEY_4              = 0x005, ///< 4
	MTY_KEY_5              = 0x006, ///< 5
	MTY_KEY_6              = 0x007, ///< 6
	MTY_KEY_7              = 0x008, ///< 7
	MTY_KEY_8              = 0x009, ///< 8
	MTY_KEY_9              = 0x00A, ///< 9
	MTY_KEY_0              = 0x00B, ///< 0
	MTY_KEY_MINUS          = 0x00C, ///< -
	MTY_KEY_EQUALS         = 0x00D, ///< =
	MTY_KEY_BACKSPACE      = 0x00E, ///< Backspace
	MTY_KEY_TAB            = 0x00F, ///< Tab
	MTY_KEY_Q              = 0x010, ///< Q
	MTY_KEY_AUDIO_PREV     = 0x110, ///< Audio Previous Track
	MTY_KEY_W              = 0x011, ///< W
	MTY_KEY_E              = 0x012, ///< E
	MTY_KEY_R              = 0x013, ///< R
	MTY_KEY_T              = 0x014, ///< T
	MTY_KEY_Y              = 0x015, ///< Y
	MTY_KEY_U              = 0x016, ///< U
	MTY_KEY_I              = 0x017, ///< I
	MTY_KEY_O              = 0x018, ///< O
	MTY_KEY_P              = 0x019, ///< P
	MTY_KEY_AUDIO_NEXT     = 0x119, ///< Audio Next Track
	MTY_KEY_LBRACKET       = 0x01A, ///< [
	MTY_KEY_RBRACKET       = 0x01B, ///< ]
	MTY_KEY_ENTER          = 0x01C, ///< Enter
	MTY_KEY_NP_ENTER       = 0x11C, ///< Enter (numpad)
	MTY_KEY_LCTRL          = 0x01D, ///< Left Ctrl
	MTY_KEY_RCTRL          = 0x11D, ///< Right Ctrl
	MTY_KEY_A              = 0x01E, ///< A
	MTY_KEY_S              = 0x01F, ///< S
	MTY_KEY_D              = 0x020, ///< D
	MTY_KEY_MUTE           = 0x120, ///< Mute
	MTY_KEY_F              = 0x021, ///< F
	MTY_KEY_G              = 0x022, ///< G
	MTY_KEY_AUDIO_PLAY     = 0x122, ///< Audio Play
	MTY_KEY_H              = 0x023, ///< H
	MTY_KEY_J              = 0x024, ///< J
	MTY_KEY_AUDIO_STOP     = 0x124, ///< Audio Stop
	MTY_KEY_K              = 0x025, ///< K
	MTY_KEY_L              = 0x026, ///< L
	MTY_KEY_SEMICOLON      = 0x027, ///< ;
	MTY_KEY_QUOTE          = 0x028, ///< "
	MTY_KEY_GRAVE          = 0x029, ///< `
	MTY_KEY_LSHIFT         = 0x02A, ///< Left Shift
	MTY_KEY_BACKSLASH      = 0x02B, ///< Backslash
	MTY_KEY_Z              = 0x02C, ///< Z
	MTY_KEY_X              = 0x02D, ///< X
	MTY_KEY_C              = 0x02E, ///< C
	MTY_KEY_VOLUME_DOWN    = 0x12E, ///< Volume Down
	MTY_KEY_V              = 0x02F, ///< V
	MTY_KEY_B              = 0x030, ///< B
	MTY_KEY_VOLUME_UP      = 0x130, ///< Volume Up
	MTY_KEY_N              = 0x031, ///< N
	MTY_KEY_M              = 0x032, ///< M
	MTY_KEY_COMMA          = 0x033, ///< ,
	MTY_KEY_PERIOD         = 0x034, ///< .
	MTY_KEY_SLASH          = 0x035, ///< /
	MTY_KEY_NP_DIVIDE      = 0x135, ///< Divide (numpad)
	MTY_KEY_RSHIFT         = 0x036, ///< Right Shift
	MTY_KEY_NP_MULTIPLY    = 0x037, ///< Multiply (numpad)
	MTY_KEY_PRINT_SCREEN   = 0x137, ///< Print Screen
	MTY_KEY_LALT           = 0x038, ///< Left Alt
	MTY_KEY_RALT           = 0x138, ///< Right Alt
	MTY_KEY_SPACE          = 0x039, ///< Space
	MTY_KEY_CAPS           = 0x03A, ///< Caps Lock
	MTY_KEY_F1             = 0x03B, ///< F1
	MTY_KEY_F2             = 0x03C, ///< F2
	MTY_KEY_F3             = 0x03D, ///< F3
	MTY_KEY_F4             = 0x03E, ///< F4
	MTY_KEY_F5             = 0x03F, ///< F5
	MTY_KEY_F6             = 0x040, ///< F6
	MTY_KEY_F7             = 0x041, ///< F7
	MTY_KEY_F8             = 0x042, ///< F8
	MTY_KEY_F9             = 0x043, ///< F9
	MTY_KEY_F10            = 0x044, ///< F10
	MTY_KEY_NUM_LOCK       = 0x145, ///< Num Lock
	MTY_KEY_SCROLL_LOCK    = 0x046, ///< Scroll Lock
	MTY_KEY_PAUSE          = 0x045, ///< Pause/Break
	MTY_KEY_SIGBREAK       = 0x146, ///< Ctrl + Pause/Break (Windows)
	MTY_KEY_NP_7           = 0x047, ///< 7 (numpad)
	MTY_KEY_HOME           = 0x147, ///< Home
	MTY_KEY_NP_8           = 0x048, ///< 8 (numpad)
	MTY_KEY_UP             = 0x148, ///< Up
	MTY_KEY_NP_9           = 0x049, ///< 9 (numpad)
	MTY_KEY_PAGE_UP        = 0x149, ///< Page Up
	MTY_KEY_NP_MINUS       = 0x04A, ///< Minus (numpad)
	MTY_KEY_NP_4           = 0x04B, ///< 4 (numpad)
	MTY_KEY_LEFT           = 0x14B, ///< Left Arrow
	MTY_KEY_NP_5           = 0x04C, ///< 5 (numpad)
	MTY_KEY_NP_6           = 0x04D, ///< 6 (numpad)
	MTY_KEY_RIGHT          = 0x14D, ///< Right Arrow
	MTY_KEY_NP_PLUS        = 0x04E, ///< Plus (numpad)
	MTY_KEY_NP_1           = 0x04F, ///< 1 (numpad)
	MTY_KEY_END            = 0x14F, ///< End
	MTY_KEY_NP_2           = 0x050, ///< 2 (numpad)
	MTY_KEY_DOWN           = 0x150, ///< Down Arrow
	MTY_KEY_NP_3           = 0x051, ///< 3 (numpad)
	MTY_KEY_PAGE_DOWN      = 0x151, ///< Page Down
	MTY_KEY_NP_0           = 0x052, ///< 0 (numpad)
	MTY_KEY_INSERT         = 0x152, ///< Insert
	MTY_KEY_NP_PERIOD      = 0x053, ///< Period (numpad)
	MTY_KEY_DELETE         = 0x153, ///< Delete
	MTY_KEY_INTL_BACKSLASH = 0x056, ///< International Backslash
	MTY_KEY_F11            = 0x057, ///< F11
	MTY_KEY_F12            = 0x058, ///< F12
	MTY_KEY_NP_EQUAL       = 0x059, ///< Equal (numpad)
	MTY_KEY_LWIN           = 0x15B, ///< Left Windows (Meta/Super)
	MTY_KEY_RWIN           = 0x15C, ///< Right Windows (Meta/Super)
	MTY_KEY_APP            = 0x15D, ///< Application Menu
	MTY_KEY_F13            = 0x064, ///< F13
	MTY_KEY_F14            = 0x065, ///< F14
	MTY_KEY_F15            = 0x066, ///< F15
	MTY_KEY_F16            = 0x067, ///< F16
	MTY_KEY_F17            = 0x068, ///< F17
	MTY_KEY_F18            = 0x069, ///< F18
	MTY_KEY_F19            = 0x06A, ///< F19
	MTY_KEY_F20            = 0x06B, ///< F20
	MTY_KEY_F21            = 0x06C, ///< F21
	MTY_KEY_F22            = 0x06D, ///< F22
	MTY_KEY_F23            = 0x06E, ///< F23
	MTY_KEY_MEDIA_SELECT   = 0x16D, ///< Media Select
	MTY_KEY_JP             = 0x070, ///< Katakana/Hiragana
	MTY_KEY_RO             = 0x073, ///< Ro
	MTY_KEY_F24            = 0x076, ///< F24
	MTY_KEY_HENKAN         = 0x079, ///< Henkan
	MTY_KEY_MUHENKAN       = 0x07B, ///< Muhenkan
	MTY_KEY_INTL_COMMA     = 0x07E, ///< JIS Comma
	MTY_KEY_YEN            = 0x07D, ///< Yen
	MTY_KEY_MAX            = 0x200, ///< The maximum possible MTY_Key value.
	MTY_KEY_MAKE_32        = INT32_MAX,
} MTY_Key;

/// @brief Key modifiers.
typedef enum {
	MTY_MOD_NONE    = 0x000, ///< No mods.
	MTY_MOD_LSHIFT  = 0x001, ///< Left Shift
	MTY_MOD_RSHIFT  = 0x002, ///< Right Shift
	MTY_MOD_LCTRL   = 0x004, ///< Left Ctrl
	MTY_MOD_RCTRL   = 0x008, ///< Right Ctrl
	MTY_MOD_LALT    = 0x010, ///< Left Alt
	MTY_MOD_RALT    = 0x020, ///< Right Alt
	MTY_MOD_LWIN    = 0x040, ///< Left Windows (Meta/Super)
	MTY_MOD_RWIN    = 0x080, ///< Right Windows (Meta/Super)
	MTY_MOD_CAPS    = 0x100, ///< Caps Lock toggled on
	MTY_MOD_NUM     = 0x200, ///< Num Lock toggled on
	MTY_MOD_SHIFT   = 0x003, ///< Left or right Shift
	MTY_MOD_CTRL    = 0x00C, ///< Left or right Ctrl
	MTY_MOD_ALT     = 0x030, ///< Left or right Alt
	MTY_MOD_WIN     = 0x0C0, ///< Left or right Windows (Meta/Super)
	MTY_MOD_MAKE_32 = INT32_MAX,
} MTY_Mod;

/// @brief Mouse buttons.
typedef enum {
	MTY_BUTTON_NONE    = 0, ///< No buttons.
	MTY_BUTTON_LEFT    = 1, ///< Left mouse button.
	MTY_BUTTON_RIGHT   = 2, ///< Right mouse button.
	MTY_BUTTON_MIDDLE  = 3, ///< Middle mouse button.
	MTY_BUTTON_X1      = 4, ///< X1 (back) mouse button.
	MTY_BUTTON_X2      = 5, ///< X2 (forward) mouse button.
	MTY_BUTTON_MAKE_32 = INT32_MAX,
} MTY_Button;

/// @brief Controller types.
/// @details MTY_CTYPE_DEFAULT means raw HID on Windows and macOS, Android's `InputEvent`
///   system, `evdev` on Linux, and the browser's Gamepad API.
typedef enum {
	MTY_CTYPE_DEFAULT = 0, ///< The platform's default controller driver.
	MTY_CTYPE_XINPUT  = 1, ///< XInput controller (Xbox compatible).
	MTY_CTYPE_SWITCH  = 2, ///< Nintendo Switch controller.
	MTY_CTYPE_PS4     = 3, ///< Playstation 4 DualShock controller.
	MTY_CTYPE_PS5     = 4, ///< Playstation 5 DualSense controller.
	MTY_CTYPE_STADIA  = 5, ///< Google Stadia controller.
	MTY_CTYPE_XBOX    = 6, ///< Xbox Bluetooth controller.
	MTY_CTYPE_XBOXW   = 7, ///< Xbox wired controller.
	MTY_CTYPE_MAKE_32 = INT32_MAX,
} MTY_CType;

/// @brief Standardized controller buttons.
/// @details libmatoya makes a best effort to remap the buttons of known controllers
///   to these values.
typedef enum {
	MTY_CBUTTON_X              = 0,  ///< X
	MTY_CBUTTON_A              = 1,  ///< A
	MTY_CBUTTON_B              = 2,  ///< B
	MTY_CBUTTON_Y              = 3,  ///< Y
	MTY_CBUTTON_LEFT_SHOULDER  = 4,  ///< Left Shoulder
	MTY_CBUTTON_RIGHT_SHOULDER = 5,  ///< Right Shoulder
	MTY_CBUTTON_LEFT_TRIGGER   = 6,  ///< Left Trigger
	MTY_CBUTTON_RIGHT_TRIGGER  = 7,  ///< Right Trigger
	MTY_CBUTTON_BACK           = 8,  ///< Back
	MTY_CBUTTON_START          = 9,  ///< Start
	MTY_CBUTTON_LEFT_THUMB     = 10, ///< Left Thumb Stick
	MTY_CBUTTON_RIGHT_THUMB    = 11, ///< Right Thumb Stick
	MTY_CBUTTON_GUIDE          = 12, ///< Guide Button
	MTY_CBUTTON_DPAD_UP        = 13, ///< D-pad Up
	MTY_CBUTTON_DPAD_RIGHT     = 14, ///< D-pad Right
	MTY_CBUTTON_DPAD_DOWN      = 15, ///< D-pad Down
	MTY_CBUTTON_DPAD_LEFT      = 16, ///< D-pad Left
	MTY_CBUTTON_TOUCHPAD       = 17, ///< Touchpad Button
	MTY_CBUTTON_CAPTURE        = 18, ///< Capture Button (Stadia)
	MTY_CBUTTON_MAX            = 64, ///< Maximum number of possible buttons.
	MTY_CBUTTON_MAKE_32        = INT32_MAX,
} MTY_CButton;

/// @brief Standardized controller axes.
/// @details libmatoya makes a best effort to remap axes (sticks, triggers)
///   of known controllers to these values.
typedef enum {
	MTY_CAXIS_THUMB_LX  = 0,  ///< Left Thumb Stick X-axis.
	MTY_CAXIS_THUMB_LY  = 1,  ///< Left Thumb Stick Y-axis.
	MTY_CAXIS_THUMB_RX  = 2,  ///< Right Thumb Stick X-axis.
	MTY_CAXIS_THUMB_RY  = 3,  ///< Right Thumb Stick Y-axis.
	MTY_CAXIS_TRIGGER_L = 4,  ///< Left Trigger
	MTY_CAXIS_TRIGGER_R = 5,  ///< Right Trigger
	MTY_CAXIS_MAX       = 16, ///< Maximum number of possible axes.
	MTY_CAXIS_MAKE_32   = INT32_MAX,
} MTY_CAxis;

/// @brief Pen attributes.
typedef enum {
	MTY_PEN_FLAG_LEAVE    = 0x01, ///< Pen has left the drawing surface.
	MTY_PEN_FLAG_TOUCHING = 0x02, ///< Pen is touching the drawing surface.
	MTY_PEN_FLAG_INVERTED = 0x04, ///< The pen is inverted.
	MTY_PEN_FLAG_ERASER   = 0x08, ///< The eraser is touching the drawing surface.
	MTY_PEN_FLAG_MAKE_32  = INT32_MAX,
} MTY_PenFlag;

/// @brief Window keyboard/mouse detach states.
typedef enum {
	MTY_DETACH_STATE_NONE    = 0, ///< Input is not detached.
	MTY_DETACH_STATE_GRAB    = 1, ///< Temporarily ignore mouse and keyboard grabbing.
	MTY_DETACH_STATE_FULL    = 2, ///< Temporarily ignore grabbing and relative mouse mode.
	MTY_DETACH_STATE_MAKE_32 = INT32_MAX,
} MTY_DetachState;

/// @brief Mobile device orientations.
typedef enum {
	MTY_ORIENTATION_USER      = 0, ///< No orientation specified.
	MTY_ORIENTATION_LANDSCAPE = 1, ///< Horizontal landscape orientation.
	MTY_ORIENTATION_PORTRAIT  = 2, ///< Vertical portrait orientation.
	MTY_ORIENTATION_MAKE_32   = INT32_MAX,
} MTY_Orientation;

/// @brief Scope of an operation.
typedef enum {
	MTY_SCOPE_LOCAL   = 0, ///< Operation affecting only the current application.
	MTY_SCOPE_GLOBAL  = 1, ///< Operation globally affecting the OS.
	MTY_SCOPE_MAKE_32 = INT32_MAX,
} MTY_Scope;

/// @brief Mobile input modes.
typedef enum {
	MTY_INPUT_MODE_UNSPECIFIED = 0, ///< No input mode specified.
	MTY_INPUT_MODE_TOUCHSCREEN = 1, ///< Report input events as if the user is touching the screen.
	MTY_INPUT_MODE_TRACKPAD    = 2, ///< Report input events as if the user is using a trackpad.
	MTY_INPUT_MODE_MAKE_32     = INT32_MAX,
} MTY_InputMode;

/// @brief State of a window's graphics context.
/// @details On most platforms, changes to the context state are rare but are possible
///   on extreme events like the GPU getting removed. On Android the context can be
///   recreated frequently when the app moves in and out of focus.
typedef enum {
	MTY_CONTEXT_STATE_NORMAL  = 0, ///< The graphics context is operating normally.
	MTY_CONTEXT_STATE_REFRESH = 1, ///< The surface has been altered and needs to be presented.
	MTY_CONTEXT_STATE_NEW     = 2, ///< The graphics context has been completely recreated and has
	                               ///<   lost any previously loaded textures and state.
	MTY_CONTEXT_STATE_MAKE_32 = INT32_MAX,
} MTY_ContextState;

/// @brief Window modes and behaviors.
typedef enum {
	MTY_WINDOW_NORMAL     = 0x0, ///< Normal resizable, bordered window.
	MTY_WINDOW_HIDDEN     = 0x1, ///< Window is hidden without any visible elements.
	MTY_WINDOW_FULLSCREEN = 0x2, ///< Window is in fullscreen mode.
	MTY_WINDOW_MAXIMIZED  = 0x4, ///< Window is maximized (zoomed).
	MTY_WINDOW_MAKE_32    = INT32_MAX,
} MTY_WindowType;

/// @brief Predefined cursors set via MTY_AppSetCursor.
typedef enum {
	MTY_CURSOR_NONE    = 0, ///< Revert the effects of MTY_AppSetCursor.
	MTY_CURSOR_ARROW   = 1, ///< Standard arrow cursor.
	MTY_CURSOR_HAND    = 2, ///< Hand cursor.
	MTY_CURSOR_IBEAM   = 3, ///< I-beam cursor.
	MTY_CURSOR_MAKE_32 = INT32_MAX,
} MTY_Cursor;

/// @brief Description of a render operation.
typedef struct {
	MTY_ColorFormat format; ///< The color format of a raw image.
	MTY_Rotation rotation;  ///< Rotation applied to the image.
	MTY_Chroma chroma;      ///< Color subsampling, chroma layout for planar YUV formats.
	MTY_Filter filter;      ///< Filter applied to the image.
	MTY_Effect effects[2];  ///< Effects applied to the image.
	float levels[2];        ///< Intensity of the applied `effects` between `0.0f` and `1.0f`.
	bool fullRangeYUV;      ///< Use the full 0-255 color range for YUV formats.
	bool multiplyYUV;       ///< Properly normalize 10-bit YUV formats if not already done.
	uint8_t layer;          ///< If drawing multiple layers of quads between present, `layer`
	                        ///<   determines the order in the blending hierarchy.
	uint32_t imageWidth;    ///< The width in pixels of the image.
	uint32_t imageHeight;   ///< The height in pixels of the image.
	uint32_t cropWidth;     ///< Desired crop width of the image from the top left corner.
	uint32_t cropHeight;    ///< Desired crop height of the image from the top left corner.
	uint32_t viewWidth;     ///< The width of the viewport.
	uint32_t viewHeight;    ///< The height of the viewport.
	float aspectRatio;      ///< Desired aspect ratio of the image. The renderer will letterbox
	                        ///<   the image to maintain the specified aspect ratio.
	float scale;            ///< Multiplier applied to the dimensions of the image, producing an
	                        ///<   minimized or magnified image. This can be set to 0
	                        ///<   if unnecessary.
} MTY_RenderDesc;

/// @brief A point with an `x` and `y` coordinate.
typedef struct {
	float x; ///< Horizontal position.
	float y; ///< Vertical position
} MTY_Point;

/// @brief A rectangle with `left`, `top`, `right`, and `bottom` coordinates.
typedef struct {
	float left;   ///< Left edge position.
	float top;    ///< Top edge position.
	float right;  ///< Right edge position.
	float bottom; ///< Bottom edge position.
} MTY_Rect;

/// @brief UI vertex.
typedef struct {
	MTY_Point pos; ///< Vertex position.
	MTY_Point uv;  ///< Vertex texcoord.
	uint32_t col;  ///< Element color.
} MTY_Vtx;

/// @brief UI draw command.
typedef struct {
	MTY_Rect clip;      ///< Clip rectangle.
	uint32_t texture;   ///< Texture reference.
	uint32_t elemCount; ///< Number of indices.
	uint32_t idxOffset; ///< Index buffer offset.
	uint32_t vtxOffset; ///< Vertex buffer offset.
} MTY_Cmd;

/// @brief UI draw command list.
typedef struct {
	MTY_Cmd *cmd;       ///< List of commands.
	MTY_Vtx *vtx;       ///< Vertex buffer data.
	uint16_t *idx;      ///< Index buffer data.
	uint32_t cmdLength; ///< Number of commands.
	uint32_t cmdMax;    ///< Size of the `cmd` buffer, used internally for deduping.
	uint32_t vtxLength; ///< Total number of vertices.
	uint32_t vtxMax;    ///< Size of the `vtx` buffer, used internally for deduping.
	uint32_t idxLength; ///< Total number of indices.
	uint32_t idxMax;    ///< Size of the `idx` buffer, used internally for deduping.
} MTY_CmdList;

/// @brief UI draw data.
typedef struct {
	MTY_Point displaySize;   ///< Size of the viewport.
	MTY_CmdList *cmdList;    ///< Command lists.
	uint32_t cmdListLength;  ///< Number of command lists.
	uint32_t cmdListMax;     ///< Size of `cmdList`, used internally for deduping.
	uint32_t idxTotalLength; ///< Total number of indices in all command lists.
	uint32_t vtxTotalLength; ///< Total number of vertices in all command lists.
	bool clear;              ///< Surface should be cleared before drawing.
} MTY_DrawData;

/// @brief Key event.
typedef struct {
	MTY_Key key;   ///< The key that has been pressed or released.
	MTY_Mod mod;   ///< Modifiers in effect.
	uint32_t vkey; ///< The OS specific virtual code. Windows, macOS, and Linux only.
	bool pressed;  ///< State of the key.
} MTY_KeyEvent;

/// @brief Scroll event.
typedef struct {
	int32_t x;   ///< Horizontal scroll value.
	int32_t y;   ///< Vertical scroll value.
	bool pixels; ///< The scroll values are expressed in exact pixels.
} MTY_ScrollEvent;

/// @brief Button event.
typedef struct {
	MTY_Button button; ///< The button that has been pressed or released.
	int32_t x;         ///< Horizontal position in the client area window.
	int32_t y;         ///< Vertical position in the client area of the window.
	bool pressed;      ///< State of the button.
} MTY_ButtonEvent;

/// @brief Motion event.
typedef struct {
	int32_t x;     ///< If `relative` is true, the horizontal delta since the previous motion
	               ///<   event, otherwise the horizontal position in the window's client area.
	int32_t y;     ///< In `relative` is true, the vertical delta since the previous motion event,
	               ///<   otherwise the vertical position in the window's client area.
	bool relative; ///< The event is a relative motion event.
	bool synth;    ///< The event was synthesized by libmatoya.
} MTY_MotionEvent;

/// @brief File drop event.
typedef struct {
	const char *name; ///< The name of file.
	const void *buf;  ///< A buffer containing the contents of the file.
	size_t size;      ///< The size of `buf`.
} MTY_DropEvent;

/// @brief Controller axis.
typedef struct {
	uint16_t usage; ///< The mapped HID usage ID.
	int16_t value;  ///< Axis value.
	int16_t min;    ///< Axis logical minimum.
	int16_t max;    ///< Axis logical maximum.
} MTY_Axis;

/// @brief Controller state event.
typedef struct {
	bool buttons[MTY_CBUTTON_MAX]; ///< Controller buttons pressed.
	MTY_Axis axes[MTY_CAXIS_MAX];  ///< Controller axes.
	MTY_CType type;                ///< Game controller type.
	uint32_t id;                   ///< Assigned controller `id`.
	uint16_t vid;                  ///< HID Vendor ID.
	uint16_t pid;                  ///< HID Product ID.
	uint8_t numButtons;            ///< Number of buttons in `buttons`.
	uint8_t numAxes;               ///< Number of axes in `axes`.
} MTY_ControllerEvent;

/// @brief HID input report from certain controllers.
/// @details If enabled via MTY_APP_FLAG_HID_EVENTS, all controllers except XInput
///   controllers will generate input report events.
typedef struct {
	const void *report; ///< The HID input report.
	size_t size;        ///< Size in bytes of `report`.
	MTY_CType type;     ///< Game controller type.
	uint32_t id;        ///< Assigned controller `id`.
	uint16_t vid;       ///< HID Vendor ID.
	uint16_t pid;       ///< HID Product ID.
} MTY_HIDEvent;

/// @brief Pen event.
typedef struct {
	MTY_PenFlag flags; ///< Pen attributes.
	uint16_t x;        ///< The horizontal position in the client area of the window.
	uint16_t y;        ///< The vertical position in the client area of the window.
	uint16_t pressure; ///< Pressure on the drawing surface between 0 and 1024.
	uint16_t rotation; ///< Rotation of the pen between 0 and 359.
	int8_t tiltX;      ///< Horizontal tilt of the pen between -90 and 90.
	int8_t tiltY;      ///< Vertical tilt of the pen between -90 and 90.
} MTY_PenEvent;

/// @brief App event encapsulating all event types.
/// @details First inspect the `type` member to determine what kind of event it is,
///   then choose one of the members from the nameless union.
typedef struct MTY_Event {
	MTY_EventType type; ///< The type of event determining which member in the union is valid.
	MTY_Window window;  ///< The window associated with the event.

	union {
		MTY_ControllerEvent controller; ///< Valid on MTY_EVENT_CONTROLLER, MTY_EVENT_CONNECT, and
		                                ///<   MTY_EVENT_DISCONNECT.
		MTY_ScrollEvent scroll;         ///< Valid on MTY_EVENT_SCROLL.
		MTY_ButtonEvent button;         ///< Valid on MTY_EVENT_BUTTON.
		MTY_MotionEvent motion;         ///< Valid on MTY_EVENT_MOTION.
		MTY_DropEvent drop;             ///< Valid on MTY_EVENT_DROP.
		MTY_PenEvent pen;               ///< Valid on MTY_EVENT_PEN.
		MTY_KeyEvent key;               ///< Valid on MTY_EVENT_KEY.
		MTY_HIDEvent hid;               ///< Valid on MTY_EVENT_HID.

		const char *reopenArg;   ///< Valid on MTY_EVENT_REOPEN, the argument supplied.
		const char *webviewText; ///< Valid on MTY_EVENT_WEBVIEW_TEXT, text sent from the
		                         ///<   WebView's JavaScript enitronment.
		uint32_t hotkey;         ///< Valid on MTY_EVENT_HOTKEY, the `id` set via MTY_AppSetHotkey.
		uint32_t trayID;         ///< Valid on MTY_EVENT_TRAY, the menu `id` set via MTY_AppSetTray.
		char text[8];            ///< Valid on MTY_EVENT_TEXT, the UTF-8 text.
		bool focus;              ///< Valid on MTY_EVENT_FOCUS, the focus state.
	};
} MTY_Event;

/// @brief Menu item on a tray's menu.
typedef struct {
	MTY_MenuItemCheckedFunc checked; ///< Function called when the menu is opened to determine
	                                 ///<   if the menu item should be checked.
	const char *label;               ///< The menu item's label.
	uint32_t trayID;                 ///< The `trayID` sent with an MTY_Event when the menu item
	                                 ///<   is selected.
} MTY_MenuItem;

/// @brief Size struct containing width and height.
typedef struct {
	uint32_t w; ///< Width.
	uint32_t h; ///< Height.
} MTY_Size;

/// @brief Window size and position.
typedef struct {
	MTY_WindowType type;         ///< Window type.
	MTY_Size size;               ///< Window client (content) size.
	char screen[MTY_SCREEN_MAX]; ///< Screen identifier.
	int32_t x;                   ///< Window horizontal offset from the left of `screen`.
	int32_t y;                   ///< Window vertical offset from the top of `screen`.
} MTY_Frame;

/// @brief Function called for each event sent to the app.
/// @param evt The MTY_Event received by the app.
/// @param opaque Pointer set via MTY_AppCreate.
typedef void (*MTY_EventFunc)(const MTY_Event *evt, void *opaque);

/// @brief Create an MTY_App that handles input and window creation.
/// @param appFunc Function called once per message cycle.
/// @param eventFunc Function called once per event fired.
/// @param opaque Your application's top level context, passed to a variety of
///   callbacks in the App module.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_App must be destroyed with MTY_AppDestroy.
MTY_EXPORT MTY_App *
MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque);

/// @brief Destroy an MTY_App.
/// @details This function will also destroy all open windows and the system tray
///   icon if present.
/// @param app Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_AppDestroy(MTY_App **app);

/// @brief Run the app and begin executing its MTY_AppFunc and MTY_EventFunc.
/// @details This function will block until your MTY_AppFunc returns false. It should
///   be run on the main thread. Internally, it polls for messages from the OS,
///   dispatches events to your MTY_EventFunc, and performs internal state management
///   for the MTY_App object as a whole.
/// @param ctx The MTY_App.
MTY_EXPORT void
MTY_AppRun(MTY_App *ctx);

/// @brief Set the frequency of an app's MTY_AppFunc if it is not blocked by
///   other means.
/// @param ctx The MTY_App.
/// @param timeout Time to wait in milliseconds between MTY_AppFunc calls.
MTY_EXPORT void
MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout);

/// @brief Check if an app is currently focused and in the foreground.
/// @param ctx The MTY_App.
MTY_EXPORT bool
MTY_AppIsActive(MTY_App *ctx);

/// @brief Activate or deactivate the app.
/// @details Activating the app brings its main window to the foreground and focuses
///   it, deactivating the app hides all windows.
/// @param ctx The MTY_App.
/// @param active Set true to activate the app, false to deactivate it.
//- #support Windows macOS Linux
MTY_EXPORT void
MTY_AppActivate(MTY_App *ctx, bool active);

/// @brief Set a system tray icon for the app.
/// @param ctx The MTY_App.
/// @param tooltip Text that appears when the tray icon is hovered.
/// @param items Menu items that appear when the tray icon is activated.
/// @param len Number of elements of `items`.
//- #support Windows
MTY_EXPORT void
MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items,
	uint32_t len);

/// @brief Remove the tray icon associated with the app.
/// @details The tray is automatically destroyed as part of MTY_AppDestroy.
/// @param ctx The MTY_App.
//- #support Windows
MTY_EXPORT void
MTY_AppRemoveTray(MTY_App *ctx);

/// @brief Send a system-wide notification (toast).
/// @details A tray icon must be set via MTY_AppSetTray for notifications to work.
/// @param ctx The MTY_App.
/// @param title Title of the notification.
/// @param msg Body of the notification.
//- #support Windows
MTY_EXPORT void
MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg);

/// @brief Get the current text content of the clipboard.
/// @param ctx The MTY_App.
/// @returns If no clipboard data is available, NULL is returned.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_AppGetClipboard(MTY_App *ctx);

/// @brief Set the current text content of the clipboard.
/// @param ctx The MTY_App.
/// @param text String to set.
MTY_EXPORT void
MTY_AppSetClipboard(MTY_App *ctx, const char *text);

/// @brief Keep the system from going to sleep.
/// @details The app defaults to allow sleep.
/// @param ctx The MTY_App.
/// @param enable Set true to stay awake, false to allow sleep.
MTY_EXPORT void
MTY_AppStayAwake(MTY_App *ctx, bool enable);

/// @brief Get the app's current detach state.
/// @param ctx The MTY_App.
MTY_EXPORT MTY_DetachState
MTY_AppGetDetachState(MTY_App *ctx);

/// @brief Detach mouse/keyboard grab or relative mouse behavior temporarily.
/// @details The app defaults to MTY_DETACH_STATE_NONE.
/// @param ctx The MTY_App.
/// @param state Detach state.
MTY_EXPORT void
MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state);

/// @brief Check if the mouse is grabbed.
/// @param ctx The MTY_App.
MTY_EXPORT bool
MTY_AppIsMouseGrabbed(MTY_App *ctx);

/// @brief Grab the mouse, preventing the cursor from exiting the current window.
/// @details The app defaults to being ungrabbed.
/// @param ctx The MTY_App.
/// @param grab Set true to grab, false to ungrab.
MTY_EXPORT void
MTY_AppGrabMouse(MTY_App *ctx, bool grab);

/// @brief Check if relative mouse mode is set.
/// @param ctx The MTY_App.
MTY_EXPORT bool
MTY_AppGetRelativeMouse(MTY_App *ctx);

/// @brief Set relative mouse mode on or off.
/// @details Relative mouse mode causes the cursor to lock in place and disappear,
///   then subsequently reports relative coordinates via MTY_EVENT_MOTION.
/// @param ctx The MTY_App.
/// @param relative Set true to enter relative mode, false to exit relative mode.
MTY_EXPORT void
MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative);

/// @brief Set the app's cursor to an RGBA image.
/// @param ctx The MTY_App.
/// @param image A buffer holding 8-bits per channel RGBA.
/// @param width Width of `image`.
/// @param height Height of `image`.
/// @param hotX The cursor's horizontal hotspot position.
/// @param hotY The cursor's vertical hotspot position.
MTY_EXPORT void
MTY_AppSetRGBACursor(MTY_App *ctx, const void *image, uint32_t width, uint32_t height,
	uint32_t hotX, uint32_t hotY);

/// @brief Set the app's cursor to a monochrome image.
/// @param ctx The MTY_App.
/// @param image A buffer containing a two plane monochrome image, the first plane contains
///   a 1 bits per pixel AND mask, the second contains a 1 bits per pixel XOR mask.
/// @param width Width in pixels of `image`.
/// @param height Height in pixels of `image`.
/// @param hotX The cursor's horizontal hotspot position.
/// @param hotY The cursor's vertical hotspot position.
void MTY_AppSetMonochromeCursor(MTY_App *ctx, const void *image, uint32_t width, uint32_t height,
	uint32_t hotX, uint32_t hotY);

/// @brief Set the app's cursor to a PNG image.
/// @param ctx The MTY_App.
/// @param image A buffer holding a compressed PNG image.
/// @param size Size in bytes of `image`.
/// @param hotX The cursor's horizontal hotspot position.
/// @param hotY The cursor's vertical hotspot position.
MTY_EXPORT void
MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY);

/// @brief Use a cursor predefined by the OS.
/// @details The cursor set via this function will take precedence over any set with
///   MTY_AppSetPNGCursor.
/// @param ctx The MTY_App.
/// @param cursor The predefined cursor. Set MTY_CURSOR_NONE to revert the effects
///   of this function.
MTY_EXPORT void
MTY_AppSetCursor(MTY_App *ctx, MTY_Cursor cursor);

/// @brief Show or hide the cursor.
/// @param ctx The MTY_App.
/// @param show Set true to show the cursor, false to hide it.
MTY_EXPORT void
MTY_AppShowCursor(MTY_App *ctx, bool show);

/// @brief Check if the app is able to warp the cursor.
/// @param ctx The MTY_App.
MTY_EXPORT bool
MTY_AppCanWarpCursor(MTY_App *ctx);

/// @brief Check if the keyboard is currently grabbed.
/// @param ctx The MTY_App.
MTY_EXPORT bool
MTY_AppIsKeyboardGrabbed(MTY_App *ctx);

/// @brief Grab the keyboard, capturing certain keys usually handled by the OS.
/// @details The app defaults being ungrabbed. This function will attempt to route all
///   special OS hotkeys to the app instead, i.e. ATL+TAB on Windows. On macOS,
///   MTY_APP_FLAG_HID_KEYBOARD must be set during MTY_AppCreate for this function to succeed.
/// @param ctx The MTY_App.
/// @param grab Set true to grab, false to ungrab.
/// @returns The resulting keyboard grab state of the app.
//- #support Windows macOS
MTY_EXPORT bool
MTY_AppGrabKeyboard(MTY_App *ctx, bool grab);

/// @brief Get a previously set hotkey's `id`.
/// @param ctx The MTY_App.
/// @param scope Global or local hotkey.
/// @param mod Hotkey modifier.
/// @param key Hotkey key.
MTY_EXPORT uint32_t
MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key);

/// @brief Set a hotkey combination.
/// @details Globally scoped hotkeys are only available on Windows.
/// @param ctx The MTY_App.
/// @param scope Global or local scope.
/// @param mod Hotkey modifier.
/// @param key Hotkey key.
/// @param id An `id` associated with the hotkey that comes back when the key
///   combination is pressed via an MTY_EVENT_HOTKEY.
MTY_EXPORT void
MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id);

/// @brief Remove all hotkeys.
/// @param ctx The MTY_App.
/// @param scope Global or local scope.
MTY_EXPORT void
MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope);

/// @brief Temporarily enable or disable the currently set globally scoped hotkeys.
/// @details The app defaults to having global hotkeys enabled.
/// @param ctx The MTY_App.
/// @param enable Set true to enable global hotkeys, false to disable.
//- #support Windows
MTY_EXPORT void
MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable);

/// @brief Check if the software keyboard is showing.
/// @param ctx The MTY_App.
//- #support Android
MTY_EXPORT bool
MTY_AppIsSoftKeyboardShowing(MTY_App *ctx);

/// @brief Show or hide the software keyboard.
/// @param ctx The MTY_App.
/// @param show Set true to show the software keyboard, false to hide it.
//- #support Android
MTY_EXPORT void
MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show);

/// @brief Get the device's orientation.
/// @param ctx The MTY_App.
//- #support Android
MTY_EXPORT MTY_Orientation
MTY_AppGetOrientation(MTY_App *ctx);

/// @brief Set the devices orientation.
/// @details The app defaults to MTY_ORIENTATION_USER.
/// @param ctx The MTY_App.
/// @param orientation Requested orientation.
//- #support Android
MTY_EXPORT void
MTY_AppSetOrientation(MTY_App *ctx, MTY_Orientation orientation);

/// @brief Set the rumble state on a controller.
/// @details Rumble on controllers is generally stateful and must be set to 0
///   to stop rumbling. Some controllers will automatically stop rumbling if it has
///   not been refreshed after a short period of time.
/// @param ctx The MTY_App.
/// @param id A controller `id` found via MTY_EVENT_CONTROLLER or MTY_EVENT_CONNECT.
/// @param low Strength of low frequency motor between 0 and UINT16_MAX.
/// @param high Strength of the high frequency motor between 0 and UINT16_MAX.
MTY_EXPORT void
MTY_AppRumbleController(MTY_App *ctx, uint32_t id, uint16_t low, uint16_t high);

/// @brief Get the device name of a controller.
/// @param ctx The MTY_App.
/// @param id A controller `id` found via MTY_EVENT_CONTROLLER or MTY_EVENT_CONNECT.
/// @returns The device name of the controller on success, NULL if the controller does
///   not exist or the controller's device name is not available.
//- #support Windows
MTY_EXPORT const char *
MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id);

/// @brief Get the type of controller.
/// @param ctx The MTY_App.
/// @param id A controller `id` found via MTY_EVENT_CONTROLLER or MTY_EVENT_CONNECT.
/// @returns The type of controller on success, MTY_CTYPE_DEFAULT if the controller does
///   not exist or the controller type is not available.
//- #support Windows macOS
MTY_EXPORT MTY_CType
MTY_AppGetControllerType(MTY_App *ctx, uint32_t id);

/// @brief Submit an HID output report to a controller.
/// @details Be careful to make sure the report is compatible with the device!
/// @param ctx The MTY_App.
/// @param id A controller `id` found via MTY_EVENT_CONTROLLER or MTY_EVENT_CONNECT.
/// @param report The HID output report.
/// @param size The size of `report`.
//- #support Windows macOS
MTY_EXPORT void
MTY_AppSubmitHIDReport(MTY_App *ctx, uint32_t id, const void *report, size_t size);

/// @brief Check if pen events are enabled.
/// @param ctx The MTY_App.
//- #support Windows macOS
MTY_EXPORT bool
MTY_AppIsPenEnabled(MTY_App *ctx);

/// @brief Enable or disable pen events.
/// @details The app defaults to having the pen disabled, which means pen events will
///   show up as MTY_EVENT_MOTION and MTY_EVENT_BUTTON events.
/// @param ctx The MTY_App.
/// @param enable Set true to enable the pen, false to disable it.
//- #support Windows macOS
MTY_EXPORT void
MTY_AppEnablePen(MTY_App *ctx, bool enable);

/// @brief Get the app's current mobile input mode.
/// @param ctx The MTY_App.
//- #support Android
MTY_EXPORT MTY_InputMode
MTY_AppGetInputMode(MTY_App *ctx);

/// @brief Set the app's current mobile input mode.
/// @details The app defaults to MTY_INPUT_MODE_UNSPECIFIED.
/// @param ctx The MTY_App.
/// @param mode Mobile touch input mode.
//- #support Android
MTY_EXPORT void
MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode);

/// @brief Set a custom window message handler for the app.
/// @param ctx The MTY_App.
/// @param func Function called internally for each window message. Setting this to NULL will
///   remove any existing handler.
MTY_EXPORT void
MTY_AppSetWMsgFunc(MTY_App *ctx, MTY_WMsgFunc func);

/// @brief Create an MTY_Window, the primary interactive view of an application.
/// @details An MTY_Window is a child of the MTY_App object, so everywhere a window
///   is referenced the MTY_App comes along with it. All functions taking an MTY_Window
///   as an argument are designed to handle invalid windows, which are integers.
/// @param app The MTY_App.
/// @param title The title of the window.
/// @param frame The window's size and position. Use MTY_MakeDefaultFrame or MTY_WindowGetFrame
///   to fill a frame more precisely. This function expects unscaled values. May be NULL
///   for sensible defaults.
/// @param index Attempt to create the window with the specified index. If the index is already
///   taken, the first available is used.
/// @returns On success, a value between 0 and MTY_WINDOW_MAX is returned.\n\n
///   On failure, -1 is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_Window may be destroyed with MTY_WindowDestroy, or destroyed
///   during MTY_AppDestroy which destroys all windows.
MTY_EXPORT MTY_Window
MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index);

/// @brief Destroy an MTY_Window.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT void
MTY_WindowDestroy(MTY_App *app, MTY_Window window);

/// @brief Get a window's current client area width and height.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT MTY_Size
MTY_WindowGetSize(MTY_App *app, MTY_Window window);

/// @brief Get a window's normalized size and position.
/// @details This function can be used to query the "restored" size and position of a window
///   even when it is maximized or in fullscreen mode. It also will always return unscaled values
///   taking into account the screen's current scaling setting. The returned MTY_Frame can be
///   passed directly into MTY_WindowCreate or MTY_WindowSetFrame.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT MTY_Frame
MTY_WindowGetFrame(MTY_App *app, MTY_Window window);

/// @brief Set a window's size and position.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param frame An MTY_Frame containing the window's size and position. This function expects
///   unscaled values.
MTY_EXPORT void
MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame);

/// @brief Set a window's minimum size.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param minWidth The window's minimum possible width.
/// @param minHeight The window's minimum possible height.
MTY_EXPORT void
MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight);

/// @brief Get the width and height of the screen where the window currently resides.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT MTY_Size
MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window);

/// @brief Get the scaling factor of the screen where the window currently resides.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @returns A scaling factor of `1.0f` is the default scaling factor on all platforms.
///   Greater than `1.0f` means the interface should be enlarged, less than `1.0f` means
///   the interface should be reduced.
MTY_EXPORT float
MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window);

/// @brief Get the refresh rate of the display where the window currently resides.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT uint32_t
MTY_WindowGetRefreshRate(MTY_App *app, MTY_Window window);

/// @brief Set the window's title.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param title The new title of the window.
MTY_EXPORT void
MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title);

/// @brief Check if the window is at least partially visible.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WindowIsVisible(MTY_App *app, MTY_Window window);

/// @brief Check if the window is currently in the foreground and focused.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WindowIsActive(MTY_App *app, MTY_Window window);

/// @brief Activate or deactivate a window.
/// @details Activating a window brings it to the foreground and focuses it,
///   deactivating a window hides it. Windows are automatically activated when they
///   are created.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param active Set true to activate, false to deactivate.
//- #support Windows macOS Linux
MTY_EXPORT void
MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active);

/// @brief Check if the window exists.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WindowExists(MTY_App *app, MTY_Window window);

/// @brief Check if the window is in fullscreen mode.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window);

/// @brief Put a window into or out of fullscreen mode.
/// @details libmatoya uses "borderless windowed" mode exclusively and does not
///   support exclusive fullscreen mode.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param fullscreen Set true to put the window into fullscreen, false to take it out.
MTY_EXPORT void
MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen);

/// @brief Move the cursor to a specified location within a window.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param x Horizontal position to move the cursor in the window's client area.
/// @param y Vertical position to move the cursor in the window's client area.
//- #support Windows macOS Linux
MTY_EXPORT void
MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y);

/// @brief Calculate the viewport for a window given an MTY_RenderDesc.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param desc An MTY_RenderDesc that could be passed to MTY_WindowDrawQuad.
/// @param x Set to the location of the left edge of the viewport.
/// @param y Set to the location of the top edge of the viewport.
/// @param w Set to the width of the viewport.
/// @param h Set to the height of the viewport.
void MTY_WindowGetViewport(MTY_App *app, MTY_Window window, const MTY_RenderDesc *desc,
	float *x, float *y, float *w, float *h);

/// @brief Draw a quad with a raw image and MTY_RenderDesc.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param image The raw image.
/// @param desc Description of the raw image and how it should be rendered. The
///   `viewWidth` and `viewHeight` members of this structure are ignored.
MTY_EXPORT void
MTY_WindowDrawQuad(MTY_App *app, MTY_Window window, const void *image,
	const MTY_RenderDesc *desc);

/// @brief Clear the window surface to a solid color.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param r The red color channel value between 0 and 1.
/// @param g The green color channel value between 0 and 1.
/// @param b The blue color channel value between 0 and 1.
/// @param a The alpha color channel value between 0 and 1.
MTY_EXPORT void
MTY_WindowClear(MTY_App *app, MTY_Window window, float r, float g, float b, float a);

/// @brief Draw a UI with MTY_DrawData.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param dd The UI draw data containing a full frame.
MTY_EXPORT void
MTY_WindowDrawUI(MTY_App *app, MTY_Window window, const MTY_DrawData *dd);

/// @brief Check if a texture with `id` has been set.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param id An `id` specified via MTY_WindowSetUITexture.
MTY_EXPORT bool
MTY_WindowHasUITexture(MTY_App *app, MTY_Window window, uint32_t id);

/// @brief Set an RGBA texture image for use in MTY_DrawData.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param id The desired `id` for the texture.
/// @param rgba RGBA 8-bits per channel image.
/// @param width Width of `rgba`.
/// @param height Height of `rgba`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_WindowSetUITexture(MTY_App *app, MTY_Window window, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height);

/// @brief Present all pending draw operations.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT void
MTY_WindowPresent(MTY_App *app, MTY_Window window);

/// @brief Get the current graphics API in use by the window.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT MTY_GFX
MTY_WindowGetGFX(MTY_App *app, MTY_Window window);

/// @brief Set the window's graphics API.
/// @details This function should be called on the thread that is doing the rendering.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param api Graphics API to set.
/// @param vsync Set true to synchronize presentation to the vertical refresh rate
///   of the monitor, false to present as soon as possible even if tearing occurs.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_WindowSetGFX(MTY_App *app, MTY_Window window, MTY_GFX api, bool vsync);

/// @brief Set the number of vertical blanks to wait until presentation occurs.
/// @details This function only works with MTY_GFX_D3D11 and MTY_GFX_D3D12.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param interval The number of vertical blanks after calling MTY_WindowPresent that
///   presentation will occur, multiplied by 100.
//- #support Windows
MTY_EXPORT void
MTY_WindowSetSyncInterval(MTY_App *app, MTY_Window window, uint32_t interval);

/// @brief Get a list of available graphics APIs on the current OS.
/// @param apis Array to receive the list of available graphics APIs. This buffer
///   should be MTY_GFX_MAX elements.
/// @returns The number of graphics APIs set in `apis`.
MTY_EXPORT uint32_t
MTY_GetAvailableGFX(MTY_GFX *apis);

/// @brief Get the default graphics API for the current OS.
MTY_EXPORT MTY_GFX
MTY_GetDefaultGFX(void);

/// @brief Get the graphics context's current state.
/// @details The context state can inform you if your textures need to be reloaded.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT MTY_ContextState
MTY_WindowGetContextState(MTY_App *app, MTY_Window window);

/// @brief Get the OS's native window.
/// @details This function will return an `HWND` on Windows, an `NSWindow *` on
///   macOS, and an `ANativeWindow *` on Android.
/// @param app The MTY_App.
/// @param window An MTY_Window.
//- #support Windows macOS Android
MTY_EXPORT void *
MTY_WindowGetNative(MTY_App *app, MTY_Window window);

/// @brief Set a WebView overlaying a native window.
/// @details This will essentially overlay an entire web browser over the native window. To
///   communicate with the WebView, see MTY_WebViewSendText and MTY_EVENT_WEBVIEW_TEXT. The
///   WebView is based off of Windows' WebView2 system shipped with Windows 11 and Apple's
///   WKWebView framework.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param dir Path to where temporary files related to the WebView will be stored (Windows only).
///   If MTY_WebViewIsSteam returns true, `dir` will be interpreted as the directory where the
///   Steam API shared object resides (i.e. `libsteam_api.so`).
/// @param debug Set to true to enable debugging tools.
/// @returns Returns true on success, false if the WebView failed to be set or already exists.
//- #support Windows macOS
MTY_EXPORT bool
MTY_WindowSetWebView(MTY_App *app, MTY_Window window, const char *dir, bool debug);

/// @brief Remove the WebView from a window.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT void
MTY_WindowRemoveWebView(MTY_App *app, MTY_Window window);

/// @brief Navigate the WebView to a given URL or HTML string.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param source Source content to be loaded by the WebView. This argument can either be a
///   URL or direct HTML depending on the value of `url`.
/// @param url If true, `source` will be treated as a URL, else `source` will be loaded
///   directly as HTML.
MTY_EXPORT void
MTY_WebViewNavigate(MTY_App *app, MTY_Window window, const char *source, bool url);

/// @brief Check if a WebView exists on a window.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WebViewExists(MTY_App *app, MTY_Window window);

/// @brief Show or hide the WebView.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param show Set true to show the WebView, false to hide it.
MTY_EXPORT void
MTY_WebViewShow(MTY_App *app, MTY_Window window, bool show);

/// @brief Check if the WebView is visible.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT bool
MTY_WebViewIsVisible(MTY_App *app, MTY_Window window);

/// @brief Send text to the WebView's JavaScript environment.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param text UTF-8 string to be sent.
MTY_EXPORT void
MTY_WebViewSendText(MTY_App *app, MTY_Window window, const char *text);

/// @brief Reload the WebView.
/// @param app The MTY_App.
/// @param window An MTY_Window.
MTY_EXPORT void
MTY_WebViewReload(MTY_App *app, MTY_Window window);

/// @brief Open the dev tools for the WebView.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @returns Returns true on success, false if the WebView is not visible or was not initialized with debugging enabled.
MTY_EXPORT bool
MTY_WebViewOpenDevTools(MTY_App *app, MTY_Window window);

/// @brief Enable or disable input passthrough from the WebView.
/// @details When input passthrough is enabled, keyboard events from the JavaScript
///   environment will be automatically converted into MTY_Event events. See MTY_EventType for
///   details.
/// @param app The MTY_App.
/// @param window An MTY_Window.
/// @param Enable or disable input passthrough from the WebView.
MTY_EXPORT void
MTY_WebViewSetInputPassthrough(MTY_App *app, MTY_Window window, bool passthrough);

/// @brief Check if libmatoya is using the Steam API for its WebView implementation.
MTY_EXPORT bool
MTY_WebViewIsSteam(void);

/// @brief Fill an MTY_Frame taking the current display settings into account.
/// @details The returned MTY_Frame can be passed directly to MTY_WindowCreate or
///   MTY_WindowSetFrame.
/// @param x The window's horizontal offset from the center of the primary screen.
/// @param y The window's vertical offset from the center of the primary screen.
/// @param w The window's unscaled width.
/// @param h The window's unscaled height.
/// @param maxHeight Between 0.0f and 1.0f, limit the frame's height to a percentage of the
///   primary screen's height. The frame's width is adjusted accordingly keeping the original
///   aspect ratio intact. This calculation is based on the final scaled height of the window.
MTY_EXPORT MTY_Frame
MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight);

/// @brief Get the string representation of a key combination.
/// @details This function attempts to use the current locale.
/// @param mod Combo modifier.
/// @param key Combo key.
/// @param str Output string.
/// @param len Size in bytes of `str`.
MTY_EXPORT void
MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len);

/// @brief Set the app's `id`.
/// @details The app's `id`, or "Application User Model ID" on Windows is important for
///   proper toast behavior. It is used by Windows to pair the current process to its
///   concept of an application. In order for toasts to work properly on modern versions
///   of Windows, a shortcut (.lnk) also assigned this same `id` must be present in the
///   user's start menu.
/// @param id The app's `id`.
//- #support Windows
MTY_EXPORT void
MTY_SetAppID(const char *id);

/// @brief Print an MTY_Event to stdout.
/// @param evt The event to inspect.
MTY_EXPORT void
MTY_PrintEvent(const MTY_Event *evt);

/// @brief If using MTY_GFX_GL, retrieve a GL function by its name.
/// @details A GL context (WGL, GLX, EGL) must be active on the current thread for
///   this function to work properly.
/// @param name The name of the GL function, i.e. `glGenFramebuffers`.
/// @returns A pointer a function retrieved from the current GL context, or NULL
///   if the symbol was not found.
//- #support Windows Linux
MTY_EXPORT void *
MTY_GLGetProcAddress(const char *name);

/// @brief Runs a loop while also allowing the OS's event loop to continue to run.
/// @details This function only matters when targeting the Web. If a regular `while` loop is used
///   to block a thread, under the hood the `Worker` that is executing the WebAssembly will be
///   blocked and can not process the JavaScript event loop. This means that the `Worker` can not
///   communicate with the main thread via `postMessage`, and most importantly can not perform
///   deferred cleanup via the event loop, causing memory leaks.
/// @param iter Function called in a loop until it returns false.
/// @param opaque Pointer passed to each call to `iter`.
MTY_EXPORT void
MTY_RunAndYield(MTY_IterFunc iter, void *opaque);

/// @brief Wait until signaled by the main JavaScript thread. Web only.
/// @details When providing a `userEnv` argument to the MTY_Start JavaScript function, it may
///   become necessary to have the WebAssembly thread wait while running asynchronous code in
///   JavaScript. All functions supplied via the `userEnv` argument will always run on the main
///   JavaScript thread which is not allowed to block in any way. These functions should call
///   MTY_SignalPtr when done with their asynchronous operations on a `sync` variable, which
///   will then unblock the WebAssembly side waiting with MTY_WaitPtr.
/// @param sync Pointer to shared memory. This memory must not be allocated on the stack.
//- #support Web
MTY_EXPORT void
MTY_WaitPtr(int32_t *sync);


//- #module Audio
//- #mbrief Simple audio playback and resampling.
//- #mdetails This is a very minimal interface that assumes 2-channel, 16-bit signed PCM
//-   submitted by pushing to a queue. This module also includes a straightforward
//-   resampler.

typedef struct MTY_Audio MTY_Audio;
typedef struct MTY_Resampler MTY_Resampler;

/// @brief Create an MTY_Audio context for playback.
/// @param sampleRate Audio sample rate in KHz.
/// @param minBuffer The minimum amount of audio in milliseconds that must be queued
///   before playback begins.
/// @param maxBuffer The maximum amount of audio in milliseconds that can be queued
///   before audio begins getting dropped. The queue will flush to zero, then begin
///   building back towards `minBuffer` again before playback resumes.
/// @param channels Number of audio channels.
/// @param deviceID Specify a specific audio device for playback, or NULL for the default
///   device. Windows only.
/// @param fallback If `deviceID` is not NULL, set to true to fallback to the default device, or
///   false to cause this function to fail and return NULL. Windows only.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_Audio context must be destroyed with MTY_AudioDestroy.
MTY_EXPORT MTY_Audio *
MTY_AudioCreate(uint32_t sampleRate, uint32_t minBuffer, uint32_t maxBuffer, uint8_t channels,
	const char *deviceID, bool fallback);

/// @brief Destroy an MTY_Audio context.
/// @param audio Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_AudioDestroy(MTY_Audio **audio);

/// @brief Flush the context and reset it as though it was just created.
/// @details Internally this function will stop playback so the OS does not think
///   audio is actively playing. This function should be called whenever you will not
///   be queuing audio for a while.
/// @param ctx An MTY_Audio context.
MTY_EXPORT void
MTY_AudioReset(MTY_Audio *ctx);

/// @brief Get the number of milliseconds currently queued for playback.
/// @param ctx An MTY_Audio context.
MTY_EXPORT uint32_t
MTY_AudioGetQueued(MTY_Audio *ctx);

/// @brief Queue 16-bit signed PCM audio for playback.
/// @param ctx An MTY_Audio context.
/// @param frames Buffer containing 16-bit signed PCM audio frames. The number of audio channels
///   is specified during MTY_AudioCreate. In the case of 2-channel PCM, one audio frame is two
///   samples, each sample being one channel.
/// @param count The number of frames contained in `frames`. The number of frames would
///   be the size of `frames` in bytes divided by 2 * `channels` specified during MTY_AudioCreate.
MTY_EXPORT void
MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count);

/// @brief Create and MTY_Resampler for sample rate conversion.
/// @returns The returned MTY_Resampler must be destroyed with MTY_ResamplerDestroy.
MTY_EXPORT MTY_Resampler *
MTY_ResamplerCreate(void);

/// @brief Destroy an MTY_Resampler.
/// @param resampler Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_ResamplerDestroy(MTY_Resampler **resampler);

/// @brief Resample 2-channel, 16-bit signed PCM audio.
/// @param ctx An MTY_Resampler.
/// @param ratio The output sample rate divided by the input sample rate.
/// @param in Input buffer containing 2-channel, 16-bit signed PCM audio frames.
/// @param inFrames The number of frames contained in `in`. The number of frames
///   would be the size of `in` in bytes divided by 4.
/// @param outFrames Set to the number of frames in the returned buffer.
/// @returns Output buffer containing the resampled audio. This pointer remains
///   valid as long as `ctx` is not destroyed.
MTY_EXPORT const int16_t *
MTY_Resample(MTY_Resampler *ctx, float ratio, const int16_t *in, size_t inFrames,
	size_t *outFrames);

/// @brief Reset all internal state as though the MTY_Resampler was just created.
/// @param ctx An MTY_Resampler.
MTY_EXPORT void
MTY_ResamplerReset(MTY_Resampler *ctx);


//- #module Compression
//- #mbrief Basic compression.
//- #mdetails These functions are platform specific and any data compressed with
//-   these functions should only be decompressed on the same platform.

/// @brief Compress data with a balanced algorithm.
/// @param input Uncompressed input buffer.
/// @param inputSize Size in bytes of `input`.
/// @param outputSize Set to the size in bytes of the returned compressed buffer.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_Compress(const void *input, size_t inputSize, size_t *outputSize);

/// @brief Decompress data compressed with MTY_Compress.
/// @details This function should only be called on the same platform that originally
///   called MTY_Compress.
/// @param input Compressed input buffer.
/// @param inputSize Size in bytes of `input`.
/// @param outputSize Set to the size in bytes of the returned decompressed buffer.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_Decompress(const void *input, size_t inputSize, size_t *outputSize);


//- #module Crypto
//- #mbrief Common cryptography tasks.

#define MTY_SHA1_SIZE      20 ///< Size in bytes of a SHA-1 digest.
#define MTY_SHA1_HEX_MAX   48 ///< Comfortable buffer size for a hex string SHA-1 digest.
#define MTY_SHA256_SIZE    32 ///< Size in bytes of a SHA-256 digest.
#define MTY_SHA256_HEX_MAX 72 ///< Comfortable buffer size for a hex string SHA-256 digest.

typedef struct MTY_AESGCM MTY_AESGCM;

/// @brief Hash algorithms.
typedef enum {
	MTY_ALGORITHM_SHA1       = 0, ///< SHA-1 hash function.
	MTY_ALGORITHM_SHA1_HEX   = 1, ///< SHA-1 hash function with hex string output.
	MTY_ALGORITHM_SHA256     = 2, ///< SHA-256 hash function.
	MTY_ALGORITHM_SHA256_HEX = 3, ///< SHA-256 hash function with hex string output.
	MTY_ALGORITHM_MAKE_32    = INT32_MAX,
} MTY_Algorithm;

/// @brief CRC32 checksum.
/// @details This CRC32 implementation uses the reverse polynomial `0xEDB88320`.
/// @param crc CRC32 seed value.
/// @param buf Input buffer.
/// @param size Size in bytes of `buf`.
MTY_EXPORT uint32_t
MTY_CRC32(uint32_t crc, const void *buf, size_t size);

/// @brief Daniel J. Bernstein's classic string hash function.
/// @param str String to hash.
/// @returns Hash value.
MTY_EXPORT uint32_t
MTY_DJB2(const char *str);

/// @brief Convert bytes to a hex string.
/// @details This function will safely truncate overflows with a null character.
/// @param bytes Input buffer.
/// @param size Size in bytes of `bytes`.
/// @param hex Hex string output buffer.
/// @param hexSize Size in bytes of `hex`.
MTY_EXPORT void
MTY_BytesToHex(const void *bytes, size_t size, char *hex, size_t hexSize);

/// @brief Convert a hex string to bytes.
/// @param hex Hex string input buffer.
/// @param bytes Output buffer.
/// @param size Size in bytes of `bytes`.
MTY_EXPORT void
MTY_HexToBytes(const char *hex, void *bytes, size_t size);

/// @brief Convert bytes to a Base64 string.
/// @details This function will safely truncate overflows with a null character.
/// @param bytes Input buffer.
/// @param size Size in bytes of `bytes`.
/// @param base64 Base64 string output buffer.
/// @param base64Size Size in bytes of `base64`.
MTY_EXPORT void
MTY_BytesToBase64(const void *bytes, size_t size, char *base64, size_t base64Size);

/// @brief Run a hash algorithm on a buffer with optional HMAC key.
/// @param algo Hash algorithm to use.
/// @param input Input buffer.
/// @param inputSize Size in bytes of `input`.
/// @param key HMAC key to use. May be NULL, in which case HMAC is not used.
/// @param keySize Size in bytes of `key`, or 0 if `key` is NULL.
/// @param output Output buffer.
/// @param outputSize Size in bytes of `output`.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize);

/// @brief Run a hash algorithm on the contents of a file with optional HMAC key.
/// @param algo Hash algorithm to use.
/// @param path Path to the input file.
/// @param key HMAC key to use. May be NULL, in which case HMAC is not used.
/// @param keySize Size in bytes of `key`, or 0 if `key` is NULL.
/// @param output Output buffer.
/// @param outputSize Size in bytes of `output`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
//- #support Windows macOS Android Linux
MTY_EXPORT bool
MTY_CryptoHashFile(MTY_Algorithm algo, const char *path, const void *key, size_t keySize,
	void *output, size_t outputSize);

/// @brief Generate cryptographically strong random bytes.
/// @param buf Output buffer.
/// @param size Size in bytes of `buf`.
MTY_EXPORT void
MTY_GetRandomBytes(void *buf, size_t size);

/// @brief Generate a random unsigned integer within a range.
/// @param minVal Low of the random range, inclusive.
/// @param maxVal High end of the random range, exclusive.
/// @returns If `maxVal <= minVal`, `minVal` is returned.
MTY_EXPORT uint32_t
MTY_GetRandomUInt(uint32_t minVal, uint32_t maxVal);

/// @brief Create an MTY_AESGCM context for AES-GCM-128 encryption/decryption.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_AESGCM context must be destroyed with MTY_AESGCMDestroy.
/// @param key The secret key to use for encryption. This buffer must be 16 bytes,
///   the size necessary for AES-128.
//- #support Windows macOS Android Linux
MTY_EXPORT MTY_AESGCM *
MTY_AESGCMCreate(const void *key);

/// @brief Destroy an MTY_AESGCM context.
/// @param aesgcm Passed by reference and set to NULL after being destroyed.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_AESGCMDestroy(MTY_AESGCM **aesgcm);

/// @brief Encrypt plain text with a nonce using AES-GCM-128 and output the GCM tag.
/// @param ctx An MTY_AESGCM context.
/// @param nonce A buffer used as salt during encryption. This buffer must be 12 bytes,
///   and it MUST be different for each call to this function using the same
///   MTY_AESGCM context.
/// @param plainText The input data to be encrypted.
/// @param size Size in bytes of `plainText`.
/// @param tag Buffer that will hold the GCM tag. This buffer must be 16 bytes.
/// @param cipherText Output buffer for the encrypted data, always the same size
///   as `plainText`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
//- #support Windows macOS Android Linux
MTY_EXPORT bool
MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText);

/// @brief Decrypt cipher text with a nonce and GCM tag using AES-GCM-128.
/// @param ctx An MTY_AESGCM context.
/// @param nonce This buffer must be 12 bytes and it must match the `nonce` used
///   during encryption.
/// @param cipherText The input data to be decrypted.
/// @param size Size in bytes of `cipherText`.
/// @param tag The GCM tag to authenticate against. This buffer must be 16 bytes.
/// @param plainText Output buffer for the decrypted data, always the same size
///   as `cipherText`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
//- #support Windows macOS Android Linux
MTY_EXPORT bool
MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText,
	size_t size, const void *tag, void *plainText);


//- #module Dialog
//- #mbrief Stock dialog boxes provided by the OS.

/// @brief Check if the current OS supports built in dialogs.
MTY_EXPORT bool
MTY_HasDialogs(void);

/// @brief Show a simple message box with a title and formatted string.
/// @details Warning: Be careful with your format string, if it is incorrect this
///   function will have undefined behavior.
/// @param title Title of the message box.
/// @param fmt Format string for the body of the message box.
/// @param ... Variable arguments as specified by `fmt`.
//- #support Windows macOS
MTY_EXPORT void
MTY_ShowMessageBox(const char *title, const char *fmt, ...) MTY_FMT(2, 3);

/// @brief Creates a dialog box for opening files.
/// @title The title of the dialog. May be NULL for the default behavior.
/// @app The MTY_App owner of the dialog. May be NULL for none.
/// @window The MTY_Window owner of the dialog. Ignored if `app` is NULL.
/// @returns The selected file name allocated in thread local storage. Must not be freed.
MTY_EXPORT const char *
MTY_OpenFile(const char *title, MTY_App *app, MTY_Window window);


//- #module File
//- #mbrief Simple filesystem helpers.
//- #mdetails These functions are not intended for optimized IO or large files, they
//-   are convenience functions that simplify common filesystem operations.

#define MTY_PATH_MAX 1280       ///< Maximum size of a full path used internally by libmatoya.
#define MTY_FILE_MAX 0x40000000 ///< Maximum size of a file that can be read by libmatoya.

typedef struct MTY_LockFile MTY_LockFile;

/// @brief Special directories on the filesystem.
typedef enum {
	MTY_DIR_CWD         = 0, ///< The current working directory for the process.
	MTY_DIR_HOME        = 1, ///< The current user's home directory. On Windows this will usually
	                         ///<   be `%APPDATA%\Roaming`, and on Unix it will be `HOME`. On
	                         ///<   Android, libmatoya will set this to the app's External
	                         ///<   Files Directory.
	MTY_DIR_PROGRAMS    = 2, ///< The system's preferred programs directory. On Windows, usually
	                         ///<   `%ProgramFiles%`, and on Unix hard-coded to `/usr/bin`.
	MTY_DIR_GLOBAL_HOME = 3, ///< The system's concept of a home directory shared by all users. On
	                         ///<   Unix this is the same as MTY_DIR_HOME, on Windows this will be
	                         ///<   `%ProgramData`.
	MTY_DIR_MAKE_32     = INT32_MAX,
} MTY_Dir;

/// @brief File shared access modes.
typedef enum {
	MTY_FILE_MODE_SHARED    = 0, ///< Shared access, allowing many open readers but no writers.
	MTY_FILE_MODE_EXCLUSIVE = 1, ///< Exclusive access, allowing only a single open writer.
	MTY_FILE_MODE_MAKE_32   = INT32_MAX,
} MTY_FileMode;

/// @brief File properties.
typedef struct {
	char *path;    ///< The base path to the file.
	char *name;    ///< The file name.
	uint64_t size; ///< The file size in bytes.
	bool dir;      ///< The file is a directory.
} MTY_FileDesc;

/// @brief A list of files.
typedef struct {
	MTY_FileDesc *files; ///< List of file descriptions.
	uint32_t len;        ///< Number of elements in `files`.
} MTY_FileList;

/// @brief Read the entire contents of a file.
/// @details This function reads the file in binary mode.
/// @param path Path to the file.
/// @param size Set to the size in bytes of the returned buffer. May be NULL.
/// @returns The buffer always has its final byte set to 0, allowing you to treat
///   it like a string. This extra byte is not counted in the returned `size`.\n\n
///   On failure, NULL is returned. Call MTY_GetLog for details.\n\n
MTY_EXPORT void *
MTY_ReadFile(const char *path, size_t *size);

/// @brief Write a buffer to a file.
/// @details This function writes to the file in binary mode.
/// @param path Path to the file.
/// @param buf Input buffer to write.
/// @param size Size in bytes of `buf`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_WriteFile(const char *path, const void *buf, size_t size);

/// @brief Write formatted text to a file.
/// @details This function writes to the file in text mode.\n\n
///   Warning: Be careful with your format string, if it is incorrect this
///   function will have undefined behavior.
/// @param path Path to the file.
/// @param fmt Format string to write.
/// @param ... Variable arguments as specified by `fmt`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_WriteTextFile(const char *path, const char *fmt, ...) MTY_FMT(2, 3);

/// @brief Append formatted text to a file.
/// @details This function appends to the file in text mode.\n\n
///   Warning: Be careful with your format string, if it is incorrect this
///   function will have undefined behavior.
/// @param path Path to the file.
/// @param fmt Format string to append.
/// @param ... Variable arguments as specified by `fmt`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_AppendTextToFile(const char *path, const char *fmt, ...) MTY_FMT(2, 3);

/// @brief Delete a file.
/// @param path Path to the file.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_DeleteFile(const char *path);

/// @brief Check if a file exists.
/// @param path Path to the file.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_FileExists(const char *path);

/// @brief Create a directory with subdirectories.
/// @details This function behaves in a similar manner to the `mkdir -p` Unix utility.
/// @param path Path to the directory.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_Mkdir(const char *path);

/// @brief Join two components of a path using the appropriate delimiter.
/// @param path0 First component of the joined path.
/// @param path1 Second component of the joined path.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_JoinPath(const char *path0, const char *path1);

/// @brief Resolves a relative path into a full absolute path.
/// @details This function expands all symbolic links and resolves references to
///   `..` and `.`. It also removes extra delimiters.
/// @param path The relative path.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_ResolvePath(const char *path);

/// @brief Copy a file.
/// @param src Path to the source file.
/// @param dst Path to the destination file.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_CopyFile(const char *src, const char *dst);

/// @brief Move a file.
/// @param src Path to the source file.
/// @param dst Path to the destination file.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_MoveFile(const char *src, const char *dst);

/// @brief Get a special directory on the filesystem.
/// @param dir The special directory of interest.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetDir(MTY_Dir dir);

/// @brief Parse a path and extract the file name with or without the extension.
/// @param path Path to a file.
/// @param extension If interested in the extension, true, otherwise false to remove
///   the file extension.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetFileName(const char *path, bool extension);

/// @brief Parse a path and extract the file extension.
/// @param path Path to a file.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetFileExtension(const char *path);

/// @brief Get all but the final component of a path.
/// @param path Path to a file or directory.
/// @returns If `path` contains a file name, it is removed and the base path is returned.
///   Otherwise, one directory above `path` is returned.\n\n
///   This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetPathPrefix(const char *path);

/// @brief Create an MTY_LockFile for signaling resource ownership across processes.
/// @details The process that holds the lock will continue to hold the lock unil
///   MTY_LockFileDestroy is called or the process terminates. The lock file will
///   be created if it does not exist.
/// @param path Path to the lock file.
/// @param mode Choose between exclusive or shared read access.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_LockFile must be destroyed with MTY_LockFileDestroy.
MTY_EXPORT MTY_LockFile *
MTY_LockFileCreate(const char *path, MTY_FileMode mode);

/// @brief Destroy an MTY_LockFile.
/// @param lockFile Passed by reference and set to NULL after being destroyed.\n\n
///   Destroying the MTY_LockFile will not delete the actual file from the filesystem,
///   it simply releases the process' ownership of the lock file.
MTY_EXPORT void
MTY_LockFileDestroy(MTY_LockFile **lockFile);

/// @brief Get a list of all files and directories contained in a path.
/// @param path Path to a directory.
/// @param filter Substring that must match each file that should be returned. All
///   directories will be returned regardless of this filter. May be NULL for no filter.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_FileList must be destroyed with MTY_FreeFileList.
MTY_EXPORT MTY_FileList *
MTY_GetFileList(const char *path, const char *filter);

/// @brief Free a file list returned by MTY_GetFileList.
/// @param fileList Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_FreeFileList(MTY_FileList **fileList);


//- #module Image
//- #mbrief Image compression and cropping. Program icons.
//- #mdetails Basic image processing with support for only PNG and JPEG.

/// @brief Image compression methods.
typedef enum {
	MTY_IMAGE_COMPRESSION_PNG     = 0, ///< PNG encoded image.
	MTY_IMAGE_COMPRESSION_JPEG    = 1, ///< JPEG encoded image.
	MTY_IMAGE_COMPRESSION_MAKE_32 = INT32_MAX,
} MTY_ImageCompression;

/// @brief Compress an RGBA image.
/// @param method The compression method to be used on `input`.
/// @param input RGBA 8-bits per channel image data.
/// @param width The width of the `input` image.
/// @param height The height of the `input` image.
/// @param outputSize Set to the size in bytes of the returned buffer.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize);

/// @brief Decompress an image into RGBA.
/// @param input The compressed image data.
/// @param size The size in bytes of `input`.
/// @param width Set to the width of the returned buffer.
/// @param height Set to the height of the returned buffer.
/// @returns The size of the returned buffer can be calculated as
///   `width * height * 4`.\n\n
///   On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height);

/// @brief Center crop an RGBA image.
/// @param image RGBA 8-bits per channel image to be cropped.
/// @param cropWidth The desired cropped width.
/// @param cropHeight The desired cropped height.
/// @param width Set this to the current width of `image` before calling this function.
///   On output, set to the actual cropped width of the returned buffer.
/// @param height Set this to the current height of `image` before calling this function.
///   On output, set to the actual cropped height of the returned buffer.
/// @returns The cropped image.\n\n
///   On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_CropImage(const void *image, uint32_t cropWidth, uint32_t cropHeight,
	uint32_t *width, uint32_t *height);

/// @brief Get an application's program icon as an RGBA image.
/// @param path Path to the application binary.
/// @param width Set to the width of the returned buffer.
/// @param height Set to the height of the returned buffer.
/// @returns The returned buffer is RGBA 8-bits per channel.\n\n
//    On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned buffer must be destroyed with MTY_Free.
//- #support Windows
MTY_EXPORT void *
MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height);


//- #module JSON
//- #mbrief JSON parsing and construction.

/// @brief JSON types.
typedef enum {
	MTY_JSON_NULL    = 0, ///< JSON `null` type.
	MTY_JSON_BOOL    = 1, ///< JSON boolean.
	MTY_JSON_NUMBER  = 2, ///< JSON number (double).
	MTY_JSON_STRING  = 3, ///< JSON string.
	MTY_JSON_ARRAY   = 4, ///< JSON array.
	MTY_JSON_OBJECT  = 5, ///< JSON object.
	MTY_JSON_MAKE_32 = INT32_MAX,
} MTY_JSONType;

typedef struct MTY_JSON MTY_JSON;

/// @brief Parse a string into an MTY_JSON item.
/// @param input Serialized JSON string.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONParse(const char *input);

/// @brief Parse the contents of a file into an MTY_JSON item.
/// @param path Path to the serialized JSON file.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONReadFile(const char *path);

/// @brief Deep copy an MTY_JSON item.
/// @param json The MTY_JSON item to duplicate.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONDuplicate(const MTY_JSON *json);

/// @brief Get the MTY_JSONType of an MTY_JSON item.
/// @param json An MTY_JSON item.
MTY_EXPORT MTY_JSONType
MTY_JSONGetType(const MTY_JSON *json);

/// @brief Destroy an MTY_JSON item.
/// @param json Passed by reference and set to NULL after being destroyed.\n\n
///   This function destroys all children of the item, meaning only the root item
///   in the hierarchy should ever be destroyed.
MTY_EXPORT void
MTY_JSONDestroy(MTY_JSON **json);

/// @brief Serialize an MTY_JSON item into a string.
/// @param json An MTY_JSON item to serialize.
/// @returns The returned string must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_JSONSerialize(const MTY_JSON *json);

/// @brief Serialize an MTY_JSON item and write it to a file.
/// @details This function "pretty prints" the JSON, adding spaces, newlines, and tabs
///   where appropriate.
/// @param path Path to a file where the serialized output will be written.
/// @param json An MTY_JSON item to serialize.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_JSONWriteFile(const char *path, const MTY_JSON *json);

/// @brief Create a new MTY_JSON null item.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONNullCreate(void);

/// @brief Create a new MTY_JSON boolean.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONBoolCreate(bool value);

/// @brief Get the bool value from an MTY_JSON boolean.
/// @param json An MTY_JSON boolean.
/// @param value The bool value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_BOOL.
MTY_EXPORT bool
MTY_JSONBool(const MTY_JSON *json, bool *value);

/// @brief Create a new MTY_JSON number.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONNumberCreate(double value);

/// @brief Create a new MTY_JSON number, internally represented as a 32-bit integer.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONIntCreate(int32_t value);

/// @brief Get the double value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The double value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
MTY_EXPORT bool
MTY_JSONNumber(const MTY_JSON *json, double *value);

/// @brief Get the int8_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int8_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
MTY_EXPORT bool
MTY_JSONInt8(const MTY_JSON *json, int8_t *value);

/// @brief Get the int16_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int16_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
MTY_EXPORT bool
MTY_JSONInt16(const MTY_JSON *json, int16_t *value);

/// @brief Get the int32_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int32_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
MTY_EXPORT bool
MTY_JSONInt32(const MTY_JSON *json, int32_t *value);

/// @brief Get the float value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The float value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
MTY_EXPORT bool
MTY_JSONFloat(const MTY_JSON *json, float *value);

/// @brief Create a new MTY_JSON string.
/// @param value Initialization UTF-8 string.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONStringCreate(const char *value);

/// @brief Get the string value from an MTY_JSON string.
/// @param json An MTY_JSON string.
/// @param value Filled with the string value from `json`.
/// @param size Size in bytes of `value`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_STRING or
///   the string value from `json` could not fit in `value`.
MTY_EXPORT bool
MTY_JSONString(const MTY_JSON *json, char *value, size_t size);

/// @brief Get the internal string pointer from an MTY_JSON string.
/// @param json An MTY_JSON string.
/// @returns This reference is valid only as long as the `json` item is also valid.\n\n
///   If `json` is not an MTY_JSON string, NULL is returned.
MTY_EXPORT const char *
MTY_JSONStringPtr(const MTY_JSON *json);

/// @brief Create a new MTY_JSON array.
/// @param len The number of elements allocated in the array.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONArrayCreate(uint32_t len);

/// @brief Get the number of items in an MTY_JSON array.
/// @param json An MTY_JSON item to query.
MTY_EXPORT uint32_t
MTY_JSONArrayGetLength(const MTY_JSON *json);

/// @brief Get an item from an MTY_JSON array.
/// @param json An MTY_JSON array.
/// @param index Index to lookup.
/// @returns If the `index` exists, the item at that index is returned. This reference
///   is valid only as long as the `json` item is also valid.\n\n
///   If the `index` does not exist, NULL is returned.
MTY_EXPORT const MTY_JSON *
MTY_JSONArrayGetItem(const MTY_JSON *json, uint32_t index);

/// @brief Set an item in an MTY_JSON array.
/// @param json An MTY_JSON array.
/// @param index The array index where `value` will be stored.
/// @param value Item to set at `index`.
/// @returns Returns true if `value` was set successfully, otherwise false.
MTY_EXPORT bool
MTY_JSONArraySetItem(MTY_JSON *json, uint32_t index, MTY_JSON *value);

/// @brief Create a new MTY_JSON object.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_EXPORT MTY_JSON *
MTY_JSONObjCreate(void);

/// @brief Iterate through all keys in an MTY_JSON object.
/// @param json An MTY_JSON object.
/// @param iter Iterator that keeps track of the position in the object. Set this to
///   0 before the fist call to this function.
/// @param key Reference to the next key in the object.
/// @returns Returns true if there are more keys available, otherwise false.
MTY_EXPORT bool
MTY_JSONObjGetNextKey(const MTY_JSON *json, uint64_t *iter, const char **key);

/// @brief Get an item from an MTY_JSON object.
/// @param json An MTY_JSON object.
/// @param key Key to lookup.
/// @returns If the `key` exists, the associated item is returned. This reference is
///   valid only as long as the `json` item is also valid.\n\n
///   If the `key` does not exist, NULL is returned.
MTY_EXPORT const MTY_JSON *
MTY_JSONObjGetItem(const MTY_JSON *json, const char *key);

/// @brief Set an item in an MTY_JSON object.
/// @details This function will replace an existing key with the same name.
/// @param json An MTY_JSON object.
/// @param key Key to set.
/// @param value Value associated with `key`.
/// @returns Returns true if `value` was set successfully, otherwise false.
MTY_EXPORT bool
MTY_JSONObjSetItem(MTY_JSON *json, const char *key, MTY_JSON *value);

#define MTY_JSONObjGetBool(json, key, val) \
	MTY_JSONBool(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt8(json, key, val) \
	MTY_JSONInt8(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt16(json, key, val) \
	MTY_JSONInt16(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt(json, key, val) \
	MTY_JSONInt32(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetFloat(json, key, val) \
	MTY_JSONFloat(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetString(json, key, val, size) \
	MTY_JSONString(MTY_JSONObjGetItem(json, key), val, size)

#define MTY_JSONObjGetStringPtr(json, key) \
	MTY_JSONStringPtr(MTY_JSONObjGetItem(json, key))

#define MTY_JSONObjSetBool(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONBoolCreate(val))

#define MTY_JSONObjSetNumber(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONNumberCreate(val))

#define MTY_JSONObjSetInt(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONIntCreate(val))

#define MTY_JSONObjSetString(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONStringCreate(val))

#define MTY_JSONArraySetString(json, index, val) \
	MTY_JSONArraySetItem(json, index, MTY_JSONStringCreate(val))


//- #module Log
//- #mbrief Get logs, add logs, and set a log callback.

#define MTY_Log(msg, ...) \
	MTY_LogParams(__FUNCTION__, msg, ##__VA_ARGS__)

#define MTY_LogFatal(msg, ...) \
	MTY_LogFatalParams(__FUNCTION__, msg, ##__VA_ARGS__)

/// @brief Function called when a new log message is available.
/// @param msg The formatted log message.
/// @param opaque Pointer set via MTY_SetLogFunc.
typedef void (*MTY_LogFunc)(const char *msg, void *opaque);

/// @brief Get the most recent log message on the thread.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetLog(void);

/// @brief Set a function to receive log messages.
/// @details This function is set globally.
/// @param func Function called when a new log message is available. Set to NULL
///   to remove a previously set `func`.
/// @param opaque Passed to `func` when it is called.
MTY_EXPORT void
MTY_SetLogFunc(MTY_LogFunc func, void *opaque);

/// @brief Temporarily disable all logging.
/// @param disabled Specify true to disable logging, false to enable it.
MTY_EXPORT void
MTY_DisableLog(bool disabled);

/// @brief Log a formatted string.
/// @details This function is intended to be called internally via the
///   MTY_Log macro, but can be used to add to the libmatoya log.
/// @param func The name of the function that produced the message. The MTY_Log macro
///   automatically fills this value.
/// @param fmt Format string.
/// @param ... Variable arguments as specified by `fmt`.
MTY_EXPORT void
MTY_LogParams(const char *func, const char *fmt, ...) MTY_FMT(2, 3);

/// @brief Log a formatted string then abort.
/// @details This function is intended to be called internally via the
///   MTY_LogFatal macro.
/// @param func The name of the function that produced the message. The MTY_LogFatal
///   macro automatically fills this value.
/// @param fmt Format string.
/// @param ... Variable arguments as specified by `fmt`.
MTY_EXPORT void
MTY_LogFatalParams(const char *func, const char *fmt, ...) MTY_FMT(2, 3);


//- #module Memory
//- #mbrief Memory allocation and manipulation.
//- #mdetails These functions are mostly thin wrappers around C standard library
//-   functions that have platform differences. Additionally, functions like MTY_Alloc
//-   wrap `calloc` but `abort()` on failure.

#define MTY_MIN(a, b) \
	((a) > (b) ? (b) : (a))

#define MTY_MAX(a, b) \
	((a) > (b) ? (a) : (b))

#define MTY_ALIGN16(v) \
	((v) + 0xF & ~((uintptr_t) 0xF))

#define MTY_ALIGN32(v) \
	((v) + 0x1F & ~((uintptr_t) 0x1F))

/// @brief Function called while running MTY_Sort.
/// @param e0 An element evaluated during MTY_Sort.
/// @param e1 An element evaluated during MTY_Sort.
/// @returns If less than 0, `e0` goes before `e1`. If greater than 0, `e1` goes
///   before `e0`. Otherwise, the position is unchanged.
typedef int32_t (*MTY_CompareFunc)(const void *e0, const void *e1);

/// @brief Guarantee memory is zeroed without compiler interference.
/// @param mem Buffer to zero.
/// @param size Size in bytes of `mem`.
MTY_EXPORT void
MTY_SecureZero(void *mem, size_t size);

/// @brief Allocate zeroed memory.
/// @details For more information, see `calloc` from the C standard library.
/// @param len Number of elements requested.
/// @param size Size in bytes of each element.
/// @returns The zeroed buffer.\n\n
///   This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_Alloc(size_t len, size_t size);

/// @brief Allocate zeroed aligned memory.
/// @details For more information, see `_aligned_malloc` on Windows and
///   `posix_memalign` on Unix.
/// @param size Size in bytes of the requested buffer.
/// @param align Alignment required in bytes. This value must be a power of two
///   and at most 1024.
/// @returns The zeroed and aligned buffer.\n\n
///   This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_FreeAligned.
MTY_EXPORT void *
MTY_AllocAligned(size_t size, size_t align);

/// @brief Free allocated memory.
/// @details For more information, see `free` from the C standard library.
/// @param mem Dynamically allocated memory.
MTY_EXPORT void
MTY_Free(void *mem);

/// @brief Free aligned allocated memory.
/// @param mem Memory allocated by MTY_AllocAligned.
MTY_EXPORT void
MTY_FreeAligned(void *mem);

/// @brief Guarantee allocated memory is zeroed before it is freed.
/// @param mem Dynamically allocated memory.
/// @param size Size in bytes of `mem`.
MTY_EXPORT void
MTY_SecureFree(void *mem, size_t size);

/// @brief Resize previously allocated memory.
/// @details For more information, see `realloc` from the C standard library.
/// @param mem Previously allocated buffer.
/// @param len Total number of elements in the new buffer.
/// @param size Size of each element.
/// @returns The resized buffer. The additional portion of the buffer is not zeroed.\n\n
///   This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_Realloc(void *mem, size_t len, size_t size);

/// @brief Duplicate a buffer.
/// @param mem Buffer to duplicate.
/// @param size Size in bytes of `mem`.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT void *
MTY_Dup(const void *mem, size_t size);

/// @brief Duplicate a string.
/// @param str String to duplicate.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_Strdup(const char *str);

/// @brief Append to a string.
/// @details For more information, see `strcat_s` from the C standard library.
/// @param dst Destination string.
/// @param size Total size in bytes of `dst`.
/// @param src String to append to `dst`.
MTY_EXPORT void
MTY_Strcat(char *dst, size_t size, const char *src);

/// @brief Dynamically format a string with a va_list.
/// @details For more information, see `vsnprintf` from the C standard library.
/// @param fmt Format string.
/// @param args Variable arguments in the form of a va_list as specified by `fmt`.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_VsprintfD(const char *fmt, va_list args);

/// @brief Dynamically format a string.
/// @details For more information, see `snprintf` from the C standard library.\n\n
///   Warning: Be careful with your format string, if it is incorrect this
///   function will have undefined behavior.
/// @param fmt Format string.
/// @param ... Variable arguments as specified by `fmt`.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_SprintfD(const char *fmt, ...) MTY_FMT(1, 2);

/// @brief Dynamically format a string and put the result in thread local storage.
/// @details For more information, see `snprintf` from the C standard library.\n\n
///   Warning: Be careful with your format string, if it is incorrect this
///   function will have undefined behavior.
/// @param fmt Format string.
/// @param ... Variable arguments as specified by `fmt`.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_SprintfDL(const char *fmt, ...) MTY_FMT(1, 2);

/// @brief Search a string for a list of substrings.
/// @param a String to be searched.
/// @param b List of substrings delimited by `delim`.
/// @param delim Delimiters used to for `s1`. Each character in this string is treated
///   as a delimiter like MTY_Strtok.
/// @returns Returns true if any of `s1` is found in `s0`, otherwise false.
MTY_EXPORT bool
MTY_StrSearch(const char *s0, const char *s1, const char *delim);

/// @brief Case insensitive string comparison.
/// @details For more information, see `strcasecmp` from the C standard library.
/// @param s0 First string to compare.
/// @param s1 Second string to compare.
/// @returns If the strings match, returns 0, otherwise a non-zero value.
MTY_EXPORT int32_t
MTY_Strcasecmp(const char *s0, const char *s1);

/// @brief Case insensitive substring search.
/// @details For more information, see `strcasestr` from the C standard library.
/// @param s0 String to be scanned.
/// @param s1 String containing the sequence of characters to match.
/// @returns A pointer to the first occurrence in `s1` of the entire sequence of
///   characters specified in `s0`, or NULL if the sequence is not present in `s0`.
MTY_EXPORT char *
MTY_Strcasestr(const char *s0, const char *s1);

/// @brief Reentrant string tokenization.
/// @details For more information, see `strtok_r` from the C standard library.
/// @param str The string to tokenize on the first call to this function. On
///   subsequent calls, NULL. This string is mutated each time this function is called.
/// @param delim A string of tokens that are used as delimiters, i.e. `" \t\r"` would
///   tokenize around certain whitespace characters.
/// @param saveptr Should be set to NULL before the first call to this function. This
///   pointer acts as a context variable.
/// @returns See `strtok_r` from the C standard library for more information.
MTY_EXPORT char *
MTY_Strtok(char *str, const char *delim, char **saveptr);

/// @brief Stable qsort.
/// @details For more information, see `qsort` from the C standard library. The
///   difference between this function and `qsort` is that the order of elements
///   that compare equally will be preserved.
/// @param buf The buffer to sort.
/// @param len Number of elements in `buf`.
/// @param size Size in bytes of each element.
/// @param func Function called to compare elements as the algorithm processes the
///   buffer.
MTY_EXPORT void
MTY_Sort(void *buf, size_t len, size_t size, MTY_CompareFunc func);

/// @brief Convert a wide character string to its UTF-8 equivalent.
/// @param src Source wide character string.
/// @param dst Destination UTF-8 string.
/// @param size Size in bytes of `dst`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.\n\n
///   Even if a proper UTF-8 conversion fails, this function attempts to fill `dst` with
///   a best effort conversion.
MTY_EXPORT bool
MTY_WideToMulti(const wchar_t *src, char *dst, size_t size);

/// @brief Dynamic version of MTY_WideToMulti.
/// @param src Source wide character string.
/// @returns The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT char *
MTY_WideToMultiD(const wchar_t *src);

/// @brief Thread local version of MTY_WideToMulti.
/// @param src Source wide character string.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_WideToMultiDL(const wchar_t *src);

/// @brief Convert a UTF-8 string to its wide character equivalent.
/// @param src Source UTF-8 string.
/// @param dst Destination wide character string.
/// @param len Length in characters (not bytes) of `dst`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.\n\n
///   Even if a proper wide character conversion fails, this function attempts to fill
///   `dst` with a best effort conversion.
MTY_EXPORT bool
MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len);

/// @brief Dynamic version of MTY_MultiToWide.
/// @param src Source UTF-8 string.
/// @returns The returned buffer must be destroyed with MTY_Free.
MTY_EXPORT wchar_t *
MTY_MultiToWideD(const char *src);

/// @brief Thread local version of MTY_MultiToWide.
/// @param src Source UTF-8 string.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const wchar_t *
MTY_MultiToWideDL(const char *src);

/// @brief Get the bytes of a 16-bit integer in reverse order.
/// @param value Value to swap.
MTY_EXPORT uint16_t
MTY_Swap16(uint16_t value);

/// @brief Get the bytes of a 32-bit integer in reverse order.
/// @param value Value to swap.
MTY_EXPORT uint32_t
MTY_Swap32(uint32_t value);

/// @brief Get the bytes of a 64-bit integer in reverse order.
/// @param value Value to swap.
MTY_EXPORT uint64_t
MTY_Swap64(uint64_t value);

/// @brief Get the big-endian representation of a native 16-bit integer.
/// @param value Native value to swap.
MTY_EXPORT uint16_t
MTY_SwapToBE16(uint16_t value);

/// @brief Get the big-endian representation of a native 32-bit integer.
/// @param value Native value to swap.
MTY_EXPORT uint32_t
MTY_SwapToBE32(uint32_t value);

/// @brief Get the big-endian representation of a native 64-bit integer.
/// @param value Native value to swap.
MTY_EXPORT uint64_t
MTY_SwapToBE64(uint64_t value);

/// @brief Get the native representation of a big-endian 16-bit integer.
/// @param value Big-endian value to swap.
MTY_EXPORT uint16_t
MTY_SwapFromBE16(uint16_t value);

/// @brief Get the native representation of a big-endian 32-bit integer.
/// @param value Big-endian value to swap.
MTY_EXPORT uint32_t
MTY_SwapFromBE32(uint32_t value);

/// @brief Get the native representation of a big-endian 64-bit integer.
/// @param value Big-endian value to swap.
MTY_EXPORT uint64_t
MTY_SwapFromBE64(uint64_t value);


//- #module Thread
//- #mbrief Thread creation and synchronization, atomics.
//- #mdetails You should have a solid understanding of multithreaded programming
//-   before using any of these functions. This module serves as a cross-platform
//-   wrapper around POSIX `pthreads` and Windows' critical sections, condition
//-   variables, and slim reader/writer (SRW) locks.
//- #msupport Windows macOS Android Linux

typedef struct MTY_Thread MTY_Thread;
typedef struct MTY_Mutex MTY_Mutex;
typedef struct MTY_Cond MTY_Cond;
typedef struct MTY_RWLock MTY_RWLock;
typedef struct MTY_Waitable MTY_Waitable;
typedef struct MTY_ThreadPool MTY_ThreadPool;

/// @brief Function that takes a single opaque argument.
/// @param opaque Pointer set via various MTY_ThreadPool related functions.
typedef void (*MTY_AnonFunc)(void *opaque);

/// @brief Function that is executed on a thread.
/// @param opaque Pointer set via MTY_ThreadCreate or MTY_ThreadDetach.
/// @returns An opaque pointer that gets returned by MTY_ThreadDestroy if the thread
///   has not been run as detached.
typedef void *(*MTY_ThreadFunc)(void *opaque);

/// @brief Status of an asynchronous task.
typedef enum {
	MTY_ASYNC_OK       = 0, ///< The task has completed and the result is ready.
	MTY_ASYNC_DONE     = 1, ///< There is no task in progress.
	MTY_ASYNC_CONTINUE = 2, ///< The task is currently in progress.
	MTY_ASYNC_ERROR    = 3, ///< The task has failed.
	MTY_ASYNC_MAKE_32  = INT32_MAX,
} MTY_Async;

/// @brief 32-bit integer used for atomic operations.
typedef struct {
	volatile int32_t value; ///< 32-bit value wrapped in a struct for alignment.
} MTY_Atomic32;

/// @brief 64-bit integer used for atomic operations.
typedef struct {
	volatile int64_t value; ///< 64-bit integer wrapped in a struct for alignment.
} MTY_Atomic64;

/// @brief Create an MTY_Thread that executes asynchronously.
/// @param func Function that executes on its own thread.
/// @param opaque Passed to `func` when it is called.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_Thread must be destroyed with MTY_ThreadDestroy.
MTY_EXPORT MTY_Thread *
MTY_ThreadCreate(MTY_ThreadFunc func, void *opaque);

/// @brief Wait until an MTY_Thread has finished executing then destroy it.
/// @details Otherwise known as "joining" a thread.
/// @param thread Passed by reference and set to NULL after being destroyed.
/// @returns The return value from `func` set via MTY_ThreadCreate.
MTY_EXPORT void *
MTY_ThreadDestroy(MTY_Thread **thread);

/// @brief Asynchronously run a function and forgo the ability to query its result.
/// @param func Function that executes on its own thread.
/// @param opaque Passed to `func` when it is called.
MTY_EXPORT void
MTY_ThreadDetach(MTY_ThreadFunc func, void *opaque);

/// @brief Get a thread's `id`.
/// @param ctx An MTY_Thread.
MTY_EXPORT int64_t
MTY_ThreadGetID(MTY_Thread *ctx);

/// @brief Create an MTY_Mutex for synchronization.
/// @details A mutex can be locked by only one thread at a time. Other threads trying
///   to take the same mutex will block until it becomes unlocked.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_Mutex must be destroyed with MTY_MutexDestroy.
MTY_EXPORT MTY_Mutex *
MTY_MutexCreate(void);

/// @brief Destroy an MTY_Mutex.
/// @param mutex Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_MutexDestroy(MTY_Mutex **mutex);

/// @brief Wait to acquire a lock on the mutex.
/// @param ctx An MTY_Mutex.
MTY_EXPORT void
MTY_MutexLock(MTY_Mutex *ctx);

/// @brief Try to acquire a lock on a mutex but fail if doing so would block.
/// @param ctx An MTY_Mutex.
/// @returns If the lock was acquired, returns true, otherwise false.
MTY_EXPORT bool
MTY_MutexTryLock(MTY_Mutex *ctx);

/// @brief Unlock a mutex.
/// @param ctx An MTY_Mutex.
MTY_EXPORT void
MTY_MutexUnlock(MTY_Mutex *ctx);

/// @brief Create an MTY_Cond (condition variable) for signaling between threads.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_Cond must be destroyed with MTY_CondDestroy.
MTY_EXPORT MTY_Cond *
MTY_CondCreate(void);

/// @brief Destroy an MTY_Cond.
/// @param cond Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_CondDestroy(MTY_Cond **cond);

/// @brief Wait for a condition variable to be signaled with a timeout.
/// @param ctx An MTY_Cond condition variable.
/// @param mutex An MTY_Mutex that should be locked before the call to this function.
///   When this function is called, the mutex will be unlocked while it is waiting,
///   then the lock will be reacquired after this function returns.
/// @param timeout Time to wait in milliseconds for the condition variable to be
///   signaled. A negative value will not timeout.
/// @returns If signaled, returns true, otherwise false on timeout.
MTY_EXPORT bool
MTY_CondWait(MTY_Cond *ctx, MTY_Mutex *mutex, int32_t timeout);

/// @brief Signal a condition variable on a single blocked thread.
/// @param ctx An MTY_Cond condition variable.
MTY_EXPORT void
MTY_CondSignal(MTY_Cond *ctx);

/// @brief Signal a condition variable on all blocked threads.
/// @param ctx An MTY_Cond condition variable.
MTY_EXPORT void
MTY_CondSignalAll(MTY_Cond *ctx);

/// @brief Create an MTY_RWLock that allows concurrent read access.
/// @details An MTY_RWLock allows recursive locking from readers, and will prioritize
///   writers when they are waiting.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_RWLock must be destroyed with MTY_RWLockDestroy.
MTY_EXPORT MTY_RWLock *
MTY_RWLockCreate(void);

/// @brief Destroy an MTY_RWLock.
/// @param rwlock Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_RWLockDestroy(MTY_RWLock **rwlock);

/// @brief Try to acquire a read lock on an MTY_RWLock but fail if doing so would block.
/// @param ctx An MTY_RWLock.
/// @returns If the lock was acquired, returns true, otherwise false.
MTY_EXPORT bool
MTY_RWTryLockReader(MTY_RWLock *ctx);

/// @brief Acquire a read lock on an MTY_RWLock, blocking other writers.
/// @param ctx An MTY_RWLock.
MTY_EXPORT void
MTY_RWLockReader(MTY_RWLock *ctx);

/// @brief Acquire a write lock on an MTY_RWLock, block other readers and writers.
/// @param ctx An MTY_RWLock.
MTY_EXPORT void
MTY_RWLockWriter(MTY_RWLock *ctx);

/// @brief Unlock an MTY_RWLock.
/// @param ctx An MTY_RWLock.
MTY_EXPORT void
MTY_RWLockUnlock(MTY_RWLock *ctx);

/// @brief Create an MTY_Waitable object.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_Waitable object must be destroyed with MTY_WaitableDestroy.
MTY_EXPORT MTY_Waitable *
MTY_WaitableCreate(void);

/// @brief Destroy an MTY_Waitable.
/// @param waitable Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_WaitableDestroy(MTY_Waitable **waitable);

/// @brief Wait for a waitable object to be signaled with a timeout.
/// @details If MTY_WaitableSignal is called on the waitable object, this function
///   will return true for the next thread that calls this function, even if the signal
///   happens while the waitable object is not actively waiting. If multiple threads
///   are waiting on the waitable object, exactly one thread will become unblocked and
///   return true, the others will continue waiting.
/// @param ctx An MTY_Waitable object.
/// @param timeout Time to wait in milliseconds for the waitable object to be signaled.
///   A negative value will not timeout.
/// @returns If signaled, returns true, otherwise false on timeout.
MTY_EXPORT bool
MTY_WaitableWait(MTY_Waitable *ctx, int32_t timeout);

/// @brief Signal a waitable object on a single blocked thread.
/// @details If multiple threads are waiting on the waitable object, exactly one thread
///   will become unblocked and return true, the others will continue waiting.
/// @param ctx An MTY_Waitable object.
MTY_EXPORT void
MTY_WaitableSignal(MTY_Waitable *ctx);

/// @brief Create an MTY_ThreadPool for asynchronously executing tasks.
/// @param maxThreads Maximum number of threads that can be simultaneously executing.
/// @returns This function can not return NULL. It will call `abort()` on failure.\n\n
///   The returned MTY_ThreadPool object must be destroyed with MTY_ThreadPoolDestroy.
MTY_EXPORT MTY_ThreadPool *
MTY_ThreadPoolCreate(uint32_t maxThreads);

/// @brief Destroy an MTY_ThreadPool.
/// @param pool Passed by reference and set to NULL after being destroyed.
/// @param detach Function called to clean up `opaque` thread state set via
///   MTY_ThreadPoolDispatch after threads are done executing. May be NULL if not
///   applicable.
MTY_EXPORT void
MTY_ThreadPoolDestroy(MTY_ThreadPool **pool, MTY_AnonFunc detach);

/// @brief Dispatch a function to the thread pool.
/// @param ctx An MTY_ThreadPool.
/// @param func Function executed on a thread in the pool.
/// @param opaque Passed to `func` when it is called.
/// @returns On success, the index of the scheduled thread which must be greater than 0.
///   If there is no room left in the pool, 0 is returned. Call MTY_GetLog for details.
MTY_EXPORT uint32_t
MTY_ThreadPoolDispatch(MTY_ThreadPool *ctx, MTY_AnonFunc func, void *opaque);

/// @brief Allow a thread to be reused after it has finished executing.
/// @details This function may be called while the thread is executing or after it has
///   already been set to MTY_ASYNC_OK. In either case, the thread is allowed to be
///   reused via MTY_ThreadPoolDispatch as soon as it is no longer executing.
/// @param ctx An MTY_ThreadPool.
/// @param index Thread index returned by MTY_ThreadPoolCreate.
/// @param detach Function called to clean up `opaque` thread state set via
///   MTY_ThreadPoolDispatch after the thread is done executing. May be NULL if not
///   applicable.
MTY_EXPORT void
MTY_ThreadPoolDetach(MTY_ThreadPool *ctx, uint32_t index, MTY_AnonFunc detach);

/// @brief Poll the asynchronous state of a thread in a pool.
/// @param ctx An MTY_ThreadPool.
/// @param index Thread index returned by MTY_ThreadPoolCreate.
/// @param opaque Thread state set via the `opaque` argument to MTY_ThreadPoolCreate.
/// @returns MTY_ASYNC_OK means the request has finished and `opaque` has been set to
///   the value originally passed via MTY_ThreadPoolDispatch.\n\n
///   MTY_ASYNC_DONE means there is no thread running at `index`.\n\n
///   MTY_ASYNC_CONTINUE means the thread is still executing.
MTY_EXPORT MTY_Async
MTY_ThreadPoolPoll(MTY_ThreadPool *ctx, uint32_t index, void **opaque);

/// @brief Set a 32-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic32.
/// @param value Value to atomically set.
MTY_EXPORT void
MTY_Atomic32Set(MTY_Atomic32 *atomic, int32_t value);

/// @brief Set a 64-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic64.
/// @param value Value to atomically set.
MTY_EXPORT void
MTY_Atomic64Set(MTY_Atomic64 *atomic, int64_t value);

/// @brief Get a 32-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic32.
MTY_EXPORT int32_t
MTY_Atomic32Get(MTY_Atomic32 *atomic);

/// @brief Get a 64-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic64.
MTY_EXPORT int64_t
MTY_Atomic64Get(MTY_Atomic64 *atomic);

/// @brief Add to a 32-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic32.
/// @param value Value to atomically add. This value can be negative, effectively
///   performing subtraction.
/// @returns The result of the addition.
MTY_EXPORT int32_t
MTY_Atomic32Add(MTY_Atomic32 *atomic, int32_t value);

/// @brief Add to a 64-bit integer atomically.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic64.
/// @param value Value to atomically add. This value can be negative, effectively
///   performing subtraction.
/// @returns The result of the addition.
MTY_EXPORT int64_t
MTY_Atomic64Add(MTY_Atomic64 *atomic, int64_t value);

/// @brief Compare two 32-bit values and if the same, atomically set to a new value.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic32.
/// @param oldValue Value to compare against the atomic.
/// @param newValue Value the atomic is set to if `oldValue` matches the atomic.
/// @returns If the atomic is set to `newValue`, returns true, otherwise false.
MTY_EXPORT bool
MTY_Atomic32CAS(MTY_Atomic32 *atomic, int32_t oldValue, int32_t newValue);

/// @brief Compare two 64-bit values and if the same, atomically set to a new value.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param atomic An MTY_Atomic64.
/// @param oldValue Value to compare against the atomic.
/// @param newValue Value the atomic is set to if `oldValue` matches the atomic.
/// @returns If the atomic is set to `newValue`, returns true, otherwise false.
MTY_EXPORT bool
MTY_Atomic64CAS(MTY_Atomic64 *atomic, int64_t oldValue, int64_t newValue);

/// @brief Globally lock via an atomic.
/// @details All atomic operations in libmatoya create a full memory barrier.\n\n
///   Warning: There is a process wide maximum of UINT8_MAX global locks.\n\n
///   The global lock should be statically initialized to zero.
/// @param lock An MTY_Atomic32.
MTY_EXPORT void
MTY_GlobalLock(MTY_Atomic32 *lock);

/// @brief Globally unlock via an atomic.
/// @details All atomic operations in libmatoya create a full memory barrier.
/// @param lock An MTY_Atomic32.
MTY_EXPORT void
MTY_GlobalUnlock(MTY_Atomic32 *lock);


//- #module Net
//- #mbrief HTTP/HTTPS, WebSocket support.
//- #mdetails These functions are capable of making secure connections.

#define MTY_URL_MAX 1024       ///< Maximum size of a URL used internally by libmatoya.
#define MTY_RES_MAX 0x40000000 ///< Maximum size of an HTTP response that can be read by libmatoya.

typedef struct MTY_WebSocket MTY_WebSocket;

/// @brief Make a synchronous HTTP request.
/// @details Only `Content-Encoding: gzip` is supported for compression.
/// @param url The URL for the request, the scheme must be either `http` or `https`.
/// @param method The HTTP method, i.e. `GET` or `POST`.
/// @param headers HTTP header key/value pairs in the format `Key:Value` separated by
///   newline characters.\n\n
///   May be NULL for no additional headers.
/// @param body Request payload.
/// @param bodySize Size in bytes of `body`.
/// @param proxy The proxy URL including the port, i.e. `http://example.com:1337`, or NULL
///   to use the OS's default proxy.
/// @param timeout Time to wait in milliseconds for completion.
/// @param response Set to the response body, or NULL if there is no response body.
/// @param responseSize Set to the size of `response`, or 0 if there is no response body.
/// @param status The HTTP response status code.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.\n\n
///   If successful, `response` is dynamically allocated and must be destroyed with
///   MTY_Free.
MTY_EXPORT bool
MTY_HttpRequest(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout,
	void **response, size_t *responseSize, uint16_t *status);

/// @brief Create a global asynchronous HTTP thread pool.
/// @param maxThreads Maximum number of threads that can be simultaneously making
///   requests.
MTY_EXPORT void
MTY_HttpAsyncCreate(uint32_t maxThreads);

/// @brief Destroy the global HTTP thread pool.
MTY_EXPORT void
MTY_HttpAsyncDestroy(void);

/// @brief Dispatch an HTTP request to the global HTTP thread pool.
/// @param index The thread index to dispatch the request on. This variable acts as
///   a pseudo context as it should be initialized to 0, then is set to a new index
///   internally. If a request is dispatched using an index already executing, the
///   thread is automatically cleared and the new request is dispatched to a different
///   thread.
/// @param url The URL for the request, the scheme must be either `http` or `https`.
/// @param method The HTTP method, i.e. `GET` or `POST`.
/// @param headers HTTP header key/value pairs in the format `Key:Value` separated by
///   newline characters.\n\n
///   May be NULL for no additional headers.
/// @param body Request payload.
/// @param bodySize Size in bytes of `body`.
/// @param proxy The proxy URL including the port, i.e. `http://example.com:1337`, or NULL
///   to use the OS's default proxy.
/// @param timeout Time the thread will wait in milliseconds for completion.
/// @param image Attempt to decompress an image response. If successful, the `size` argument
///   supplied to MTY_HttpAsyncPoll will be set to `width | height << 16`.
MTY_EXPORT void
MTY_HttpAsyncRequest(uint32_t *index, const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout, bool image);

/// @brief Poll the global HTTP thread pool for a response.
/// @param index The thread index acquired in MTY_HttpAsyncRequest.
/// @param response Set to the response body, or NULL if there is no response body.
/// @param size Set to the response body size, or 0 if there is no response body.
/// @param status Set to the HTTP response status code.
/// @returns MTY_ASYNC_OK means the request has finished and `response`, `size`, and
///   `status` are valid. These values will remain valid until MTY_HttpAsyncClear is
///   called on the thread index.\n\n
///   MTY_ASYNC_DONE means there is no request currently pending.\n\n
///   MTY_ASYNC_CONTINUE means the request is still in progress.\n\n
///   MTY_ASYNC_ERROR means an error has occurred. Call MTY_GetLog for details.
MTY_EXPORT MTY_Async
MTY_HttpAsyncPoll(uint32_t index, void **response, size_t *size, uint16_t *status);

/// @brief Free all resources associated with a thread in the global HTTP thread pool.
/// @details This function also allows the thread index to be reused.
/// @param index The thread index to clean up. Set to 0 before returning.
MTY_EXPORT void
MTY_HttpAsyncClear(uint32_t *index);

/// @brief Connect to a WebSocket endpoint.
/// @param url The URL for the WebSocket, the scheme must be either `ws` or `wss`.
/// @param headers HTTP header key/value pairs in the format `Key:Value` separated by
///   newline characters.\n\n
///   May be NULL for no additional headers.
/// @param proxy The proxy URL including the port, i.e. `http://example.com:1337`, or NULL
///   to use the OS's default proxy.
/// @param timeout Time to wait in milliseconds for connection.
/// @param upgradeStatus Set to the HTTP response status code of the WebSocket upgrade
///   request. This value is set even on failure.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_WebSocket must be destroyed with MTY_WebSocketDestroy.
MTY_EXPORT MTY_WebSocket *
MTY_WebSocketConnect(const char *url, const char *headers, const char *proxy,
	uint32_t timeout, uint16_t *upgradeStatus);

/// @brief Destroy a WebSocket.
/// @param webSocket Passed by reference and set to NULL after being destroyed.\n\n
///   This function will attempt to gracefully close the connection with close
///   code `1000`.
MTY_EXPORT void
MTY_WebSocketDestroy(MTY_WebSocket **webSocket);

/// @brief Read a message from a WebSocket.
/// @param ctx An MTY_WebSocket.
/// @param timeout Time to wait in milliseconds for a message to become available.
/// @param msg Output message buffer.
/// @param size Size in bytes of `msg`.
/// @returns MTY_ASYNC_OK means a message has been successfully read into `msg`.\n\n
///   MTY_ASYNC_CONTINUE means the `timeout` has been reached without a message.\n\n
///   MTY_ASYNC_ERROR means an error has occurred. Call MTY_GetLog for details.
MTY_EXPORT MTY_Async
MTY_WebSocketRead(MTY_WebSocket *ctx, uint32_t timeout, char *msg, size_t size);

/// @brief Write a message to a WebSocket.
/// @param ctx An MTY_WebSocket.
/// @param msg The string message to send.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_WebSocketWrite(MTY_WebSocket *ctx, const char *msg);

/// @brief Get the 16-bit close code after a WebSocket connection has been terminated.
/// @param ctx An MTY_WebSocket that is in the closed state.
/// @returns If the WebSocket has not received a close message, this function will
///   return 0.
MTY_EXPORT uint16_t
MTY_WebSocketGetCloseCode(MTY_WebSocket *ctx);


//- #module Struct
//- #mbrief Simple data structures.

typedef struct MTY_Hash MTY_Hash;
typedef struct MTY_Queue MTY_Queue;
typedef struct MTY_List MTY_List;

/// @brief Function that frees resources you allocated within a data structure.
/// @param ptr Pointer set via MTY_HashSet et al.
typedef void (*MTY_FreeFunc)(void *ptr);

/// @brief Node in a linked list.
typedef struct MTY_ListNode {
	struct MTY_ListNode *prev; ///< The previous node in the list.
	struct MTY_ListNode *next; ///< The next node in the list.
	void *value;               ///< The value associated with the node.
} MTY_ListNode;

/// @brief Create an MTY_Hash for key/value lookup.
/// @param numBuckets The number of buckets to use. The more buckets, the larger
///   the memory usage but less chance of collision. Specifying 0 chooses a reasonable
///   default.
/// @returns The returned MTY_Hash must be destroyed with MTY_HashDestroy.
MTY_EXPORT MTY_Hash *
MTY_HashCreate(uint32_t numBuckets);

/// @brief Destroy an MTY_Hash.
/// @param hash Passed by reference and set to NULL after being destroyed.
/// @param freeFunc Function called on each remaining value in the hash to give you
///   the opportunity to free resources. This may be NULL if it is unnecessary.
MTY_EXPORT void
MTY_HashDestroy(MTY_Hash **hash, MTY_FreeFunc freeFunc);

/// @brief Get a value from a hash by string key.
/// @param ctx An MTY_Hash.
/// @param key String key to lookup.
/// @returns The value associated with `key`, otherwise NULL.
MTY_EXPORT void *
MTY_HashGet(MTY_Hash *ctx, const char *key);

/// @brief Get a value from a hash by integer key.
/// @param ctx An MTY_Hash.
/// @param key Integer key to lookup.
/// @returns The value associated with `key`, otherwise NULL.
MTY_EXPORT void *
MTY_HashGetInt(MTY_Hash *ctx, int64_t key);

/// @brief Set a value in a hash by string key.
/// @param ctx An MTY_Hash.
/// @param key String key to set.
/// @param value Value to set associated with `key`.
/// @returns If `key` already exists, the previous value is returned,
///   otherwise NULL is returned.
MTY_EXPORT void *
MTY_HashSet(MTY_Hash *ctx, const char *key, void *value);

/// @brief Set a value in a hash by integer key.
/// @param ctx An MTY_Hash.
/// @param key Integer key to set.
/// @param value Value to set associated with `key`.
/// @returns If `key` already exists, the previous value is returned,
///   otherwise NULL is returned.
MTY_EXPORT void *
MTY_HashSetInt(MTY_Hash *ctx, int64_t key, void *value);

/// @brief Get and remove a value from a hash by string key.
/// @param ctx An MTY_Hash.
/// @param key String key to lookup and remove.
/// @returns The value associated with `key`, otherwise NULL.
MTY_EXPORT void *
MTY_HashPop(MTY_Hash *ctx, const char *key);

/// @brief Get and remove a value from a hash by integer key.
/// @param ctx An MTY_Hash.
/// @param key Integer key to lookup and remove.
/// @returns The value associated with `key`, otherwise NULL.
MTY_EXPORT void *
MTY_HashPopInt(MTY_Hash *ctx, int64_t key);

/// @brief Iterate through string key/value pairs in a hash.
/// @param ctx An MTY_Hash.
/// @param iter Iterator that keeps track of the position in the hash. Set this to
///   0 before the fist call to this function.
/// @param key Reference to the next string key in the hash.
/// @returns Returns true if there are more keys available, otherwise false.
MTY_EXPORT bool
MTY_HashGetNextKey(MTY_Hash *ctx, uint64_t *iter, const char **key);

/// @brief Iterate through integer key/value pairs in a hash.
/// @param ctx An MTY_Hash.
/// @param iter Iterator that keeps track of the position in the hash. Set this to
///   0 before the fist call to this function.
/// @param key Reference to the next integer key in the hash.
/// @returns Returns true if there are more keys available, otherwise false.
MTY_EXPORT bool
MTY_HashGetNextKeyInt(MTY_Hash *ctx, uint64_t *iter, int64_t *key);

/// @brief Create an MTY_Queue for thread safe serialization.
/// @details The queue is a multi-producer single-consumer style queue, meaning
///   multiple threads can submit to the queue safely, but only a single thread can
///   pop from it.
/// @param len The number of buffers in the queue.
/// @param bufSize The preallocated size of each buffer in the queue. If only pushing
///   via MTY_QueuePushPtr, this can be set to 0.
/// @returns The returned MTY_Queue must be destroyed with MTY_QueueDestroy.
MTY_EXPORT MTY_Queue *
MTY_QueueCreate(uint32_t len, size_t bufSize);

/// @brief Destroy an MTY_Queue.
/// @param queue Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_QueueDestroy(MTY_Queue **queue);

/// @brief Get the current number of items in the queue.
/// @details Calling this function doesn't lock the queue, so it can be viewed as
///   an approximation while there is activity.
/// @param ctx An MTY_Queue.
MTY_EXPORT uint32_t
MTY_QueueGetLength(MTY_Queue *ctx);

/// @brief Lock and retrieve the next available input buffer from the queue.
/// @param ctx An MTY_Queue.
/// @returns If there are no input buffers available, NULL is returned.
MTY_EXPORT void *
MTY_QueueGetInputBuffer(MTY_Queue *ctx);

/// @brief Push and unlock the most recently acquired input buffer.
/// @param ctx An MTY_Queue.
/// @param size The amount of data filled in the most recently locked buffer. If this
///   is 0, the previous buffer is immediately released and internally set to empty.
MTY_EXPORT void
MTY_QueuePush(MTY_Queue *ctx, size_t size);

/// @brief Lock and retrieve the next available output buffer from the queue.
/// @param ctx An MTY_Queue.
/// @param timeout Time to wait in milliseconds for an output buffer to become available.
///   A negative value will not timeout.
/// @param buffer Reference to the next output buffer.
/// @param size Set to the size of the data available in `buffer`. May be NULL.
/// @returns Returns true if an output buffer was acquired, false on timeout.
MTY_EXPORT bool
MTY_QueueGetOutputBuffer(MTY_Queue *ctx, int32_t timeout, void **buffer, size_t *size);

/// @brief Lock and retrieve the most recently pushed output buffer from the queue.
/// @details This will discard all other buffers in the queue besides the most recent.
/// @param ctx An MTY_Queue.
/// @param timeout Time to wait in milliseconds for an output buffer to become available.
///   A negative value will not timeout.
/// @param buffer Reference to the output buffer.
/// @param size Set to the size of the data filled in `buffer`.
/// @returns Returns true if an output buffer was acquired, false on timeout.
MTY_EXPORT bool
MTY_QueueGetLastOutputBuffer(MTY_Queue *ctx, int32_t timeout, void **buffer,
	size_t *size);

/// @brief Unlock the most recently acquired output buffer and mark it as empty.
/// @param ctx An MTY_Queue.
MTY_EXPORT void
MTY_QueuePop(MTY_Queue *ctx);

/// @brief Push a pointer allocated by the caller to a queue.
/// @param ctx An MTY_Queue.
/// @param opaque Value you allocated and are responsible for freeing.
/// @param size Size in bytes of `opaque`. This value is returned by MTY_QueuePopPtr.
/// @returns Returns true if `opaque` was successfully pushed to the queue, otherwise
///   false if there was no input buffer available.
MTY_EXPORT bool
MTY_QueuePushPtr(MTY_Queue *ctx, void *opaque, size_t size);

/// @brief Pop a pointer allocated by the caller from a queue.
/// @param ctx An MTY_Queue.
/// @param timeout Time to wait in milliseconds for the next pointer to become available.
///   A negative value will not timeout.
/// @param opaque Reference to a pointer set via MTY_QueuePushPtr.
/// @param size Set to the `size` passed to MTY_QueuePushPtr. May be NULL.
/// @returns Returns true if an `opaque` was successfully popped from the queue,
///   otherwise false on timeout.
MTY_EXPORT bool
MTY_QueuePopPtr(MTY_Queue *ctx, int32_t timeout, void **opaque, size_t *size);

/// @brief Set all buffers in a queue as empty.
/// @param ctx An MTY_Queue.
/// @param freeFunc Function called on each user allocated value in the queue to
///   give you the opportunity to free resources. This may be NULL if it is unnecessary.
MTY_EXPORT void
MTY_QueueFlush(MTY_Queue *ctx, MTY_FreeFunc freeFunc);

/// @brief Create an MTY_List for a flexibly sized array.
/// @returns The returned MTY_List must be destroyed with MTY_ListDestroy.\n\n
///   Only the first node in the list should be passed to MTY_ListDestroy.
MTY_EXPORT MTY_List *
MTY_ListCreate(void);

/// @brief Destroy an MTY_List.
/// @param queue Passed by reference and set to NULL after being destroyed.
/// @param freeFunc Function called on each remaining value in the list to give you
///   the opportunity to free resources. This may be NULL if it is unnecessary.
MTY_EXPORT void
MTY_ListDestroy(MTY_List **list, MTY_FreeFunc freeFunc);

/// @brief Get the first node in a list.
/// @param ctx An MTY_List.
/// @returns Only the first node in the list should be passed to MTY_ListDestroy.
MTY_EXPORT MTY_ListNode *
MTY_ListGetFirst(MTY_List *ctx);

/// @brief Append an item to a list.
/// @param ctx An MTY_List.
/// @param value Value to append.
MTY_EXPORT void
MTY_ListAppend(MTY_List *ctx, void *value);

/// @brief Remove a node from a list and return its item.
/// @param ctx An MTY_List.
/// @param node The node in the list that should be removed.
/// @returns The value associated with the removed `node`.
MTY_EXPORT void *
MTY_ListRemove(MTY_List *ctx, MTY_ListNode *node);


//- #module System
//- #mbrief Functions related to the OS and current process.

typedef struct MTY_SO MTY_SO;

/// @brief Function called when the process is about to terminate.
/// @param forced The user intentionally forced termination.
/// @param opaque Pointer set via MTY_SetCrashFunc.
typedef void (*MTY_CrashFunc)(bool forced, void *opaque);

/// @brief Operating systems.
typedef enum {
	MTY_OS_UNKNOWN = 0x00000000, ///< Unable to detect the operating system.
	MTY_OS_WINDOWS = 0x01000000, ///< Microsoft Windows.
	MTY_OS_MACOS   = 0x02000000, ///< Apple macOS.
	MTY_OS_ANDROID = 0x04000000, ///< Android.
	MTY_OS_LINUX   = 0x08000000, ///< Linux with X11 windowing system.
	MTY_OS_WEB     = 0x10000000, ///< Browser environment.
	MTY_OS_IOS     = 0x20000000, ///< Apple iOS.
	MTY_OS_TVOS    = 0x40000000, ///< Apple tvOS.
	MTY_OS_MAKE_32 = INT32_MAX,
} MTY_OS;

/// @brief Dynamically load a shared object.
/// @details This function wraps `dlopen` on Unix and `LoadLibrary` on Windows.
/// @param path Path to the shared object. This can simply be the name of shared
///   object if it is in one of the default library search paths.
/// @returns If the shared object can not be loaded or is not found, NULL is returned.
///   Call MTY_GetLog for details.\n\n
///   The returned MTY_SO must be unloaded with MTY_SOUnload.
//- #support Windows macOS Android Linux
MTY_EXPORT MTY_SO *
MTY_SOLoad(const char *path);

/// @brief Get a symbol from a shared object.
/// @param so An MTY_SO.
/// @param name The name of the symbol, i.e. `malloc`.
/// @returns A pointer to the symbol retrieved from the shared object, or NULL if the
///   symbol was not found. This symbol is only valid as long as the MTY_SO is loaded.
//- #support Windows macOS Android Linux
MTY_EXPORT void *
MTY_SOGetSymbol(MTY_SO *so, const char *name);

/// @brief Unload a shared object.
/// @param so Passed by reference and set to NULL after being destroyed.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_SOUnload(MTY_SO **so);

/// @brief Get the platform's shared object file extension.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetSOExtension(void);

/// @brief Get the computer's hostname.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetHostname(void);

/// @brief Check if libmatoya is supported on the current platform.
MTY_EXPORT bool
MTY_IsSupported(void);

/// @brief Get the current platform.
/// @returns An MTY_OS value bitwise OR'd with the OS's major and minor version numbers.
///   The major version has a mask of `0xFF00` and the minor has a mask of `0xFF`. On
///   Android, only a single version number is reported, the API level, in the
///   position of the major number.
MTY_EXPORT uint32_t
MTY_GetPlatform(void);

/// @brief Get the current platform without considering MTY_OS_WEB.
/// @returns Same as MTY_GetPlatform, but MTY_OS_WEB is not possible. Instead, libmatoya
///   attempts to check the browser's environment for the actual current OS.
MTY_EXPORT uint32_t
MTY_GetPlatformNoWeb(void);

/// @brief Turn a platform integer into a readable string.
/// @param platform Platform integer returned by MTY_GetPlatform or
///   MTY_GetPlatformNoWeb. Bytes in the platform integer may be masked out to
///   omit components of the string, i.e. a mask of 0xFFFF will create a string
///   only with the version and not the OS, and a mask of 0xFF000000 will
///   create a string only with the OS.
/// @returns Example format would be `Windows 10.0`.\n\n
///   This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetPlatformString(uint32_t platform);

/// @brief Execute the default protocol handler for a given URI.
/// @param uri The resource to be handled, i.e. `C:\tmp.txt` or `http://google.com`.
/// @param token An optional `HANDLE` to a user's security token. This can be used
///   to open the resource as a different user. Windows Only.
MTY_EXPORT void
MTY_HandleProtocol(const char *uri, void *token);

/// @brief Get the full path to the current process including the executable name.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetProcessPath(void);

/// @brief Get the full base directory path to the current process executable.
/// @returns This buffer is allocated in thread local storage and must not be freed.
MTY_EXPORT const char *
MTY_GetProcessDir(void);

/// @brief Restart the current process.
/// @details For more information, see `execv` from the C standard library.
/// @param argv Arguments to set up a call to `execv`. This is an array of strings
///   that must have its last element set to NULL.
/// @returns On success this function does not return, otherwise it returns false.
///   Call MTY_GetLog for details.
//- #support Windows macOS Linux
MTY_EXPORT bool
MTY_RestartProcess(char * const *argv);

/// @brief Set a function to be called just before abnormal termination.
/// @details This will attempt to hook `SIGKILL`, `SIGTERM`, and any Windows
///   specific exception behavior that may trigger a crash.
/// @param func Function called on termination.
/// @param opaque Passed to `func` when it is called.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_SetCrashFunc(MTY_CrashFunc func, void *opaque);

/// @brief Open a console window that prints stdout and stderr.
/// @param title The title of the console window.
//- #support Windows
MTY_EXPORT void
MTY_OpenConsole(const char *title);

/// @brief Close an open console window.
//- #support Windows
MTY_EXPORT void
MTY_CloseConsole(void);

/// @brief Check if a registered application name is set to run on system startup.
/// @param name The application name set via MTY_SetRunOnStartup.
//- #support Windows
MTY_EXPORT bool
MTY_GetRunOnStartup(const char *name);

/// @brief Set an application to run on system startup.
/// @param name An arbitrary application name.
/// @param path Path to the executable that should be launched. May be NULL to remove
///   the application from system startup.
/// @param args Argument string for the executable specified in `path`. This may be
///   multiple command line arguments separated by spaces. May be NULL if `path` is also
///   NULL.
//- #support Windows
MTY_EXPORT void
MTY_SetRunOnStartup(const char *name, const char *path, const char *args);

/// @brief Get a pointer to the thread local `JNIEnv *` environment.
//- #support Android
MTY_EXPORT void *
MTY_GetJNIEnv(void);


//- #module DTLS
//- #mbrief DTLS protocol wrapper.
//- #mdetails This module performs no IO and acts as a DTLS engine that requires you
//-   to feed it input and in turn will output data suitable for sending over a network.
//- #msupport Windows Linux

#define MTY_FINGERPRINT_MAX 112 ///< Maximum size of the string set by MTY_CertGetFingerprint.

typedef struct MTY_Cert MTY_Cert;
typedef struct MTY_DTLS MTY_DTLS;

/// @brief Function called when DTLS handshake data is ready to be sent.
/// @param buf The outbound DTLS message.
/// @param size Size in bytes of `buf`.
/// @param opaque Pointer set via MTY_DTLSHandshake.
/// @returns Return true on success, false on failure. Returning false will cause
///   MTY_DTLSHandshake to return MTY_ASYNC_ERROR.
typedef bool (*MTY_DTLSWriteFunc)(const void *buf, size_t size, void *opaque);

/// @brief Create an MTY_Cert, a self-signed X.509 certificate.
/// @details This certificate is suitable for a WebRTC style peer-to-peer
///   negotiation.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_Cert must be destroyed with MTY_CertDestroy.
//- #support Windows Linux
MTY_EXPORT MTY_Cert *
MTY_CertCreate(void);

/// @brief Destroy an MTY_Cert.
/// @param cert Passed by reference and set to NULL after being destroyed.
//- #support Windows Linux
MTY_EXPORT void
MTY_CertDestroy(MTY_Cert **cert);

/// @brief Get the certificate's SHA-256 fingerprint.
/// @details This function will safely truncate overflows with a null character.
/// @param ctx An MTY_Cert.
/// @param fingerprint Output buffer to receive the fingerprint. This buffer can be
///   of size MTY_FINGERPRINT_MAX.
/// @param size Size in bytes of `fingerprint`.
//- #support Windows Linux
MTY_EXPORT void
MTY_CertGetFingerprint(MTY_Cert *ctx, char *fingerprint, size_t size);

/// @brief Create an MTY_DTLS context for secure data transfer.
/// @param cert An MTY_Cert to set during the handshake. May be NULL for no
///   client cert.
/// @param peerFingerprint Fingerprint string to verify the peer's cert. May be NULL
///   for no cert verification.
/// @param mtu Specify the UDP maximum transmission unit.
/// @returns On failure, NULL is returned. Call MTY_GetLog for details.\n\n
///   The returned MTY_DTLS context must be destroyed with MTY_DTLSDestroy.
MTY_EXPORT MTY_DTLS *
MTY_DTLSCreate(MTY_Cert *cert, const char *peerFingerprint, uint32_t mtu);

/// @brief Destroy an MTY_DTLS context.
/// @param dtls Passed by reference and set to NULL after being destroyed.
MTY_EXPORT void
MTY_DTLSDestroy(MTY_DTLS **dtls);

/// @brief Perform the next step in the DTLS handshake.
/// @details This function should be called in a loop, feeding in DTLS messages
///   received from the host and sending output data to the host via `writeFunc`.
/// @param ctx An MTY_DTLS context.
/// @param buf Input buffer with a DTLS message received from the host. May be NULL
///   on the first call to this function to generate the Client Hello.
/// @param size Size in bytes of `buf`.
/// @param writeFunc Function called when output data is ready to be sent to the host.
/// @param opaque Passed to `writeFunc` when it is called.
/// @returns MTY_ASYNC_OK means the handshake has completed successfully.\n\n
///   MTY_ASYNC_CONTINUE means the handshake needs more data to complete.\n\n
///   MTY_ASYNC_ERROR means an error has occurred. Call MTY_GetLog for details.
MTY_EXPORT MTY_Async
MTY_DTLSHandshake(MTY_DTLS *ctx, const void *buf, size_t size, MTY_DTLSWriteFunc writeFunc,
	void *opaque);

/// @brief Encrypt data with the current DTLS context.
/// @param ctx An MTY_DTLS context.
/// @param in Input plain text data.
/// @param inSize Size in bytes of `in`.
/// @param out Output encrypted DTLS message.
/// @param outSize Size in bytes of `out`.
/// @param written Set to the number of bytes written to `out`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_DTLSEncrypt(MTY_DTLS *ctx, const void *in, size_t inSize, void *out, size_t outSize,
	size_t *written);

/// @brief Decrypt data with the current DTLS context.
/// @param ctx An MTY_DTLS context.
/// @param in Input DTLS message.
/// @param inSize Size in bytes of `in`.
/// @param out Output decrypted plain text.
/// @param outSize Size in bytes of `out`.
/// @param read Set to the number of bytes written to `out`.
/// @returns Returns true on success, false on failure. Call MTY_GetLog for details.
MTY_EXPORT bool
MTY_DTLSDecrypt(MTY_DTLS *ctx, const void *in, size_t inSize, void *out, size_t outSize,
	size_t *read);

/// @brief Check if a buffer is a DTLS 1.2 handshake message.
/// @param buf Input buffer.
/// @param size Size in bytes of `buf`.
MTY_EXPORT bool
MTY_IsDTLSHandshake(const void *buf, size_t size);

/// @brief Check if a buffer is a DTLS 1.2 application data message.
/// @param buf Input buffer.
/// @param size Size in bytes of `buf`.
MTY_EXPORT bool
MTY_IsDTLSApplicationData(const void *buf, size_t size);


//- #module Time
//- #mbrief High precision time stamps. Sleep.

typedef int64_t MTY_Time;

/// @brief Get a high precision time stamp.
/// @details This value has at least microsecond precision.
MTY_EXPORT MTY_Time
MTY_GetTime(void);

/// @brief Get the difference between two MTY_Time stamps in milliseconds.
/// @param begin The beginning time stamp.
/// @param end The ending time stamp.
/// @returns This value can be negative if `begin > end`.
MTY_EXPORT double
MTY_TimeDiff(MTY_Time begin, MTY_Time end);

/// @brief Suspend the current thread.
/// @param timeout The number of milliseconds to sleep.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_Sleep(uint32_t timeout);

/// @brief Suspend the current thread with high precision.
/// @details This function uses a spinlock at the end of the sleep to get closer to the
///   requested `timeout`. Depending on the value of `spin`, it can consume much more of the CPU
///   than MTY_Sleep.
/// @param timeout The total number of milliseconds to sleep.
/// @param spin When less than `spin` milliseconds remain in the sleep, fall into a spinlock.
//- #support Windows macOS Android Linux
MTY_EXPORT void
MTY_PreciseSleep(double timeout, double spin);

/// @brief Set the sleep precision of all waitable objects.
/// @details See `timeBeginPeriod` on Windows.
/// @param res The desired precision in milliseconds. This can not be less than 1.
//- #support Windows
MTY_EXPORT void
MTY_SetTimerResolution(uint32_t res);

/// @brief Revert the precision set via MTY_SetTimerResolution.
/// @details See `timeEndPeriod` on Windows.
/// @param res The value used for the most recent call to MTY_SetTimerResolution.
//- #support Windows
MTY_EXPORT void
MTY_RevertTimerResolution(uint32_t res);


//- #module Version
//- #mbrief libmatoya version information.
//- #mdetails libmatoya has two version numbers, MTY_VERSION_MAJOR and MTY_VERSION_MINOR.
//-   A minor version increase means an implementation change or a pure addition to the
//-   interface. A major version increase means the interface has changed.

#define MTY_VERSION_MAJOR   4      ///< libmatoya major version number.
#define MTY_VERSION_MINOR   0      ///< libmatoya minor version number.
#define MTY_VERSION_STRING  "4.0"  ///< UTF-8 libmatoya version string.
#define MTY_VERSION_STRINGW L"4.0" ///< Wide character libmatoya version string.

/// @brief Get the version libmatoya was compiled with.
/// @returns The major and minor version numbers packed into a 32-bit integer. The major
///   version has a mask of `0xFF00` and the minor has a mask of `0xFF`.
MTY_EXPORT uint32_t
MTY_GetVersion(void);


#ifdef __cplusplus
}
#endif
