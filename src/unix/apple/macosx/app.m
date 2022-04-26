// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "wsize.h"
#include "scale.h"
#include "keymap.h"
#include "hid/hid.h"


// NSApp

@interface App : NSObject <NSApplicationDelegate, NSUserNotificationCenterDelegate>
	@property(strong) NSCursor *custom_cursor;
	@property(strong) NSCursor *cursor;
	@property IOPMAssertionID assertion;
	@property MTY_AppFunc app_func;
	@property MTY_EventFunc event_func;
	@property MTY_Hash *hotkey;
	@property MTY_DetachState detach;
	@property void *opaque;
	@property void *kb_mode;
	@property bool relative;
	@property bool grab_mouse;
	@property bool grab_kb;
	@property bool cont;
	@property bool pen_enabled;
	@property bool default_cursor;
	@property bool cursor_outside;
	@property bool cursor_showing;
	@property bool eraser;
	@property bool pen_left;
	@property NSUInteger buttons;
	@property uint32_t cb_seq;
	@property void **windows;
	@property float timeout;
	@property struct hid *hid;
@end

static const MTY_Button APP_MOUSE_MAP[] = {
	MTY_BUTTON_LEFT,
	MTY_BUTTON_RIGHT,
	MTY_BUTTON_MIDDLE,
	MTY_BUTTON_X1,
	MTY_BUTTON_X2,
};

typedef int32_t CGSConnection;
typedef enum {
	CGSGlobalHotKeyEnable  = 0,
	CGSGlobalHotKeyDisable = 1,
} CGSGlobalHotKeyOperatingMode;

extern CGSConnection CGSMainConnectionID(void);
extern CGError CGSSetGlobalHotKeyOperatingMode(CGSConnection connection, CGSGlobalHotKeyOperatingMode mode);

#define APP_MOUSE_MAX (sizeof(APP_MOUSE_MAP) / sizeof(MTY_Button))

static void app_schedule_func(App *ctx)
{
	[NSTimer scheduledTimerWithTimeInterval:ctx.timeout
		target:ctx selector:@selector(appFunc:) userInfo:nil repeats:NO];
}

static void app_show_cursor(App *ctx, bool show)
{
	if (!ctx.cursor_showing && show) {
		[NSCursor unhide];

	} else if (ctx.cursor_showing && !show) {
		if (!ctx.cursor_outside)
			[NSCursor hide];
	}

	ctx.cursor_showing = show;
}

static void app_apply_relative(App *ctx)
{
	bool rel = ctx.relative && ctx.detach != MTY_DETACH_STATE_FULL;

	CGAssociateMouseAndMouseCursorPosition(!rel);

	if (rel) {
		// This function is globally stateful, so call it once upfront to make sure
		// there isn't a large diff from the last time it was called while in relative
		int32_t x = 0;
		int32_t y = 0;
		CGGetLastMouseDelta(&x, &y);
	}

	app_show_cursor(ctx, !rel);
}

static void app_apply_cursor(App *ctx)
{
	NSCursor *arrow = [NSCursor arrowCursor];
	NSCursor *new = ctx.default_cursor || ctx.cursor_outside || ctx.detach != MTY_DETACH_STATE_NONE ? arrow :
		ctx.custom_cursor ? ctx.custom_cursor : arrow;

	ctx.cursor = new;
	[ctx.cursor set];
}

static void app_apply_keyboard_state(App *ctx)
{
	if (ctx.grab_kb && ctx.detach == MTY_DETACH_STATE_NONE) {
		// Requires "Enable access for assistive devices" checkbox is checked
		// in the Universal Access preference pane
		if (!ctx.kb_mode) {
			ctx.kb_mode = PushSymbolicHotKeyMode(kHIHotKeyModeAllDisabled);
			CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyDisable);
		}

	} else if (ctx.kb_mode) {
		PopSymbolicHotKeyMode(ctx.kb_mode);
		CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyEnable);
		ctx.kb_mode = NULL;
	}
}

static void app_poll_clipboard(App *ctx)
{
	uint32_t cb_seq = [[NSPasteboard generalPasteboard] changeCount];

	if (cb_seq > ctx.cb_seq) {
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CLIPBOARD;

		ctx.event_func(&evt, ctx.opaque);
		ctx.cb_seq = cb_seq;
	}
}

static void app_fix_mouse_buttons(App *ctx)
{
	if (ctx.buttons == 0)
		return;

	NSUInteger buttons = ctx.buttons;
	NSUInteger pressed = [NSEvent pressedMouseButtons];

	for (NSUInteger x = 0; buttons > 0 && x < APP_MOUSE_MAX; x++) {
		if ((buttons & 1) && !(pressed & 1)) {
			MTY_Event evt = {0};
			evt.type = MTY_EVENT_BUTTON;
			evt.button.button = APP_MOUSE_MAP[x];

			ctx.event_func(&evt, ctx.opaque);
			ctx.buttons &= ~(1 << x);
		}

		buttons >>= 1;
		pressed >>= 1;
	}
}

