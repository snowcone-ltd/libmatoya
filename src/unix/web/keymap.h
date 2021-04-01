// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "web.h"

static void keymap_set_keys(void)
{
	web_set_key(true, "Escape",          MTY_KEY_ESCAPE);
	web_set_key(true, "F1",              MTY_KEY_F1);
	web_set_key(true, "F2",              MTY_KEY_F2);
	web_set_key(true, "F3",              MTY_KEY_F3);
	web_set_key(true, "F4",              MTY_KEY_F4);
	web_set_key(true, "F5",              MTY_KEY_F5);
	web_set_key(true, "F6",              MTY_KEY_F6);
	web_set_key(true, "F7",              MTY_KEY_F7);
	web_set_key(true, "F8",              MTY_KEY_F8);
	web_set_key(true, "F9",              MTY_KEY_F9);
	web_set_key(true, "F10",             MTY_KEY_F10);
	web_set_key(true, "F11",             MTY_KEY_F11);
	web_set_key(true, "F12",             MTY_KEY_F12);

	web_set_key(true, "Backquote",       MTY_KEY_GRAVE);
	web_set_key(true, "Digit1",          MTY_KEY_1);
	web_set_key(true, "Digit2",          MTY_KEY_2);
	web_set_key(true, "Digit3",          MTY_KEY_3);
	web_set_key(true, "Digit4",          MTY_KEY_4);
	web_set_key(true, "Digit5",          MTY_KEY_5);
	web_set_key(true, "Digit6",          MTY_KEY_6);
	web_set_key(true, "Digit7",          MTY_KEY_7);
	web_set_key(true, "Digit8",          MTY_KEY_8);
	web_set_key(true, "Digit9",          MTY_KEY_9);
	web_set_key(true, "Digit0",          MTY_KEY_0);
	web_set_key(true, "Minus",           MTY_KEY_MINUS);
	web_set_key(true, "Equal",           MTY_KEY_EQUALS);
	web_set_key(true, "Backspace",       MTY_KEY_BACKSPACE);

	web_set_key(true, "Tab",             MTY_KEY_TAB);
	web_set_key(true, "KeyQ",            MTY_KEY_Q);
	web_set_key(true, "KeyW",            MTY_KEY_W);
	web_set_key(true, "KeyE",            MTY_KEY_E);
	web_set_key(true, "KeyR",            MTY_KEY_R);
	web_set_key(true, "KeyT",            MTY_KEY_T);
	web_set_key(true, "KeyY",            MTY_KEY_Y);
	web_set_key(true, "KeyU",            MTY_KEY_U);
	web_set_key(true, "KeyI",            MTY_KEY_I);
	web_set_key(true, "KeyO",            MTY_KEY_O);
	web_set_key(true, "KeyP",            MTY_KEY_P);
	web_set_key(true, "BracketLeft",     MTY_KEY_LBRACKET);
	web_set_key(true, "BracketRight",    MTY_KEY_RBRACKET);
	web_set_key(true, "Backslash",       MTY_KEY_BACKSLASH);

	web_set_key(true, "CapsLock",        MTY_KEY_CAPS);
	web_set_key(true, "KeyA",            MTY_KEY_A);
	web_set_key(true, "KeyS",            MTY_KEY_S);
	web_set_key(true, "KeyD",            MTY_KEY_D);
	web_set_key(true, "KeyF",            MTY_KEY_F);
	web_set_key(true, "KeyG",            MTY_KEY_G);
	web_set_key(true, "KeyH",            MTY_KEY_H);
	web_set_key(true, "KeyJ",            MTY_KEY_J);
	web_set_key(true, "KeyK",            MTY_KEY_K);
	web_set_key(true, "KeyL",            MTY_KEY_L);
	web_set_key(true, "Semicolon",       MTY_KEY_SEMICOLON);
	web_set_key(true, "Quote",           MTY_KEY_QUOTE);
	web_set_key(true, "Enter",           MTY_KEY_ENTER);

	web_set_key(true, "ShiftLeft",       MTY_KEY_LSHIFT);
	web_set_key(true, "KeyZ",            MTY_KEY_Z);
	web_set_key(true, "KeyX",            MTY_KEY_X);
	web_set_key(true, "KeyC",            MTY_KEY_C);
	web_set_key(true, "KeyV",            MTY_KEY_V);
	web_set_key(true, "KeyB",            MTY_KEY_B);
	web_set_key(true, "KeyN",            MTY_KEY_N);
	web_set_key(true, "KeyM",            MTY_KEY_M);
	web_set_key(true, "Comma",           MTY_KEY_COMMA);
	web_set_key(true, "Period",          MTY_KEY_PERIOD);
	web_set_key(true, "Slash",           MTY_KEY_SLASH);
	web_set_key(true, "ShiftRight",      MTY_KEY_RSHIFT);

	web_set_key(true, "ControlLeft",     MTY_KEY_LCTRL);
	web_set_key(true, "MetaLeft",        MTY_KEY_LWIN);
	web_set_key(true, "AltLeft",         MTY_KEY_LALT);
	web_set_key(true, "Space",           MTY_KEY_SPACE);
	web_set_key(true, "AltRight",        MTY_KEY_RALT);
	web_set_key(true, "MetaRight",       MTY_KEY_RWIN);
	web_set_key(true, "ContextMenu",     MTY_KEY_APP);
	web_set_key(true, "ControlRight",    MTY_KEY_RCTRL);

	web_set_key(true, "PrintScreen",     MTY_KEY_PRINT_SCREEN);
	web_set_key(true, "ScrollLock",      MTY_KEY_SCROLL_LOCK);
	web_set_key(true, "Pause",           MTY_KEY_PAUSE);

	web_set_key(true, "Insert",          MTY_KEY_INSERT);
	web_set_key(true, "Home",            MTY_KEY_HOME);
	web_set_key(true, "PageUp",          MTY_KEY_PAGE_UP);
	web_set_key(true, "Delete",          MTY_KEY_DELETE);
	web_set_key(true, "End",             MTY_KEY_END);
	web_set_key(true, "PageDown",        MTY_KEY_PAGE_DOWN);

	web_set_key(true, "ArrowUp",         MTY_KEY_UP);
	web_set_key(true, "ArrowLeft",       MTY_KEY_LEFT);
	web_set_key(true, "ArrowDown",       MTY_KEY_DOWN);
	web_set_key(true, "ArrowRight",      MTY_KEY_RIGHT);

	web_set_key(true, "NumLock",         MTY_KEY_NUM_LOCK);
	web_set_key(true, "NumpadDivide",    MTY_KEY_NP_DIVIDE);
	web_set_key(true, "NumpadMultiply",  MTY_KEY_NP_MULTIPLY);
	web_set_key(true, "NumpadSubtract",  MTY_KEY_NP_MINUS);
	web_set_key(true, "Numpad7",         MTY_KEY_NP_7);
	web_set_key(true, "Numpad8",         MTY_KEY_NP_8);
	web_set_key(true, "Numpad9",         MTY_KEY_NP_9);
	web_set_key(true, "NumpadAdd",       MTY_KEY_NP_PLUS);
	web_set_key(true, "Numpad4",         MTY_KEY_NP_4);
	web_set_key(true, "Numpad5",         MTY_KEY_NP_5);
	web_set_key(true, "Numpad6",         MTY_KEY_NP_6);
	web_set_key(true, "Numpad1",         MTY_KEY_NP_1);
	web_set_key(true, "Numpad2",         MTY_KEY_NP_2);
	web_set_key(true, "Numpad3",         MTY_KEY_NP_3);
	web_set_key(true, "NumpadEnter",     MTY_KEY_NP_ENTER);
	web_set_key(true, "Numpad0",         MTY_KEY_NP_0);
	web_set_key(true, "NumpadDecimal",   MTY_KEY_NP_PERIOD);

	// Non-US
	web_set_key(true, "IntlBackslash",   MTY_KEY_INTL_BACKSLASH);
	web_set_key(true, "IntlYen",         MTY_KEY_YEN);
	web_set_key(true, "IntlRo",          MTY_KEY_RO);
	web_set_key(true, "NonConvert",      MTY_KEY_MUHENKAN);
	web_set_key(true, "Convert",         MTY_KEY_HENKAN);
	web_set_key(true, "Hiragana",        MTY_KEY_JP);

	web_set_key(false, "AltGraph",       MTY_KEY_RALT);
	web_set_key(false, "Lang2",          MTY_KEY_MUHENKAN);
	web_set_key(false, "Lang1",          MTY_KEY_HENKAN);
	web_set_key(false, "KanaMode",       MTY_KEY_JP);
	web_set_key(false, "Zenkaku",        MTY_KEY_GRAVE);
}
