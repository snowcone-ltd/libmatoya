// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"


// Carbon private interface

#define kVK_WinApp 0x6E

enum {
	NS_MOD_LCTRL  = 0x000001,
	NS_MOD_LSHIFT = 0x000002,
	NS_MOD_RSHIFT = 0x000004,
	NS_MOD_LCMD   = 0x000008,
	NS_MOD_RCMD   = 0x000010,
	NS_MOD_LALT   = 0x000020,
	NS_MOD_RALT   = 0x000040,
	NS_MOD_RCTRL  = 0x002000,
};


// Keymap

static MTY_Mod keymap_modifier_flags_to_keymod(NSEventModifierFlags flags)
{
	MTY_Mod mod = 0;

	mod |= (flags & NS_MOD_LCTRL) ? MTY_MOD_LCTRL : 0;
	mod |= (flags & NS_MOD_RCTRL) ? MTY_MOD_RCTRL : 0;
	mod |= (flags & NS_MOD_LSHIFT) ? MTY_MOD_LSHIFT : 0;
	mod |= (flags & NS_MOD_RSHIFT) ? MTY_MOD_RSHIFT : 0;
	mod |= (flags & NS_MOD_LCMD) ? MTY_MOD_LWIN : 0;
	mod |= (flags & NS_MOD_RCMD) ? MTY_MOD_RWIN : 0;
	mod |= (flags & NS_MOD_LALT) ? MTY_MOD_LALT : 0;
	mod |= (flags & NS_MOD_RALT) ? MTY_MOD_RALT : 0;
	mod |= (flags & NSEventModifierFlagCapsLock) ? MTY_MOD_CAPS : 0;

	return mod;
}