@implementation App : NSObject
	- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
		shouldPresentNotification:(NSUserNotification *)notification
	{
		return YES;
	}

	- (void)applicationWillFinishLaunching:(NSNotification *)notification
	{
		[[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];

		[[NSAppleEventManager sharedAppleEventManager] setEventHandler:self
			andSelector:@selector(handleGetURLEvent:withReplyEvent:) forEventClass:kInternetEventClass
			andEventID:kAEGetURL];
	}

	- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
	{
		if ([NSApp windows].count > 0)
			[[NSApp windows][0] makeKeyAndOrderFront:self];

		return YES;
	}

	- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
	{
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_QUIT;

		self.event_func(&evt, self.opaque);

		return NSTerminateCancel;
	}

	- (void)appQuit
	{
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_QUIT;

		self.event_func(&evt, self.opaque);
	}

	- (void)appClose
	{
		[[NSApp keyWindow] performClose:self];
	}

	- (void)appRestart
	{
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_TRAY;
		evt.trayID = 3; // FIXME Arbitrary!

		self.event_func(&evt, self.opaque);
	}

	- (void)appMinimize
	{
		[[NSApp keyWindow] miniaturize:self];
	}

	- (void)appFunc:(NSTimer *)timer
	{
		app_poll_clipboard(self);
		app_fix_mouse_buttons(self);

		self.cont = self.app_func(self.opaque);

		if (!self.cont) {
			// Post a dummy event to spin the event loop
			NSEvent *dummy = [NSEvent otherEventWithType:NSApplicationDefined location:NSMakePoint(0, 0)
				modifierFlags:0 timestamp:[[NSDate date] timeIntervalSince1970] windowNumber:0
				context:nil subtype:0 data1:0 data2:0];

			[NSApp postEvent:dummy atStart:NO];

		} else {
			app_schedule_func(self);
		}
	}

	- (void)applicationDidFinishLaunching:(NSNotification *)notification
	{
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

		NSMenu *menubar = [NSMenu new];

		NSMenuItem *submenu = [NSMenuItem new];
		[menubar addItem:submenu];
		NSMenu *menu = [NSMenu new];
		[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Restart" action:@selector(appRestart) keyEquivalent:@""]];
		[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(appQuit) keyEquivalent:@"q"]];
		[submenu setSubmenu:menu];

		submenu = [NSMenuItem new];
		[menubar addItem:submenu];
		menu = [[NSMenu alloc] initWithTitle:@"Window"];
		[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(appMinimize) keyEquivalent:@"m"]];
		[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Close" action:@selector(appClose) keyEquivalent:@"w"]];
		[submenu setSubmenu:menu];

		[NSApp setMainMenu:menubar];

		[NSApp activateIgnoringOtherApps:YES];
	}

	- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
	{
		NSString *url = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];

		if (url) {
			MTY_Event evt = {0};
			evt.type = MTY_EVENT_REOPEN;
			evt.reopenArg = [url UTF8String];

			self.event_func(&evt, self.opaque);
		}
	}

	- (NSMenu *)applicationDockMenu:(NSApplication *)sender
	{
		NSMenu *menubar = [NSMenu new];
		NSMenuItem *item = [NSMenuItem new];
		NSMenu *menu = [NSMenu new];
		[menubar addItem:item];

		[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Restart" action:@selector(appRestart) keyEquivalent:@""]];
		[item setSubmenu:menu];

		return menu;
	}
@end


// NSWindow

@interface Window : NSWindow <NSWindowDelegate>
	@property(strong) App *app;
	@property MTY_Window window;
	@property MTY_GFX api;
	@property bool top;
	@property struct gfx_ctx *gfx_ctx;
@end


// Window helpers

static Window *app_get_window(MTY_App *app, MTY_Window window)
{
	App *ctx = (__bridge App *) app;

	return window < 0 ? nil : (__bridge Window *) ctx.windows[window];
}

static Window *app_get_window_by_number(App *ctx, NSInteger number)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
		if (ctx.windows[x]) {
			Window *window = (__bridge Window *) ctx.windows[x];

			if (window.windowNumber == number)
				return window;
		}
	}

	return nil;
}

static MTY_Window app_find_open_window(MTY_App *app, MTY_Window req)
{
	App *ctx = (__bridge App *) app;

	if (req >= 0 && req < MTY_WINDOW_MAX && !ctx.windows[req])
		return req;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (!ctx.windows[x])
			return x;

	return -1;
}

static MTY_Event window_event(Window *window, MTY_EventType type)
{
	MTY_Event evt = {0};
	evt.type = type;
	evt.window = window.window;

	return evt;
}


// Mouse movement / position helpers

static NSPoint window_mouse_pos(NSWindow *window)
{
	// Mouse position in screen coordinates
	NSPoint p = [NSEvent mouseLocation];

	p.x -= window.frame.origin.x;
	p.y = window.frame.size.height - p.y + window.frame.origin.y;

	return p;
}

static NSPoint window_client_mouse_pos(NSWindow *window)
{
	// Mouse position in client area window coordinates
	NSPoint p = [window mouseLocationOutsideOfEventStream];

	p.y = window.contentView.frame.size.height - p.y;

	return p;
}

static bool window_hit_test(NSPoint *p, NSSize s)
{
	return p->x >= 0 && p->y >= 0 && p->x < s.width && p->y < s.height;
}

static bool window_event_in_view(NSWindow *window, NSPoint *p)
{
	// Is the cursor in the client area?
	*p = window_client_mouse_pos(window);

	return window_hit_test(p, window.contentView.frame.size);
}

static bool window_event_in_nc(NSWindow *window)
{
	// Is the cursor in the non-client area?
	NSPoint p = window_mouse_pos(window);

	return window_hit_test(&p, window.frame.size);
}

