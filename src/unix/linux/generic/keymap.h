// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

// https://wiki.archlinux.org/index.php/Keyboard_input
// https://cgit.freedesktop.org/xorg/driver/xf86-input-evdev/tree/src/evdev.c

// These keycodes originate in evdev based on the scancode, then X11 adds
// MIN_KEYCODE (8) to them

static const KeySym APP_KEY_MAP[MTY_KEY_MAX] = {
	[MTY_KEY_NONE]           = 0,
	[MTY_KEY_ESCAPE]         = 9,
	[MTY_KEY_1]              = 10,
	[MTY_KEY_2]              = 11,
	[MTY_KEY_3]              = 12,
	[MTY_KEY_4]              = 13,
	[MTY_KEY_5]              = 14,
	[MTY_KEY_6]              = 15,
	[MTY_KEY_7]              = 16,
	[MTY_KEY_8]              = 17,
	[MTY_KEY_9]              = 18,
	[MTY_KEY_0]              = 19,
	[MTY_KEY_MINUS]          = 20,
	[MTY_KEY_EQUALS]         = 21,
	[MTY_KEY_BACKSPACE]      = 22,
	[MTY_KEY_TAB]            = 23,
	[MTY_KEY_Q]              = 24,
	[MTY_KEY_AUDIO_PREV]     = 0,
	[MTY_KEY_W]              = 25,
	[MTY_KEY_E]              = 26,
	[MTY_KEY_R]              = 27,
	[MTY_KEY_T]              = 28,
	[MTY_KEY_Y]              = 29,
	[MTY_KEY_U]              = 30,
	[MTY_KEY_I]              = 31,
	[MTY_KEY_O]              = 32,
	[MTY_KEY_P]              = 33,
	[MTY_KEY_AUDIO_NEXT]     = 0,
	[MTY_KEY_LBRACKET]       = 34,
	[MTY_KEY_RBRACKET]       = 35,
	[MTY_KEY_ENTER]          = 36,
	[MTY_KEY_NP_ENTER]       = 104,
	[MTY_KEY_LCTRL]          = 37,
	[MTY_KEY_RCTRL]          = 105,
	[MTY_KEY_A]              = 38,
	[MTY_KEY_S]              = 39,
	[MTY_KEY_D]              = 40,
	[MTY_KEY_MUTE]           = 0,
	[MTY_KEY_F]              = 41,
	[MTY_KEY_G]              = 42,
	[MTY_KEY_AUDIO_PLAY]     = 0,
	[MTY_KEY_H]              = 43,
	[MTY_KEY_J]              = 44,
	[MTY_KEY_AUDIO_STOP]     = 0,
	[MTY_KEY_K]              = 45,
	[MTY_KEY_L]              = 46,
	[MTY_KEY_SEMICOLON]      = 47,
	[MTY_KEY_QUOTE]          = 48,
	[MTY_KEY_GRAVE]          = 49,
	[MTY_KEY_LSHIFT]         = 50,
	[MTY_KEY_BACKSLASH]      = 51,
	[MTY_KEY_Z]              = 52,
	[MTY_KEY_X]              = 53,
	[MTY_KEY_C]              = 54,
	[MTY_KEY_VOLUME_DOWN]    = 0,
	[MTY_KEY_V]              = 55,
	[MTY_KEY_B]              = 56,
	[MTY_KEY_VOLUME_UP]      = 0,
	[MTY_KEY_N]              = 57,
	[MTY_KEY_M]              = 58,
	[MTY_KEY_COMMA]          = 59,
	[MTY_KEY_PERIOD]         = 60,
	[MTY_KEY_SLASH]          = 61,
	[MTY_KEY_NP_DIVIDE]      = 106,
	[MTY_KEY_RSHIFT]         = 62,
	[MTY_KEY_NP_MULTIPLY]    = 63,
	[MTY_KEY_PRINT_SCREEN]   = 107,
	[MTY_KEY_LALT]           = 64,
	[MTY_KEY_RALT]           = 108,
	[MTY_KEY_SPACE]          = 65,
	[MTY_KEY_CAPS]           = 66,
	[MTY_KEY_F1]             = 67,
	[MTY_KEY_F2]             = 68,
	[MTY_KEY_F3]             = 69,
	[MTY_KEY_F4]             = 70,
	[MTY_KEY_F5]             = 71,
	[MTY_KEY_F6]             = 72,
	[MTY_KEY_F7]             = 73,
	[MTY_KEY_F8]             = 74,
	[MTY_KEY_F9]             = 75,
	[MTY_KEY_F10]            = 76,
	[MTY_KEY_NUM_LOCK]       = 77,
	[MTY_KEY_SCROLL_LOCK]    = 78,
	[MTY_KEY_PAUSE]          = 127,
	[MTY_KEY_NP_7]           = 79,
	[MTY_KEY_HOME]           = 110,
	[MTY_KEY_NP_8]           = 80,
	[MTY_KEY_UP]             = 111,
	[MTY_KEY_NP_9]           = 81,
	[MTY_KEY_PAGE_UP]        = 112,
	[MTY_KEY_NP_MINUS]       = 82,
	[MTY_KEY_NP_4]           = 83,
	[MTY_KEY_LEFT]           = 113,
	[MTY_KEY_NP_5]           = 84,
	[MTY_KEY_NP_6]           = 85,
	[MTY_KEY_RIGHT]          = 114,
	[MTY_KEY_NP_PLUS]        = 86,
	[MTY_KEY_NP_1]           = 87,
	[MTY_KEY_END]            = 115,
	[MTY_KEY_NP_2]           = 88,
	[MTY_KEY_DOWN]           = 116,
	[MTY_KEY_NP_3]           = 89,
	[MTY_KEY_PAGE_DOWN]      = 117,
	[MTY_KEY_NP_0]           = 90,
	[MTY_KEY_INSERT]         = 118,
	[MTY_KEY_NP_PERIOD]      = 91,
	[MTY_KEY_DELETE]         = 119,
	[MTY_KEY_INTL_BACKSLASH] = 94,
	[MTY_KEY_F11]            = 95,
	[MTY_KEY_F12]            = 96,
	[MTY_KEY_LWIN]           = 133,
	[MTY_KEY_RWIN]           = 134,
	[MTY_KEY_APP]            = 135,
	[MTY_KEY_F13]            = 191,
	[MTY_KEY_F14]            = 192,
	[MTY_KEY_F15]            = 193,
	[MTY_KEY_F16]            = 194,
	[MTY_KEY_F17]            = 195,
	[MTY_KEY_F18]            = 196,
	[MTY_KEY_F19]            = 197,
	[MTY_KEY_MEDIA_SELECT]   = 0,
	[MTY_KEY_JP]             = 101,
	[MTY_KEY_RO]             = 97,
	[MTY_KEY_HENKAN]         = 100,
	[MTY_KEY_MUHENKAN]       = 102,
	[MTY_KEY_INTL_COMMA]     = 0,
	[MTY_KEY_YEN]            = 132,
};