static MTY_Key keymap_keycode_to_key(uint16_t kc)
{
	switch (kc) {
		case kVK_ANSI_A:              return MTY_KEY_A;
		case kVK_ANSI_S:              return MTY_KEY_S;
		case kVK_ANSI_D:              return MTY_KEY_D;
		case kVK_ANSI_F:              return MTY_KEY_F;
		case kVK_ANSI_H:              return MTY_KEY_H;
		case kVK_ANSI_G:              return MTY_KEY_G;
		case kVK_ANSI_Z:              return MTY_KEY_Z;
		case kVK_ANSI_X:              return MTY_KEY_X;
		case kVK_ANSI_C:              return MTY_KEY_C;
		case kVK_ANSI_V:              return MTY_KEY_V;
		case kVK_ISO_Section:         return MTY_KEY_INTL_BACKSLASH;
		case kVK_ANSI_B:              return MTY_KEY_B;
		case kVK_ANSI_Q:              return MTY_KEY_Q;
		case kVK_ANSI_W:              return MTY_KEY_W;
		case kVK_ANSI_E:              return MTY_KEY_E;
		case kVK_ANSI_R:              return MTY_KEY_R;
		case kVK_ANSI_Y:              return MTY_KEY_Y;
		case kVK_ANSI_T:              return MTY_KEY_T;
		case kVK_ANSI_1:              return MTY_KEY_1;
		case kVK_ANSI_2:              return MTY_KEY_2;
		case kVK_ANSI_3:              return MTY_KEY_3;
		case kVK_ANSI_4:              return MTY_KEY_4;
		case kVK_ANSI_6:              return MTY_KEY_6;
		case kVK_ANSI_5:              return MTY_KEY_5;
		case kVK_ANSI_Equal:          return MTY_KEY_EQUALS;
		case kVK_ANSI_9:              return MTY_KEY_9;
		case kVK_ANSI_7:              return MTY_KEY_7;
		case kVK_ANSI_Minus:          return MTY_KEY_MINUS;
		case kVK_ANSI_8:              return MTY_KEY_8;
		case kVK_ANSI_0:              return MTY_KEY_0;
		case kVK_ANSI_RightBracket:   return MTY_KEY_RBRACKET;
		case kVK_ANSI_O:              return MTY_KEY_O;
		case kVK_ANSI_U:              return MTY_KEY_U;
		case kVK_ANSI_LeftBracket:    return MTY_KEY_LBRACKET;
		case kVK_ANSI_I:              return MTY_KEY_I;
		case kVK_ANSI_P:              return MTY_KEY_P;
		case kVK_Return:              return MTY_KEY_ENTER;
		case kVK_ANSI_L:              return MTY_KEY_L;
		case kVK_ANSI_J:              return MTY_KEY_J;
		case kVK_ANSI_Quote:          return MTY_KEY_QUOTE;
		case kVK_ANSI_K:              return MTY_KEY_K;
		case kVK_ANSI_Semicolon:      return MTY_KEY_SEMICOLON;
		case kVK_ANSI_Backslash:      return MTY_KEY_BACKSLASH;
		case kVK_ANSI_Comma:          return MTY_KEY_COMMA;
		case kVK_ANSI_Slash:          return MTY_KEY_SLASH;
		case kVK_ANSI_N:              return MTY_KEY_N;
		case kVK_ANSI_M:              return MTY_KEY_M;
		case kVK_ANSI_Period:         return MTY_KEY_PERIOD;
		case kVK_Tab:                 return MTY_KEY_TAB;
		case kVK_Space:               return MTY_KEY_SPACE;
		case kVK_ANSI_Grave:          return MTY_KEY_GRAVE;
		case kVK_Delete:              return MTY_KEY_BACKSPACE;
		case 0x34:                    return MTY_KEY_NONE;
		case kVK_Escape:              return MTY_KEY_ESCAPE;
		case kVK_RightCommand:        return MTY_KEY_RWIN;
		case kVK_Command:             return MTY_KEY_LWIN;
		case kVK_Shift:               return MTY_KEY_LSHIFT;
		case kVK_CapsLock:            return MTY_KEY_CAPS;
		case kVK_Option:              return MTY_KEY_LALT;
		case kVK_Control:             return MTY_KEY_LCTRL;
		case kVK_RightShift:          return MTY_KEY_RSHIFT;
		case kVK_RightOption:         return MTY_KEY_RALT;
		case kVK_RightControl:        return MTY_KEY_RCTRL;
		case kVK_Function:            return MTY_KEY_NONE;
		case kVK_F17:                 return MTY_KEY_F17;
		case kVK_ANSI_KeypadDecimal:  return MTY_KEY_NP_PERIOD;
		case 0x42:                    return MTY_KEY_NONE;
		case kVK_ANSI_KeypadMultiply: return MTY_KEY_NP_MULTIPLY;
		case 0x44:                    return MTY_KEY_NONE;
		case kVK_ANSI_KeypadPlus:     return MTY_KEY_NP_PLUS;
		case 0x46:                    return MTY_KEY_NONE;
		case kVK_ANSI_KeypadClear:    return MTY_KEY_NUM_LOCK;
		case kVK_VolumeUp:            return MTY_KEY_VOLUME_UP;
		case kVK_VolumeDown:          return MTY_KEY_VOLUME_DOWN;
		case kVK_Mute:                return MTY_KEY_MUTE;
		case kVK_ANSI_KeypadDivide:   return MTY_KEY_NP_DIVIDE;
		case kVK_ANSI_KeypadEnter:    return MTY_KEY_NP_ENTER;
		case kVK_ANSI_KeypadMinus:    return MTY_KEY_NP_MINUS;
		case kVK_F18:                 return MTY_KEY_F18;
		case kVK_F19:                 return MTY_KEY_F19;
		case kVK_ANSI_KeypadEquals:   return MTY_KEY_EQUALS;
		case kVK_ANSI_Keypad0:        return MTY_KEY_NP_0;
		case kVK_ANSI_Keypad1:        return MTY_KEY_NP_1;
		case kVK_ANSI_Keypad2:        return MTY_KEY_NP_2;
		case kVK_ANSI_Keypad3:        return MTY_KEY_NP_3;
		case kVK_ANSI_Keypad4:        return MTY_KEY_NP_4;
		case kVK_ANSI_Keypad5:        return MTY_KEY_NP_5;
		case kVK_ANSI_Keypad6:        return MTY_KEY_NP_6;
		case kVK_ANSI_Keypad7:        return MTY_KEY_NP_7;
		case kVK_F20:                 return MTY_KEY_NONE;
		case kVK_ANSI_Keypad8:        return MTY_KEY_NP_8;
		case kVK_ANSI_Keypad9:        return MTY_KEY_NP_9;
		case kVK_JIS_Yen:             return MTY_KEY_YEN;
		case kVK_JIS_Underscore:      return MTY_KEY_RO;
		case kVK_JIS_KeypadComma:     return MTY_KEY_INTL_COMMA;
		case kVK_F5:                  return MTY_KEY_F5;
		case kVK_F6:                  return MTY_KEY_F6;
		case kVK_F7:                  return MTY_KEY_F7;
		case kVK_F3:                  return MTY_KEY_F3;
		case kVK_F8:                  return MTY_KEY_F8;
		case kVK_F9:                  return MTY_KEY_F9;
		case kVK_JIS_Eisu:            return MTY_KEY_MUHENKAN;
		case kVK_F11:                 return MTY_KEY_F11;
		case kVK_JIS_Kana:            return MTY_KEY_HENKAN;
		case kVK_F13:                 return MTY_KEY_PRINT_SCREEN;
		case kVK_F16:                 return MTY_KEY_F16;
		case kVK_F14:                 return MTY_KEY_SCROLL_LOCK;
		case kVK_F10:                 return MTY_KEY_F10;
		case kVK_WinApp:              return MTY_KEY_APP;
		case kVK_F12:                 return MTY_KEY_F12;
		case 0x70:                    return MTY_KEY_NONE;
		case kVK_F15:                 return MTY_KEY_PAUSE;
		case kVK_Help:                return MTY_KEY_INSERT;
		case kVK_Home:                return MTY_KEY_HOME;
		case kVK_PageUp:              return MTY_KEY_PAGE_UP;
		case kVK_ForwardDelete:       return MTY_KEY_DELETE;
		case kVK_F4:                  return MTY_KEY_F4;
		case kVK_End:                 return MTY_KEY_END;
		case kVK_F2:                  return MTY_KEY_F2;
		case kVK_PageDown:            return MTY_KEY_PAGE_DOWN;
		case kVK_F1:                  return MTY_KEY_F1;
		case kVK_LeftArrow:           return MTY_KEY_LEFT;
		case kVK_RightArrow:          return MTY_KEY_RIGHT;
		case kVK_DownArrow:           return MTY_KEY_DOWN;
		case kVK_UpArrow:             return MTY_KEY_UP;
	}

	return MTY_KEY_NONE;
}