static Window *window_find_mouse(Window *me, NSPoint *p)
{
	// Find the window where the cursor resides, accounting for overlapping windows
	// and non-client areas
	Window *top = nil;
	bool key_hit = false;
	NSArray<NSWindow *> *windows = [NSApp windows];

	for (uint32_t x = 0; x < windows.count; x++) {
		Window *window = app_get_window_by_number(me.app, windows[x].windowNumber);
		if (!window)
			continue;

		NSPoint wp = {0};
		if (window_event_in_view(window, &wp)) {
			if (window.isKeyWindow && window.windowNumber == me.windowNumber) {
				*p = wp;
				return window;
			}

			if (!top && !key_hit) {
				if (!window_event_in_nc([NSApp keyWindow])) {
					*p = wp;
					top = window;
				}
			}

			if (window.isKeyWindow) {
				key_hit = true;
				top = nil;
			}
		}
	}

	return top;
}


// Pen

static void window_pen_event(Window *window, NSEvent *event)
{
	NSPoint p = {0};
	Window *cur = window_find_mouse(window, &p);
	if (!cur)
		return;

	CGFloat scale = mty_screen_scale(cur.screen);
	MTY_Event evt = window_event(cur, MTY_EVENT_PEN);
	evt.pen.pressure = (uint16_t) lrint(event.pressure * 1024.0f);
	evt.pen.rotation = (uint16_t) lrint(event.rotation * 359.0f);
	evt.pen.tiltX = (int8_t) lrint(event.tilt.x * 90.0f);
	evt.pen.tiltY = (int8_t) lrint(event.tilt.y * 90.0f);
	evt.pen.x = lrint(p.x * scale);
	evt.pen.y = lrint(p.y * scale);

	bool touching = event.buttonMask & NSEventButtonMaskPenTip;

	// INVERTED must be set while hovering, but ERASER should only be set by
	// while TOUCHING is also true
	if (window.app.eraser) {
		evt.pen.flags |= MTY_PEN_FLAG_INVERTED;

		if (touching) {
			evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;
			evt.pen.flags |= MTY_PEN_FLAG_ERASER;
		}

	} else if (touching) {
		evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;
	}

	// While BARREL is held, TOUCHING must also be set
	if (event.buttonMask & NSEventButtonMaskPenLowerSide) {
		evt.pen.flags |= MTY_PEN_FLAG_BARREL;
		evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;
	}

	// LEAVE is set when the pen moves out of the tracking area (only once)
	if (window.app.pen_left) {
		evt.pen.flags |= MTY_PEN_FLAG_LEAVE;
		window.app.pen_left = false;
	}

	window.app.event_func(&evt, window.app.opaque);
}


// Mouse

static void window_mouse_button_event(Window *window, NSUInteger index, bool pressed)
{
	NSPoint p = {0};
	Window *cur = window_find_mouse(window, &p);
	if (!cur)
		return;

	if (index >= APP_MOUSE_MAX)
		return;

	CGFloat scale = mty_screen_scale(cur.screen);

	MTY_Event evt = window_event(cur, MTY_EVENT_BUTTON);
	evt.button.button = APP_MOUSE_MAP[index];
	evt.button.pressed = pressed;
	evt.button.x = lrint(scale * p.x);
	evt.button.y = lrint(scale * p.y);

	window.app.event_func(&evt, window.app.opaque);

	if (pressed) {
		window.app.buttons |= 1 << index;

	} else {
		window.app.buttons &= ~(1 << index);
	}
}

static void window_button_event(Window *window, NSEvent *event, NSUInteger index, bool pressed)
{
	if (window.app.pen_enabled && event.subtype == NSTabletPointEventSubtype) {
		window_pen_event(window, event);

	} else {
		window_mouse_button_event(window, index, pressed);
	}
}

static void window_warp_cursor(NSWindow *ctx, uint32_t x, int32_t y)
{
	CGDirectDisplayID display = ((NSNumber *) [ctx.screen deviceDescription][@"NSScreenNumber"]).intValue;

	CGFloat scale = mty_screen_scale(ctx.screen);
	CGFloat client_top = ctx.screen.frame.size.height - ctx.frame.origin.y +
		ctx.screen.frame.origin.y - ctx.contentView.frame.size.height;
	CGFloat client_left = ctx.frame.origin.x - ctx.screen.frame.origin.x;

	NSPoint pscreen = {0};
	pscreen.x = client_left + (CGFloat) x / scale;
	pscreen.y = client_top + (CGFloat) y / scale;

	CGDisplayMoveCursorToPoint(display, pscreen);
	CGAssociateMouseAndMouseCursorPosition(YES); // Supposedly reduces latency
}

static void window_confine_cursor(void)
{
	NSWindow *window = [NSApp keyWindow];
	if (!window)
		return;

	NSPoint wp = [window mouseLocationOutsideOfEventStream];
	NSSize size = window.contentView.frame.size;
	CGFloat scale = mty_screen_scale(window.screen);

	wp.y = size.height - wp.y;

	if (wp.x < 0)
		wp.x = 0;

	if (wp.y < 0)
		wp.y = 0;

	if (wp.x >= size.width)
		wp.x = size.width - 1;

	if (wp.y >= size.height)
		wp.y = size.height - 1;

	window_warp_cursor(window, lrint(wp.x * scale), lrint(wp.y * scale));
}