static MTY_Key keymap_keycode_to_key(unsigned int keycode)
{
	switch (keycode) {
		case 9:    return MTY_KEY_ESCAPE;
		case 10:   return MTY_KEY_1;
		case 11:   return MTY_KEY_2;
		case 12:   return MTY_KEY_3;
		case 13:   return MTY_KEY_4;
		case 14:   return MTY_KEY_5;
		case 15:   return MTY_KEY_6;
		case 16:   return MTY_KEY_7;
		case 17:   return MTY_KEY_8;
		case 18:   return MTY_KEY_9;
		case 19:   return MTY_KEY_0;
		case 20:   return MTY_KEY_MINUS;
		case 21:   return MTY_KEY_EQUALS;
		case 22:   return MTY_KEY_BACKSPACE;
		case 23:   return MTY_KEY_TAB;
		case 24:   return MTY_KEY_Q;
		case 25:   return MTY_KEY_W;
		case 26:   return MTY_KEY_E;
		case 27:   return MTY_KEY_R;
		case 28:   return MTY_KEY_T;
		case 29:   return MTY_KEY_Y;
		case 30:   return MTY_KEY_U;
		case 31:   return MTY_KEY_I;
		case 32:   return MTY_KEY_O;
		case 33:   return MTY_KEY_P;
		case 34:   return MTY_KEY_LBRACKET;
		case 35:   return MTY_KEY_RBRACKET;
		case 36:   return MTY_KEY_ENTER;
		case 104:  return MTY_KEY_NP_ENTER;
		case 37:   return MTY_KEY_LCTRL;
		case 105:  return MTY_KEY_RCTRL;
		case 38:   return MTY_KEY_A;
		case 39:   return MTY_KEY_S;
		case 40:   return MTY_KEY_D;
		case 41:   return MTY_KEY_F;
		case 42:   return MTY_KEY_G;
		case 43:   return MTY_KEY_H;
		case 44:   return MTY_KEY_J;
		case 45:   return MTY_KEY_K;
		case 46:   return MTY_KEY_L;
		case 47:   return MTY_KEY_SEMICOLON;
		case 48:   return MTY_KEY_QUOTE;
		case 49:   return MTY_KEY_GRAVE;
		case 50:   return MTY_KEY_LSHIFT;
		case 51:   return MTY_KEY_BACKSLASH;
		case 52:   return MTY_KEY_Z;
		case 53:   return MTY_KEY_X;
		case 54:   return MTY_KEY_C;
		case 55:   return MTY_KEY_V;
		case 56:   return MTY_KEY_B;
		case 57:   return MTY_KEY_N;
		case 58:   return MTY_KEY_M;
		case 59:   return MTY_KEY_COMMA;
		case 60:   return MTY_KEY_PERIOD;
		case 61:   return MTY_KEY_SLASH;
		case 106:  return MTY_KEY_NP_DIVIDE;
		case 62:   return MTY_KEY_RSHIFT;
		case 63:   return MTY_KEY_NP_MULTIPLY;
		case 107:  return MTY_KEY_PRINT_SCREEN;
		case 64:   return MTY_KEY_LALT;
		case 108:  return MTY_KEY_RALT;
		case 65:   return MTY_KEY_SPACE;
		case 66:   return MTY_KEY_CAPS;
		case 67:   return MTY_KEY_F1;
		case 68:   return MTY_KEY_F2;
		case 69:   return MTY_KEY_F3;
		case 70:   return MTY_KEY_F4;
		case 71:   return MTY_KEY_F5;
		case 72:   return MTY_KEY_F6;
		case 73:   return MTY_KEY_F7;
		case 74:   return MTY_KEY_F8;
		case 75:   return MTY_KEY_F9;
		case 76:   return MTY_KEY_F10;
		case 77:   return MTY_KEY_NUM_LOCK;
		case 78:   return MTY_KEY_SCROLL_LOCK;
		case 127:  return MTY_KEY_PAUSE;
		case 79:   return MTY_KEY_NP_7;
		case 110:  return MTY_KEY_HOME;
		case 80:   return MTY_KEY_NP_8;
		case 111:  return MTY_KEY_UP;
		case 81:   return MTY_KEY_NP_9;
		case 112:  return MTY_KEY_PAGE_UP;
		case 82:   return MTY_KEY_NP_MINUS;
		case 83:   return MTY_KEY_NP_4;
		case 113:  return MTY_KEY_LEFT;
		case 84:   return MTY_KEY_NP_5;
		case 85:   return MTY_KEY_NP_6;
		case 114:  return MTY_KEY_RIGHT;
		case 86:   return MTY_KEY_NP_PLUS;
		case 87:   return MTY_KEY_NP_1;
		case 115:  return MTY_KEY_END;
		case 88:   return MTY_KEY_NP_2;
		case 116:  return MTY_KEY_DOWN;
		case 89:   return MTY_KEY_NP_3;
		case 117:  return MTY_KEY_PAGE_DOWN;
		case 90:   return MTY_KEY_NP_0;
		case 118:  return MTY_KEY_INSERT;
		case 91:   return MTY_KEY_NP_PERIOD;
		case 119:  return MTY_KEY_DELETE;
		case 94:   return MTY_KEY_INTL_BACKSLASH;
		case 95:   return MTY_KEY_F11;
		case 96:   return MTY_KEY_F12;
		case 133:  return MTY_KEY_LWIN;
		case 134:  return MTY_KEY_RWIN;
		case 135:  return MTY_KEY_APP;
		case 191:  return MTY_KEY_F13;
		case 192:  return MTY_KEY_F14;
		case 193:  return MTY_KEY_F15;
		case 194:  return MTY_KEY_F16;
		case 195:  return MTY_KEY_F17;
		case 196:  return MTY_KEY_F18;
		case 197:  return MTY_KEY_F19;
		case 101:  return MTY_KEY_JP;
		case 97:   return MTY_KEY_RO;
		case 100:  return MTY_KEY_HENKAN;
		case 102:  return MTY_KEY_MUHENKAN;
		case 132:  return MTY_KEY_YEN;
	}

	return MTY_KEY_NONE;
}