static const char *keymap_keycode_to_text(uint16_t kc)
{
	switch (kc) {
		case kVK_Return:              return "Enter";
		case kVK_Tab:                 return "Tab";
		case kVK_Space:               return "Space";
		case kVK_Delete:              return "Backspace";
		case kVK_Escape:              return "Esc";
		case kVK_RightCommand:        return "Command";
		case kVK_Command:             return "Command";
		case kVK_Shift:               return "Shift";
		case kVK_CapsLock:            return "Caps";
		case kVK_Option:              return "Alt";
		case kVK_Control:             return "Alt";
		case kVK_RightShift:          return "Shift";
		case kVK_RightOption:         return "Alt";
		case kVK_RightControl:        return "Ctrl";
		case kVK_VolumeUp:            return "Volume Up";
		case kVK_VolumeDown:          return "Volume Down";
		case kVK_Mute:                return "Mute";
		case kVK_F18:                 return "F18";
		case kVK_F19:                 return "F19";
		case kVK_F20:                 return "F20";
		case kVK_F5:                  return "F5";
		case kVK_F6:                  return "F6";
		case kVK_F7:                  return "F7";
		case kVK_F3:                  return "F3";
		case kVK_F8:                  return "F8";
		case kVK_F9:                  return "F9";
		case kVK_F11:                 return "F11";
		case kVK_F13:                 return "Print Screen";
		case kVK_F16:                 return "F16";
		case kVK_F14:                 return "F14";
		case kVK_F10:                 return "F10";
		case kVK_WinApp:              return "App";
		case kVK_F12:                 return "F12";
		case kVK_F15:                 return "F15";
		case kVK_Help:                return "Insert";
		case kVK_Home:                return "Home";
		case kVK_PageUp:              return "Page Up";
		case kVK_ForwardDelete:       return "Delete";
		case kVK_F4:                  return "F4";
		case kVK_End:                 return "End";
		case kVK_F2:                  return "F2";
		case kVK_PageDown:            return "Page Down";
		case kVK_F1:                  return "F1";
		case kVK_LeftArrow:           return "Left";
		case kVK_RightArrow:          return "Right";
		case kVK_DownArrow:           return "Down";
		case kVK_UpArrow:             return "Up";
	}

	return NULL;
}