static void window_mouse_motion_event(Window *window, NSEvent *event, bool pen_in_range)
{
	if (window.app.relative && window.app.detach == MTY_DETACH_STATE_NONE && !pen_in_range) {
		MTY_Event evt = window_event(window, MTY_EVENT_MOTION);

		evt.motion.relative = true;
		CGGetLastMouseDelta(&evt.motion.x, &evt.motion.y);

		window.app.event_func(&evt, window.app.opaque);

	} else {
		NSPoint p = {0};
		Window *cur = window_find_mouse(window, &p);

		if (cur) {
			if (window.app.grab_mouse && window.app.detach == MTY_DETACH_STATE_NONE && !cur.isKeyWindow) {
				window_confine_cursor();

			} else if (cur.occlusionState & NSWindowOcclusionStateVisible) {
				MTY_Event evt = window_event(cur, MTY_EVENT_MOTION);

				CGFloat scale = mty_screen_scale(cur.screen);
				evt.motion.relative = false;
				evt.motion.x = lrint(scale * p.x);
				evt.motion.y = lrint(scale * p.y);

				window.app.event_func(&evt, window.app.opaque);
			}

		} else if (window.app.grab_mouse && window.app.detach == MTY_DETACH_STATE_NONE) {
			window_confine_cursor();
		}
	}
}

static void window_motion_event(Window *window, NSEvent *event)
{
	bool pen_in_range = event.subtype == NSTabletPointEventSubtype;

	if (window.app.pen_enabled && pen_in_range) {
		window_pen_event(window, event);

	} else {
		window_mouse_motion_event(window, event, pen_in_range);
	}
}

static void window_scroll_event(Window *window, NSEvent *event)
{
	CGFloat scale = mty_screen_scale(window.screen);
	int32_t delta = event.hasPreciseScrollingDeltas ? scale : scale * 80.0f;

	MTY_Event evt = window_event(window, MTY_EVENT_SCROLL);
	evt.scroll.x = lrint(-event.scrollingDeltaX * delta);
	evt.scroll.y = lrint(event.scrollingDeltaY * delta);

	window.app.event_func(&evt, window.app.opaque);
}


// Keyboard

static void window_text_event(Window *window, const char *text)
{
	// Make sure visible ASCII
	if (text && text[0] && text[0] >= 0x20 && text[0] != 0x7F) {
		MTY_Event evt = window_event(window, MTY_EVENT_TEXT);
		snprintf(evt.text, 8, "%s", text);

		window.app.event_func(&evt, window.app.opaque);
	}
}

static void window_keyboard_event(Window *window, int16_t key_code, NSEventModifierFlags flags, bool pressed)
{
	MTY_Event evt = window_event(window, MTY_EVENT_KEY);
	evt.key.key = keymap_keycode_to_key(key_code);
	evt.key.mod = keymap_modifier_flags_to_keymod(flags);
	evt.key.pressed = pressed;

	MTY_Mod mod = evt.key.mod & 0xFF;

	uint32_t hotkey = (uint32_t) (uintptr_t) MTY_HashGetInt(window.app.hotkey, (mod << 16) | evt.key.key);

	if (hotkey != 0) {
		if (pressed) {
			evt.type = MTY_EVENT_HOTKEY;
			evt.hotkey = hotkey;

			window.app.event_func(&evt, window.app.opaque);
		}

	} else {
		if (evt.key.key != MTY_KEY_NONE)
			window.app.event_func(&evt, window.app.opaque);
	}
}

static void window_mod_event(Window *window, NSEvent *event)
{
	MTY_Event evt = window_event(window, MTY_EVENT_KEY);
	evt.key.key = keymap_keycode_to_key(event.keyCode);
	evt.key.mod = keymap_modifier_flags_to_keymod(event.modifierFlags);

	// Macos doesn't send capslock keycodes, so emulate them to act more like windows
	if (event.keyCode == kVK_CapsLock) {
		window_keyboard_event(window, event.keyCode, event.modifierFlags, true);
		window_keyboard_event(window, event.keyCode, event.modifierFlags, false);
	}

	switch (evt.key.key) {
		case MTY_KEY_LSHIFT: evt.key.pressed = evt.key.mod & MTY_MOD_LSHIFT; break;
		case MTY_KEY_LCTRL:  evt.key.pressed = evt.key.mod & MTY_MOD_LCTRL;  break;
		case MTY_KEY_LALT:   evt.key.pressed = evt.key.mod & MTY_MOD_LALT;   break;
		case MTY_KEY_LWIN:   evt.key.pressed = evt.key.mod & MTY_MOD_LWIN;   break;
		case MTY_KEY_RSHIFT: evt.key.pressed = evt.key.mod & MTY_MOD_RSHIFT; break;
		case MTY_KEY_RCTRL:  evt.key.pressed = evt.key.mod & MTY_MOD_RCTRL;  break;
		case MTY_KEY_RALT:   evt.key.pressed = evt.key.mod & MTY_MOD_RALT;   break;
		case MTY_KEY_RWIN:   evt.key.pressed = evt.key.mod & MTY_MOD_RWIN;   break;
		default:
			return;
	}

	window.app.event_func(&evt, window.app.opaque);
}