static MTY_Mod keymap_keystate_to_keymod(MTY_Key key, bool pressed, unsigned int state)
{
	MTY_Mod mod = MTY_MOD_NONE;

	// ModXMask may be remapped by the user

	if (state & ShiftMask)   mod |= MTY_MOD_LSHIFT;
	if (state & LockMask)    mod |= MTY_MOD_CAPS;
	if (state & ControlMask) mod |= MTY_MOD_LCTRL;
	if (state & Mod1Mask)    mod |= MTY_MOD_LALT;
	if (state & Mod2Mask)    mod |= MTY_MOD_NUM;
	if (state & Mod4Mask)    mod |= MTY_MOD_LWIN;

	// X11 gives you the mods just before the current key so we need to incorporate
	// the current event's key

	if (key == MTY_KEY_LSHIFT || key == MTY_KEY_RSHIFT)
		mod = pressed ? (mod | MTY_MOD_LSHIFT) : (mod & ~MTY_MOD_LSHIFT);

	if (key == MTY_KEY_LCTRL || key == MTY_KEY_RCTRL)
		mod = pressed ? (mod | MTY_MOD_LCTRL) : (mod & ~MTY_MOD_LCTRL);

	if (key == MTY_KEY_LALT || key == MTY_KEY_RALT)
		mod = pressed ? (mod | MTY_MOD_LALT) : (mod & ~MTY_MOD_LALT);

	if (key == MTY_KEY_LWIN || key == MTY_KEY_RWIN)
		mod = pressed ? (mod | MTY_MOD_LWIN) : (mod & ~MTY_MOD_LWIN);

	return mod;
}
