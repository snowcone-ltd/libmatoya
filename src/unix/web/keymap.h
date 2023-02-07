// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

static MTY_Mod web_keymap_mods(uint32_t jmods)
{
	MTY_Mod mods = 0;

	if (jmods & 0x01) mods |= MTY_MOD_LSHIFT;
	if (jmods & 0x02) mods |= MTY_MOD_LCTRL;
	if (jmods & 0x04) mods |= MTY_MOD_LALT;
	if (jmods & 0x08) mods |= MTY_MOD_LWIN;
	if (jmods & 0x10) mods |= MTY_MOD_CAPS;
	if (jmods & 0x20) mods |= MTY_MOD_NUM;

	return mods;
}

static void web_keymap_set_key(MTY_Hash *h, bool reverse, const char *code, MTY_Key key)
{
	uintptr_t val = key;

	if (reverse)
		val |= 0x10000;

	MTY_HashSet(h, code, (void *) val);
}

static MTY_Hash *web_keymap_hash(void)
{
	MTY_Hash *h = MTY_HashCreate(250);

	web_keymap_set_key(h, true, "Escape",          MTY_KEY_ESCAPE);
	web_keymap_set_key(h, true, "F1",              MTY_KEY_F1);
	web_keymap_set_key(h, true, "F2",              MTY_KEY_F2);
	web_keymap_set_key(h, true, "F3",              MTY_KEY_F3);
	web_keymap_set_key(h, true, "F4",              MTY_KEY_F4);
	web_keymap_set_key(h, true, "F5",              MTY_KEY_F5);
	web_keymap_set_key(h, true, "F6",              MTY_KEY_F6);
	web_keymap_set_key(h, true, "F7",              MTY_KEY_F7);
	web_keymap_set_key(h, true, "F8",              MTY_KEY_F8);
	web_keymap_set_key(h, true, "F9",              MTY_KEY_F9);
	web_keymap_set_key(h, true, "F10",             MTY_KEY_F10);
	web_keymap_set_key(h, true, "F11",             MTY_KEY_F11);
	web_keymap_set_key(h, true, "F12",             MTY_KEY_F12);

	web_keymap_set_key(h, true, "Backquote",       MTY_KEY_GRAVE);
	web_keymap_set_key(h, true, "Digit1",          MTY_KEY_1);
	web_keymap_set_key(h, true, "Digit2",          MTY_KEY_2);
	web_keymap_set_key(h, true, "Digit3",          MTY_KEY_3);
	web_keymap_set_key(h, true, "Digit4",          MTY_KEY_4);
	web_keymap_set_key(h, true, "Digit5",          MTY_KEY_5);
	web_keymap_set_key(h, true, "Digit6",          MTY_KEY_6);
	web_keymap_set_key(h, true, "Digit7",          MTY_KEY_7);
	web_keymap_set_key(h, true, "Digit8",          MTY_KEY_8);
	web_keymap_set_key(h, true, "Digit9",          MTY_KEY_9);
	web_keymap_set_key(h, true, "Digit0",          MTY_KEY_0);
	web_keymap_set_key(h, true, "Minus",           MTY_KEY_MINUS);
	web_keymap_set_key(h, true, "Equal",           MTY_KEY_EQUALS);
	web_keymap_set_key(h, true, "Backspace",       MTY_KEY_BACKSPACE);

	web_keymap_set_key(h, true, "Tab",             MTY_KEY_TAB);
	web_keymap_set_key(h, true, "KeyQ",            MTY_KEY_Q);
	web_keymap_set_key(h, true, "KeyW",            MTY_KEY_W);
	web_keymap_set_key(h, true, "KeyE",            MTY_KEY_E);
	web_keymap_set_key(h, true, "KeyR",            MTY_KEY_R);
	web_keymap_set_key(h, true, "KeyT",            MTY_KEY_T);
	web_keymap_set_key(h, true, "KeyY",            MTY_KEY_Y);
	web_keymap_set_key(h, true, "KeyU",            MTY_KEY_U);
	web_keymap_set_key(h, true, "KeyI",            MTY_KEY_I);
	web_keymap_set_key(h, true, "KeyO",            MTY_KEY_O);
	web_keymap_set_key(h, true, "KeyP",            MTY_KEY_P);
	web_keymap_set_key(h, true, "BracketLeft",     MTY_KEY_LBRACKET);
	web_keymap_set_key(h, true, "BracketRight",    MTY_KEY_RBRACKET);
	web_keymap_set_key(h, true, "Backslash",       MTY_KEY_BACKSLASH);

	web_keymap_set_key(h, true, "CapsLock",        MTY_KEY_CAPS);
	web_keymap_set_key(h, true, "KeyA",            MTY_KEY_A);
	web_keymap_set_key(h, true, "KeyS",            MTY_KEY_S);
	web_keymap_set_key(h, true, "KeyD",            MTY_KEY_D);
	web_keymap_set_key(h, true, "KeyF",            MTY_KEY_F);
	web_keymap_set_key(h, true, "KeyG",            MTY_KEY_G);
	web_keymap_set_key(h, true, "KeyH",            MTY_KEY_H);
	web_keymap_set_key(h, true, "KeyJ",            MTY_KEY_J);
	web_keymap_set_key(h, true, "KeyK",            MTY_KEY_K);
	web_keymap_set_key(h, true, "KeyL",            MTY_KEY_L);
	web_keymap_set_key(h, true, "Semicolon",       MTY_KEY_SEMICOLON);
	web_keymap_set_key(h, true, "Quote",           MTY_KEY_QUOTE);
	web_keymap_set_key(h, true, "Enter",           MTY_KEY_ENTER);

	web_keymap_set_key(h, true, "ShiftLeft",       MTY_KEY_LSHIFT);
	web_keymap_set_key(h, true, "KeyZ",            MTY_KEY_Z);
	web_keymap_set_key(h, true, "KeyX",            MTY_KEY_X);
	web_keymap_set_key(h, true, "KeyC",            MTY_KEY_C);
	web_keymap_set_key(h, true, "KeyV",            MTY_KEY_V);
	web_keymap_set_key(h, true, "KeyB",            MTY_KEY_B);
	web_keymap_set_key(h, true, "KeyN",            MTY_KEY_N);
	web_keymap_set_key(h, true, "KeyM",            MTY_KEY_M);
	web_keymap_set_key(h, true, "Comma",           MTY_KEY_COMMA);
	web_keymap_set_key(h, true, "Period",          MTY_KEY_PERIOD);
	web_keymap_set_key(h, true, "Slash",           MTY_KEY_SLASH);
	web_keymap_set_key(h, true, "ShiftRight",      MTY_KEY_RSHIFT);

	web_keymap_set_key(h, true, "ControlLeft",     MTY_KEY_LCTRL);
	web_keymap_set_key(h, true, "MetaLeft",        MTY_KEY_LWIN);
	web_keymap_set_key(h, true, "AltLeft",         MTY_KEY_LALT);
	web_keymap_set_key(h, true, "Space",           MTY_KEY_SPACE);
	web_keymap_set_key(h, true, "AltRight",        MTY_KEY_RALT);
	web_keymap_set_key(h, true, "MetaRight",       MTY_KEY_RWIN);
	web_keymap_set_key(h, true, "ContextMenu",     MTY_KEY_APP);
	web_keymap_set_key(h, true, "ControlRight",    MTY_KEY_RCTRL);

	web_keymap_set_key(h, true, "PrintScreen",     MTY_KEY_PRINT_SCREEN);
	web_keymap_set_key(h, true, "ScrollLock",      MTY_KEY_SCROLL_LOCK);
	web_keymap_set_key(h, true, "Pause",           MTY_KEY_PAUSE);

	web_keymap_set_key(h, true, "Insert",          MTY_KEY_INSERT);
	web_keymap_set_key(h, true, "Home",            MTY_KEY_HOME);
	web_keymap_set_key(h, true, "PageUp",          MTY_KEY_PAGE_UP);
	web_keymap_set_key(h, true, "Delete",          MTY_KEY_DELETE);
	web_keymap_set_key(h, true, "End",             MTY_KEY_END);
	web_keymap_set_key(h, true, "PageDown",        MTY_KEY_PAGE_DOWN);

	web_keymap_set_key(h, true, "ArrowUp",         MTY_KEY_UP);
	web_keymap_set_key(h, true, "ArrowLeft",       MTY_KEY_LEFT);
	web_keymap_set_key(h, true, "ArrowDown",       MTY_KEY_DOWN);
	web_keymap_set_key(h, true, "ArrowRight",      MTY_KEY_RIGHT);

	web_keymap_set_key(h, true, "NumLock",         MTY_KEY_NUM_LOCK);
	web_keymap_set_key(h, true, "NumpadDivide",    MTY_KEY_NP_DIVIDE);
	web_keymap_set_key(h, true, "NumpadMultiply",  MTY_KEY_NP_MULTIPLY);
	web_keymap_set_key(h, true, "NumpadSubtract",  MTY_KEY_NP_MINUS);
	web_keymap_set_key(h, true, "Numpad7",         MTY_KEY_NP_7);
	web_keymap_set_key(h, true, "Numpad8",         MTY_KEY_NP_8);
	web_keymap_set_key(h, true, "Numpad9",         MTY_KEY_NP_9);
	web_keymap_set_key(h, true, "NumpadAdd",       MTY_KEY_NP_PLUS);
	web_keymap_set_key(h, true, "Numpad4",         MTY_KEY_NP_4);
	web_keymap_set_key(h, true, "Numpad5",         MTY_KEY_NP_5);
	web_keymap_set_key(h, true, "Numpad6",         MTY_KEY_NP_6);
	web_keymap_set_key(h, true, "Numpad1",         MTY_KEY_NP_1);
	web_keymap_set_key(h, true, "Numpad2",         MTY_KEY_NP_2);
	web_keymap_set_key(h, true, "Numpad3",         MTY_KEY_NP_3);
	web_keymap_set_key(h, true, "NumpadEnter",     MTY_KEY_NP_ENTER);
	web_keymap_set_key(h, true, "Numpad0",         MTY_KEY_NP_0);
	web_keymap_set_key(h, true, "NumpadDecimal",   MTY_KEY_NP_PERIOD);

	// Non-US
	web_keymap_set_key(h, true, "IntlBackslash",   MTY_KEY_INTL_BACKSLASH);
	web_keymap_set_key(h, true, "IntlYen",         MTY_KEY_YEN);
	web_keymap_set_key(h, true, "IntlRo",          MTY_KEY_RO);
	web_keymap_set_key(h, true, "NonConvert",      MTY_KEY_MUHENKAN);
	web_keymap_set_key(h, true, "Convert",         MTY_KEY_HENKAN);
	web_keymap_set_key(h, true, "Hiragana",        MTY_KEY_JP);

	web_keymap_set_key(h, false, "AltGraph",       MTY_KEY_RALT);
	web_keymap_set_key(h, false, "Lang2",          MTY_KEY_MUHENKAN);
	web_keymap_set_key(h, false, "Lang1",          MTY_KEY_HENKAN);
	web_keymap_set_key(h, false, "KanaMode",       MTY_KEY_JP);
	web_keymap_set_key(h, false, "Zenkaku",        MTY_KEY_GRAVE);

	return h;
}