@implementation Window : NSWindow
	- (BOOL)canBecomeKeyWindow
	{
		return YES;
	}

	- (BOOL)canBecomeMainWindow
	{
		return YES;
	}

	- (BOOL)performKeyEquivalent:(NSEvent *)event
	{
		NSUInteger mods = NSEventModifierFlagControl | NSEventModifierFlagCommand;

		// Allow bypassing some system keys when in immersive
		bool grabbed = self.app.grab_kb;
		bool is_command = (event.modifierFlags & NSEventModifierFlagCommand) ? true : false;
		bool is_command_q = (event.keyCode == kVK_ANSI_Q) && is_command;
		bool is_command_w = (event.keyCode == kVK_ANSI_W) && is_command;
		bool is_command_space = (event.keyCode == kVK_Space) && is_command;

		// macOS swallows Ctrl+Tab and Cmd+Tab, special cases
		bool is_command_tab = (event.keyCode == kVK_Tab) && (event.modifierFlags & mods);

		bool override_hotkey = grabbed && (is_command_q || is_command_w || is_command_space);

		if (override_hotkey || is_command_tab) {
			window_keyboard_event(self, event.keyCode, event.modifierFlags, true);
			window_keyboard_event(self, event.keyCode, event.modifierFlags, false);
		}

		return override_hotkey ? YES : NO;
	}

	- (BOOL)windowShouldClose:(NSWindow *)sender
	{
		MTY_Event evt = window_event(self, MTY_EVENT_CLOSE);
		self.app.event_func(&evt, self.app.opaque);

		return NO;
	}

	- (void)windowDidResignKey:(NSNotification *)notification
	{
		MTY_Event evt = window_event(self, MTY_EVENT_FOCUS);
		evt.focus = false;

		// When in full screen and the window loses focus (changing spaces etc)
		// we need to set the window level back to normal and change the content size.
		// Cmd+Tab behavior and rendering can have weird edge case behvavior when the OS
		// is optimizing the graphics and the window is above the dock
		if (self.styleMask & NSWindowStyleMaskFullScreen) {
			[self setLevel:NSNormalWindowLevel];

			// 1px hack for older versions of macOS
			if (@available(macOS 11.0, *)) {
			} else {
				NSSize cur = self.contentView.frame.size;
				cur.height -= 1.0f;

				[self setContentSize:cur];
			}
		}

		self.app.event_func(&evt, self.app.opaque);
	}

	- (void)windowDidBecomeKey:(NSNotification *)notification
	{
		MTY_Event evt = window_event(self, MTY_EVENT_FOCUS);
		evt.focus = true;

		self.app.event_func(&evt, self.app.opaque);
	}

	- (void)windowDidChangeScreen:(NSNotification *)notification
	{
		// This event fires at the right time to re-apply the window level above the dock
		if (self.top && self.isKeyWindow && (self.styleMask & NSWindowStyleMaskFullScreen)) {
			[self setLevel:NSDockWindowLevel + 1];
			app_apply_cursor(self.app);
		}
	}

	- (void)windowDidResize:(NSNotification *)notification
	{
		MTY_Event evt = window_event(self, MTY_EVENT_SIZE);
		self.app.event_func(&evt, self.app.opaque);
	}

	- (void)windowDidMove:(NSNotification *)notification
	{
		MTY_Event evt = window_event(self, MTY_EVENT_MOVE);
		self.app.event_func(&evt, self.app.opaque);
	}

	- (void)keyUp:(NSEvent *)event
	{
		window_keyboard_event(self, event.keyCode, event.modifierFlags, false);
	}

	- (void)keyDown:(NSEvent *)event
	{
		window_text_event(self, [event.characters UTF8String]);
		window_keyboard_event(self, event.keyCode, event.modifierFlags, true);
	}

	- (void)flagsChanged:(NSEvent *)event
	{
		window_mod_event(self, event);
	}

	- (void)mouseUp:(NSEvent *)event
	{
		window_button_event(self, event, event.buttonNumber, false);
	}

	- (void)mouseDown:(NSEvent *)event
	{
		window_button_event(self, event, event.buttonNumber, true);
	}

	- (void)rightMouseUp:(NSEvent *)event
	{
		window_button_event(self, event, event.buttonNumber, false);
	}

	- (void)rightMouseDown:(NSEvent *)event
	{
		window_button_event(self, event, event.buttonNumber, true);
	}

	- (void)otherMouseUp:(NSEvent *)event
	{
		// Ignore pen event for the middle / X buttons
		window_mouse_button_event(self, event.buttonNumber, false);
	}

	- (void)otherMouseDown:(NSEvent *)event
	{
		// Ignore pen event for the middle / X buttons
		window_mouse_button_event(self, event.buttonNumber, true);
	}

	- (void)mouseMoved:(NSEvent *)event
	{
		window_motion_event(self, event);
	}

	- (void)mouseDragged:(NSEvent *)event
	{
		window_motion_event(self, event);
	}

	- (void)rightMouseDragged:(NSEvent *)event
	{
		window_motion_event(self, event);
	}

	- (void)otherMouseDragged:(NSEvent *)event
	{
		window_motion_event(self, event);
	}

	- (void)mouseEntered:(NSEvent *)event
	{
		self.app.cursor_outside = false;
		app_apply_cursor(self.app);
	}

	- (void)mouseExited:(NSEvent *)event
	{
		self.app.cursor_outside = true;
		app_apply_cursor(self.app);
	}

	- (void)scrollWheel:(NSEvent *)event
	{
		window_scroll_event(self, event);
	}

	- (void)tabletProximity:(NSEvent *)event
	{
		self.app.eraser = event.pointingDeviceType == NSPointingDeviceTypeEraser;
		self.app.pen_left = !event.enteringProximity;
		app_apply_cursor(self.app);
	}

	- (NSApplicationPresentationOptions)window:(NSWindow *)window
		willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
	{
		return self.top ? NSApplicationPresentationFullScreen | NSApplicationPresentationHideDock |
			NSApplicationPresentationHideMenuBar : proposedOptions;
	}

	- (void)windowDidEnterFullScreen:(NSNotification *)notification
	{
		if (self.top)
			[self setLevel:NSDockWindowLevel + 1];
	}

	- (void)windowWillExitFullScreen:(NSNotification *)notification
	{
		self.top = false;
		[self setLevel:NSNormalWindowLevel];
	}
@end


// NSView

@interface View : NSView
	@property(strong) NSTrackingArea *area;
@end

@implementation View : NSView
	- (BOOL)acceptsFirstMouse:(NSEvent *)event
	{
		return YES;
	}

	- (void)updateTrackingAreas
	{
		if (self.area)
			[self removeTrackingArea:self.area];

		NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways;
		self.area = [[NSTrackingArea alloc] initWithRect:self.bounds options:options owner:self.window userInfo:nil];

		[self addTrackingArea:self.area];

		[super updateTrackingAreas];
	}
@end


// App

static void app_hid_connect(struct hid_dev *device, void *opaque)
{
	App *ctx = (__bridge App *) opaque;

	mty_hid_driver_init(device);

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx.event_func(&evt, ctx.opaque);
}

static void app_hid_disconnect(struct hid_dev *device, void *opaque)
{
	App *ctx = (__bridge App *) opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx.event_func(&evt, ctx.opaque);
}

static void app_hid_report(struct hid_dev *device, const void *buf, size_t size, void *opaque)
{
	App *ctx = (__bridge App *) opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONTROLLER;

	if (mty_hid_driver_state(device, buf, size, &evt.controller)) {
		// Prevent gamepad input while in the background
		if (evt.type != MTY_EVENT_NONE && MTY_AppIsActive((MTY_App *) opaque))
			ctx.event_func(&evt, ctx.opaque);
	}
}

static void app_pump_events(App *ctx, NSDate *until)
{
	while (ctx.cont) {
		@autoreleasepool {
			NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
				inMode:NSDefaultRunLoopMode dequeue:YES];

			if (!event)
				break;

			[NSApp sendEvent:event];
		}
	}
}

MTY_App *MTY_AppCreate(MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	App *ctx = [App new];
	MTY_App *app = (__bridge_retained MTY_App *) ctx;

	ctx.app_func = appFunc;
	ctx.event_func = eventFunc;
	ctx.opaque = opaque;
	ctx.cursor_showing = true;
	ctx.cont = true;

	ctx.hid = mty_hid_create(app_hid_connect, app_hid_disconnect, app_hid_report, app);

	ctx.windows = MTY_Alloc(MTY_WINDOW_MAX, sizeof(void *));
	ctx.hotkey = MTY_HashCreate(0);

	ctx.cb_seq = [[NSPasteboard generalPasteboard] changeCount];

	[NSApplication sharedApplication];
	[NSApp setDelegate:ctx];

	// Ensure applicationDidFinishLaunching fires before this function returns
	[NSApp finishLaunching];
	app_pump_events(ctx, nil);

	return app;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	App *ctx = (__bridge_transfer App *) *app;

	MTY_AppStayAwake(*app, false);

	if (ctx.kb_mode) {
		PopSymbolicHotKeyMode(ctx.kb_mode);
		CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyEnable);
		ctx.kb_mode = NULL;
	}

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		MTY_WindowDestroy(*app, x);

	MTY_Free(ctx.windows);

	struct hid *hid = ctx.hid;
	mty_hid_destroy(&hid);
	ctx.hid = NULL;

	MTY_Hash *h = ctx.hotkey;
	MTY_HashDestroy(&h, NULL);
	ctx.hotkey = NULL;

	[NSApp terminate:ctx];
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	app_schedule_func(app);
	app_pump_events(app, [NSDate distantFuture]);
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
	App *app = (__bridge App *) ctx;

	app.timeout = (float) timeout / 1000.0f;
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return [NSApp isActive];
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	App *app = (__bridge App *) ctx;

	if (active) {
		[NSApp unhide:app];

	} else {
		[NSApp hide:app];
	}
}

void MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items, uint32_t len)
{
}

void MTY_AppRemoveTray(MTY_App *ctx)
{
}

void MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg)
{
}

char *MTY_AppGetClipboard(MTY_App *ctx)
{
	char *text = NULL;

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

	NSString *available = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSPasteboardTypeString]];
	if ([available isEqualToString:NSPasteboardTypeString]) {
		NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
		if (string)
			text = MTY_Strdup([string UTF8String]);
	}

	return text;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
	App *app = (__bridge App *) ctx;

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

	app.cb_seq = [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
	[pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	App *app = (__bridge App *) ctx;

	if (app.assertion) {
		IOPMAssertionRelease(app.assertion);
		app.assertion = 0;
	}

	if (enable) {
		IOPMAssertionID assertion = 0;
		IOPMAssertionCreateWithDescription(kIOPMAssertPreventUserIdleDisplaySleep,
			CFSTR("MTY_AppStayAwake"), NULL, NULL, NULL, 0, NULL, &assertion);

		app.assertion = assertion;
	}
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	return app.detach;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
	App *app = (__bridge App *) ctx;

	app.detach = state;

	app_apply_cursor(app);
	app_apply_relative(app);
	app_apply_keyboard_state(app);
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	return app.grab_mouse;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
	App *app = (__bridge App *) ctx;

	app.grab_mouse = grab;
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	return app.relative;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
	App *app = (__bridge App *) ctx;

	app.relative = relative;
	app_apply_relative(app);
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	App *app = (__bridge App *) ctx;

	NSCursor *cursor = nil;

	if (image) {
		NSData *data = [NSData dataWithBytes:image length:size];
		NSImage *nsi = [[NSImage alloc] initWithData:data];

		cursor = [[NSCursor alloc] initWithImage:nsi hotSpot:NSMakePoint(hotX, hotY)];
	}

	app.custom_cursor = cursor;

	app_apply_cursor(app);
}

void MTY_AppUseDefaultCursor(MTY_App *ctx, bool useDefault)
{
	App *app = (__bridge App *) ctx;

	app.default_cursor = useDefault;

	app_apply_cursor(app);
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
	App *app = (__bridge App *) ctx;

	app_show_cursor(app, show);
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return true;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	return app.grab_kb;
}

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	App *app = (__bridge App *) ctx;

	app.grab_kb = grab;
	app_apply_keyboard_state(app);
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	App *app = (__bridge App *) ctx;
	mod &= 0xFF;

	return (uint32_t) (uintptr_t) MTY_HashGetInt(app.hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	App *app = (__bridge App *) ctx;

	mod &= 0xFF;
	MTY_HashSetInt(app.hotkey, (mod << 16) | key, (void *) (uintptr_t) id);
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	App *app = (__bridge App *) ctx;

	MTY_Hash *h = app.hotkey;
	MTY_HashDestroy(&h, NULL);

	app.hotkey = MTY_HashCreate(0);
}

void MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable)
{
}

bool MTY_AppIsSoftKeyboardShowing(MTY_App *ctx)
{
	return false;
}

void MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show)
{
}

MTY_Orientation MTY_AppGetOrientation(MTY_App *ctx)
{
	return MTY_ORIENTATION_USER;
}

void MTY_AppSetOrientation(MTY_App *ctx, MTY_Orientation orientation)
{
}

void MTY_AppRumbleController(MTY_App *ctx, uint32_t id, uint16_t low, uint16_t high)
{
	App *app = (__bridge App *) ctx;

	mty_hid_driver_rumble(app.hid, id, low, high);
}

const void *MTY_AppGetControllerTouchpad(MTY_App *ctx, uint32_t id, size_t *size)
{
	App *app = (__bridge App *) ctx;

	return mty_hid_device_get_touchpad(app.hid, id, size);
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	App *app = (__bridge App *) ctx;

	return app.pen_enabled;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
	App *app = (__bridge App *) ctx;

	app.pen_enabled = enable;
}

MTY_InputMode MTY_AppGetInputMode(MTY_App *ctx)
{
	return MTY_INPUT_MODE_UNSPECIFIED;
}

void MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode)
{
}


// Window

static void window_revert_levels(void)
{
	NSArray<NSWindow *> *windows = [NSApp windows];

	for (uint32_t x = 0; x < windows.count; x++)
		[windows[x] setLevel:NSNormalWindowLevel];
}

MTY_Window MTY_WindowCreate(MTY_App *app, const MTY_WindowDesc *desc)
{
	MTY_Window window = -1;
	bool r = true;

	Window *ctx = nil;
	View *content = nil;
	NSScreen *screen = [NSScreen mainScreen];

	window = app_find_open_window(app, desc->index);
	if (window == -1) {
		r = false;
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		goto except;
	}

	window_revert_levels();

	CGSize size = screen.frame.size;

	int32_t x = desc->x;
	int32_t y = -desc->y;
	int32_t width = size.width;
	int32_t height = size.height;

	wsize_client(desc, 1.0f, size.height, &x, &y, &width, &height);

	if (desc->origin == MTY_ORIGIN_CENTER)
		wsize_center(0, 0, size.width, size.height, &x, &y, &width, &height);

	NSRect rect = NSMakeRect(x, y, width, height);
	NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

	ctx = [[Window alloc] initWithContentRect:rect styleMask:style
		backing:NSBackingStoreBuffered defer:NO screen:screen];
	ctx.title = [NSString stringWithUTF8String:desc->title ? desc->title : "MTY_Window"];
	ctx.window = window;
	ctx.app = (__bridge App *) app;

	[ctx setDelegate:ctx];
	[ctx setAcceptsMouseMovedEvents:YES];
	[ctx setReleasedWhenClosed:NO];
	[ctx setMinSize:NSMakeSize(desc->minWidth, desc->minHeight)];
	[ctx setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

	content = [[View alloc] initWithFrame:[ctx contentRectForFrameRect:ctx.frame]];
	[content setWantsBestResolutionOpenGLSurface:YES];
	[ctx setContentView:content];

	ctx.app.windows[window] = (__bridge void *) ctx;

	if (!desc->hidden)
		MTY_WindowActivate(app, window, true);

	if (desc->api != MTY_GFX_NONE) {
		if (!MTY_WindowSetGFX(app, window, desc->api, desc->vsync)) {
			r = false;
			goto except;
		}
	}

	except:

	if (!r) {
		MTY_WindowDestroy(app, window);
		window = -1;
	}

	return window;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx.app.windows[window] = NULL;
	[ctx close];
}

bool MTY_WindowGetSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	CGSize size = ctx.contentView.frame.size;
	CGFloat scale = mty_screen_scale(ctx.screen);

	*width = lrint(size.width * scale);
	*height = lrint(size.height * scale);

	return true;
}

void MTY_WindowGetPosition(MTY_App *app, MTY_Window window, int32_t *x, int32_t *y)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	*y = lrint(ctx.screen.frame.size.height - ctx.frame.origin.y +
		ctx.screen.frame.origin.y - ctx.frame.size.height);

	*x = lrint(ctx.frame.origin.x - ctx.screen.frame.origin.x);
}

bool MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	CGSize size = ctx.screen.frame.size;
	CGFloat scale = mty_screen_scale(ctx.screen);

	*width = lrint(size.width * scale);
	*height = lrint(size.height * scale);

	return true;
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return 1.0f;

	// macOS scales the display as though it switches resolutions,
	// so all we need to report is the high DPI device multiplier
	return mty_screen_scale(ctx.screen);
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx.title = [NSString stringWithUTF8String:title];
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx.occlusionState & NSWindowOcclusionStateVisible;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx.isKeyWindow;
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (active) {
		[ctx makeKeyAndOrderFront:ctx];

	} else {
		[ctx orderOut:ctx];
	}
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) ? true : false;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx.styleMask & NSWindowStyleMaskFullScreen;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	bool is_fullscreen = MTY_WindowIsFullscreen(app, window);

	if ((!is_fullscreen && fullscreen) || (is_fullscreen && !fullscreen)) {
		ctx.top = fullscreen;
		[ctx toggleFullScreen:ctx];
	}
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	window_warp_cursor(ctx, x, y);
	MTY_AppSetRelativeMouse(app, false);
}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}


// Window Private

void mty_window_set_gfx(MTY_App *app, MTY_Window window, MTY_GFX api, struct gfx_ctx *gfx_ctx)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx.api = api;
	ctx.gfx_ctx = gfx_ctx;
}

MTY_GFX mty_window_get_gfx(MTY_App *app, MTY_Window window, struct gfx_ctx **gfx_ctx)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return MTY_GFX_NONE;

	if (gfx_ctx)
		*gfx_ctx = ctx.gfx_ctx;

	return ctx.api;
}

void *mty_window_get_native(MTY_App *app, MTY_Window window)
{
	Window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return (__bridge void *) ctx;
}


// Misc

static MTY_Atomic32 APP_GLOCK;
static char APP_KEYS[MTY_KEY_MAX][16];

static void app_carbon_key(uint16_t kc, char *text, size_t len)
{
	TISInputSourceRef kb = TISCopyCurrentKeyboardInputSource();
	CFDataRef layout_data = TISGetInputSourceProperty(kb, kTISPropertyUnicodeKeyLayoutData);
	if (layout_data) {
		const UCKeyboardLayout *layout = (const UCKeyboardLayout *) CFDataGetBytePtr(layout_data);

		UInt32 dead_key_state = 0;
		UniCharCount out_len = 0;
		UniChar chars[8];

		UCKeyTranslate(layout, kc, kUCKeyActionDown, 0, LMGetKbdLast(),
			kUCKeyTranslateNoDeadKeysBit, &dead_key_state, 8, &out_len, chars);

		NSString *str = (__bridge_transfer NSString *) CFStringCreateWithCharacters(kCFAllocatorDefault, chars, 1);
		snprintf(text, len, "%s", [[str uppercaseString] UTF8String]);

		CFRelease(kb);
	}
}

static void app_fill_keys(void)
{
	for (uint16_t kc = 0; kc < 0x100; kc++) {
		MTY_Key sc = keymap_keycode_to_key(kc);

		if (sc != MTY_KEY_NONE) {
			const char *text = keymap_keycode_to_text(kc);
			if (text) {
				snprintf(APP_KEYS[sc], 16, "%s", text);

			} else {
				app_carbon_key(kc, APP_KEYS[sc], 16);
			}
		}
	}
}

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	memset(str, 0, len);

	MTY_Strcat(str, len, (mod & MTY_MOD_WIN) ? "Command+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_CTRL) ? "Ctrl+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_ALT) ? "Alt+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_SHIFT) ? "Shift+" : "");

	if (key != MTY_KEY_NONE) {
		if (MTY_Atomic32Get(&APP_GLOCK) == 0) {
			MTY_GlobalLock(&APP_GLOCK);

			if ([NSThread isMainThread]) {
				app_fill_keys();

			} else {
				dispatch_sync(dispatch_get_main_queue(), ^{
					app_fill_keys();
				});
			}

			MTY_GlobalUnlock(&APP_GLOCK);
		}

		MTY_Strcat(str, len, APP_KEYS[key]);
	}
}

void MTY_SetAppID(const char *id)
{
}

void *MTY_GLGetProcAddress(const char *name)
{
	return NULL;
}
