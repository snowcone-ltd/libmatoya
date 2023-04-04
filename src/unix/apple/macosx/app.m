// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"
#include "app-os.h"

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "objc.h"
#include "scale.h"
#include "keymap.h"
#include "hid/hid.h"
#include "hid/utils.h"


// Private global hotkey interface

enum CGSGlobalHotKeyOperatingMode {
	CGSGlobalHotKeyEnable  = 0,
	CGSGlobalHotKeyDisable = 1,
};

int32_t CGSMainConnectionID(void);
CGError CGSSetGlobalHotKeyOperatingMode(int32_t conn, enum CGSGlobalHotKeyOperatingMode mode);


// App

struct window;

struct MTY_App {
	NSObject <NSApplicationDelegate, NSUserNotificationCenterDelegate> *nsapp;
	NSCursor *custom_cursor;
	NSCursor *cursor;
	IOPMAssertionID assertion;
	MTY_AppFunc app_func;
	MTY_EventFunc event_func;
	MTY_Hash *hotkey;
	MTY_Hash *deduper;
	MTY_DetachState detach;
	MTY_Mod hid_kb_mod;
	MTY_Cursor scursor;
	MTY_AppFlag flags;
	void *opaque;
	void *kb_mode;
	bool relative;
	bool grab_mouse;
	bool grab_kb;
	bool cont;
	bool pen_enabled;
	bool default_cursor;
	bool cursor_outside;
	bool cursor_showing;
	bool eraser;
	bool pen_left;
	NSUInteger buttons;
	uint32_t cb_seq;
	struct window *windows[MTY_WINDOW_MAX];
	bool keys[MTY_KEY_MAX];
	float timeout;
	struct hid *hid;
};

static const MTY_Button APP_MOUSE_MAP[] = {
	MTY_BUTTON_LEFT,
	MTY_BUTTON_RIGHT,
	MTY_BUTTON_MIDDLE,
	MTY_BUTTON_X1,
	MTY_BUTTON_X2,
};

#define APP_MOUSE_MAX (sizeof(APP_MOUSE_MAP) / sizeof(MTY_Button))

static void app_schedule_func(MTY_App *ctx)
{
	[NSTimer scheduledTimerWithTimeInterval:ctx->timeout
		target:ctx->nsapp selector:@selector(appFunc:) userInfo:nil repeats:NO];
}

static void app_show_cursor(MTY_App *ctx, bool show)
{
	if (!ctx->cursor_showing && show) {
		[NSCursor unhide];

	} else if (ctx->cursor_showing && !show) {
		if (!ctx->cursor_outside)
			[NSCursor hide];
	}

	ctx->cursor_showing = show;
}

static void app_apply_relative(MTY_App *ctx)
{
	bool rel = ctx->relative && ctx->detach != MTY_DETACH_STATE_FULL;

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

static void app_apply_cursor(MTY_App *ctx)
{
	NSCursor *arrow = [NSCursor arrowCursor];
	NSCursor *scursor = nil;

	switch (ctx->scursor) {
		case MTY_CURSOR_ARROW: scursor = [NSCursor arrowCursor];        break;
		case MTY_CURSOR_HAND:  scursor = [NSCursor pointingHandCursor]; break;
		case MTY_CURSOR_IBEAM: scursor = [NSCursor IBeamCursor];        break;
	}

	NSCursor *new = ctx->cursor_outside || ctx->detach != MTY_DETACH_STATE_NONE ? arrow :
		scursor ? scursor : ctx->custom_cursor ? ctx->custom_cursor : arrow;

	ctx->cursor = new;
	[ctx->cursor set];
}

static void app_apply_keyboard_state(MTY_App *ctx)
{
	if (ctx->grab_kb && ctx->detach == MTY_DETACH_STATE_NONE) {
		// Requires "Enable access for assistive devices" checkbox is checked
		// in the Universal Access preference pane
		if (!ctx->kb_mode) {
			ctx->kb_mode = PushSymbolicHotKeyMode(kHIHotKeyModeAllDisabled);
			CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyDisable);
		}

	} else if (ctx->kb_mode) {
		CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyEnable);
		PopSymbolicHotKeyMode(ctx->kb_mode);
		ctx->kb_mode = NULL;
	}
}

static void app_poll_clipboard(MTY_App *ctx)
{
	uint32_t cb_seq = [[NSPasteboard generalPasteboard] changeCount];

	if (cb_seq > ctx->cb_seq) {
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CLIPBOARD;

		ctx->event_func(&evt, ctx->opaque);
		ctx->cb_seq = cb_seq;
	}
}

static void app_fix_mouse_buttons(MTY_App *ctx)
{
	if (ctx->buttons == 0)
		return;

	NSUInteger buttons = ctx->buttons;
	NSUInteger pressed = [NSEvent pressedMouseButtons];

	for (NSUInteger x = 0; buttons > 0 && x < APP_MOUSE_MAX; x++) {
		if ((buttons & 1) && !(pressed & 1)) {
			MTY_Event evt = {0};
			evt.type = MTY_EVENT_BUTTON;
			evt.button.button = APP_MOUSE_MAP[x];

			ctx->event_func(&evt, ctx->opaque);
			ctx->buttons &= ~(1 << x);
		}

		buttons >>= 1;
		pressed >>= 1;
	}
}


// Class: App

static BOOL app_userNotificationCenter_shouldPresentNotification(id self, SEL _cmd, NSUserNotificationCenter *center,
	NSUserNotification *notification)
{
	return YES;
}

static void app_applicationWillFinishLaunching(id self, SEL _cmd, NSNotification *notification)
{
	[[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];

	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:self
		andSelector:@selector(appHandleGetURLEvent:withReplyEvent:) forEventClass:kInternetEventClass
		andEventID:kAEGetURL];
}

static BOOL app_applicationShouldHandleReopen_hasVisibleWindows(id self, SEL _cmd, NSApplication *sender, BOOL flag)
{
	if ([NSApp windows].count > 0)
		[[NSApp windows][0] makeKeyAndOrderFront:self];

	return YES;
}

static NSApplicationTerminateReply app_applicationShouldTerminate(id self, SEL _cmd, NSApplication *sender)
{
	MTY_App *ctx = OBJC_CTX();

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_QUIT;

	ctx->event_func(&evt, ctx->opaque);

	return NSTerminateCancel;
}

static void app_appQuit(id self, SEL _cmd)
{
	MTY_App *ctx = OBJC_CTX();

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_QUIT;

	ctx->event_func(&evt, ctx->opaque);
}

static void app_appClose(id self, SEL _cmd)
{
	[[NSApp keyWindow] performClose:self];
}

static void app_appRestart(id self, SEL _cmd)
{
	MTY_App *ctx = OBJC_CTX();

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_TRAY;
	evt.trayID = 3; // FIXME Arbitrary!

	ctx->event_func(&evt, ctx->opaque);
}

static void app_appMinimize(id self, SEL _cmd)
{
	[[NSApp keyWindow] miniaturize:self];
}

static void app_appFunc(id self, SEL _cmd, NSTimer *timer)
{
	MTY_App *ctx = OBJC_CTX();

	app_poll_clipboard(ctx);
	app_fix_mouse_buttons(ctx);

	ctx->cont = ctx->app_func(ctx->opaque);

	if (!ctx->cont) {
		// Post a dummy event to spin the event loop
		NSEvent *dummy = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0)
			modifierFlags:0 timestamp:[[NSDate date] timeIntervalSince1970] windowNumber:0
			context:nil subtype:0 data1:0 data2:0];

		[NSApp postEvent:dummy atStart:NO];

	} else {
		app_schedule_func(ctx);
	}
}

static void app_applicationDidFinishLaunching(id self, SEL _cmd, NSNotification *notification)
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

static void app_appHandleGetURLEvent_withReplyEvent(id self, SEL _cmd, NSAppleEventDescriptor *event,
	NSAppleEventDescriptor *replyEvent)
{
	MTY_App *ctx = OBJC_CTX();

	NSString *url = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];

	if (url) {
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_REOPEN;
		evt.reopenArg = [url UTF8String];

		ctx->event_func(&evt, ctx->opaque);
	}
}

static NSMenu *app_applicationDockMenu(id self, SEL _cmd, NSApplication *sender)
{
	NSMenu *menubar = [NSMenu new];
	NSMenuItem *item = [NSMenuItem new];
	NSMenu *menu = [NSMenu new];
	[menubar addItem:item];

	[menu addItem:[[NSMenuItem alloc] initWithTitle:@"Restart" action:@selector(appRestart) keyEquivalent:@""]];
	[item setSubmenu:menu];

	return menu;
}

static Class app_class(void)
{
	Class cls = objc_getClass(APP_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSObject", APP_CLASS_NAME);

	// NSApplicationDelegate
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(NSApplicationDelegate));
	if (proto) {
		OBJC_POVERRIDE(cls, proto, NO, @selector(applicationWillFinishLaunching:), app_applicationWillFinishLaunching);
		OBJC_POVERRIDE(cls, proto, NO, @selector(applicationShouldHandleReopen:hasVisibleWindows:),
			app_applicationShouldHandleReopen_hasVisibleWindows);
		OBJC_POVERRIDE(cls, proto, NO, @selector(applicationShouldTerminate:), app_applicationShouldTerminate);
		OBJC_POVERRIDE(cls, proto, NO, @selector(applicationDidFinishLaunching:), app_applicationDidFinishLaunching);
		OBJC_POVERRIDE(cls, proto, NO, @selector(applicationDockMenu:), app_applicationDockMenu);
	}

	// NSUserNotificationCenterDelegate
	proto = OBJC_PROTOCOL(cls, @protocol(NSUserNotificationCenterDelegate));
	if (proto)
		OBJC_POVERRIDE(cls, proto, NO, @selector(userNotificationCenter:shouldPresentNotification:),
			app_userNotificationCenter_shouldPresentNotification);

	// New methods
	class_addMethod(cls, @selector(appQuit), (IMP) app_appQuit, "v@:");
	class_addMethod(cls, @selector(appClose), (IMP) app_appClose, "v@:");
	class_addMethod(cls, @selector(appRestart), (IMP) app_appRestart, "v@:");
	class_addMethod(cls, @selector(appMinimize), (IMP) app_appMinimize, "v@:");
	class_addMethod(cls, @selector(appFunc:), (IMP) app_appFunc, "v@:@");
	class_addMethod(cls, @selector(appHandleGetURLEvent:withReplyEvent:),
		(IMP) app_appHandleGetURLEvent_withReplyEvent, "v@:@@");

	objc_registerClassPair(cls);

	return cls;
}


// Window

struct window {
	NSWindow <NSWindowDelegate> *nsw;
	NSTrackingArea *area;
	MTY_App *app;
	struct window_common cmn;
	MTY_Window window;
	NSRect normal_frame;
	bool was_maximized;
	bool top;
};

static struct window *app_get_window(MTY_App *ctx, MTY_Window window)
{
	return window < 0 ? NULL : ctx->windows[window];
}

static struct window *app_get_window_by_number(MTY_App *ctx, NSInteger number)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (ctx->windows[x] && ctx->windows[x]->nsw.windowNumber == number)
			return ctx->windows[x];

	return NULL;
}

static MTY_Window app_find_open_window(MTY_App *ctx, MTY_Window req)
{
	if (req >= 0 && req < MTY_WINDOW_MAX && !ctx->windows[req])
		return req;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (!ctx->windows[x])
			return x;

	return -1;
}


// Screen helpers

static CGDirectDisplayID screen_get_display_id(NSScreen *screen)
{
	return ((NSNumber *) [screen deviceDescription][@"NSScreenNumber"]).intValue;
}

static NSScreen *screen_get_primary(void)
{
	return [NSScreen screens][0];
}

static NSScreen *screen_from_display_id(CGDirectDisplayID display_id)
{
	NSArray<NSScreen *> *screens = [NSScreen screens];

	for (uint32_t x = 0; x < screens.count; x++)
		if (display_id == screen_get_display_id(screens[x]))
			return screens[x];

	return screen_get_primary();
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

static struct window *window_find_mouse(struct window *ctx, NSPoint *p)
{
	// Find the window where the cursor resides, accounting for overlapping windows
	// and non-client areas
	struct window *top = NULL;
	bool key_hit = false;
	NSArray<NSWindow *> *windows = [NSApp windows];

	for (uint32_t x = 0; x < windows.count; x++) {
		struct window *window = app_get_window_by_number(ctx->app, windows[x].windowNumber);
		if (!window)
			continue;

		NSPoint wp = {0};
		if (window_event_in_view(window->nsw, &wp)) {
			if (window->nsw.isKeyWindow && window->nsw.windowNumber == ctx->nsw.windowNumber) {
				*p = wp;
				return window;
			}

			if (!top && !key_hit) {
				if (!window_event_in_nc([NSApp keyWindow])) {
					*p = wp;
					top = window;
				}
			}

			if (window->nsw.isKeyWindow) {
				key_hit = true;
				top = NULL;
			}
		}
	}

	return top;
}


// Pen

static void window_pen_event(struct window *ctx, NSEvent *event, bool pressed)
{
	NSPoint p = {0};
	struct window *cur = window_find_mouse(ctx, &p);
	if (!cur)
		return;

	CGFloat scale = mty_screen_scale(cur->nsw.screen);

	MTY_Event evt = {
		.type = MTY_EVENT_PEN,
		.window = ctx->window,
		.pen.pressure = (uint16_t) lrint(event.pressure * 1024.0f),
		.pen.rotation = (uint16_t) lrint(event.rotation * 359.0f),
		.pen.tiltX = (int8_t) lrint(event.tilt.x * 90.0f),
		.pen.tiltY = (int8_t) lrint(event.tilt.y * -90.0f),
		.pen.x = lrint(p.x * scale),
		.pen.y = lrint(p.y * scale),
	};

	bool touching = event.buttonMask & NSEventButtonMaskPenTip || pressed;

	// INVERTED must be set while hovering, but ERASER should only be set by
	// while TOUCHING is also true
	if (ctx->app->eraser) {
		evt.pen.flags |= MTY_PEN_FLAG_INVERTED;

		if (touching) {
			evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;
			evt.pen.flags |= MTY_PEN_FLAG_ERASER;
		}

	} else if (touching) {
		evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;
	}

	// LEAVE is set when the pen moves out of the tracking area (only once)
	if (ctx->app->pen_left) {
		evt.pen.flags |= MTY_PEN_FLAG_LEAVE;
		ctx->app->pen_left = false;
	}

	ctx->app->event_func(&evt, ctx->app->opaque);
}


// Mouse

static void window_mouse_button_event(struct window *ctx, NSUInteger index, bool pressed)
{
	NSPoint p = {0};
	struct window *cur = window_find_mouse(ctx, &p);
	if (!cur)
		return;

	if (index >= APP_MOUSE_MAX)
		return;

	CGFloat scale = mty_screen_scale(cur->nsw.screen);

	MTY_Event evt = {
		.type = MTY_EVENT_BUTTON,
		.window = ctx->window,
		.button.button = APP_MOUSE_MAP[index],
		.button.pressed = pressed,
		.button.x = lrint(scale * p.x),
		.button.y = lrint(scale * p.y),
	};

	ctx->app->event_func(&evt, ctx->app->opaque);

	if (pressed) {
		ctx->app->buttons |= 1 << index;

	} else {
		ctx->app->buttons &= ~(1 << index);
	}
}

static void window_button_event(struct window *ctx, NSEvent *event, NSUInteger index, bool pressed)
{
	// An index of zero indicates a pen event
	bool is_pen_press = event.buttonMask & NSEventButtonMaskPenTip || index == 0;

	if (ctx->app->pen_enabled && event.subtype == NSEventSubtypeTabletPoint && is_pen_press) {
		window_pen_event(ctx, event, pressed);

	} else {
		window_mouse_button_event(ctx, index, pressed);
	}
}

static void window_warp_cursor(NSWindow *ctx, uint32_t x, int32_t y)
{
	CGDirectDisplayID display = screen_get_display_id(ctx.screen);

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

static void window_mouse_motion_event(struct window *ctx, NSEvent *event, bool pen_in_range)
{
	if (ctx->app->relative && ctx->app->detach == MTY_DETACH_STATE_NONE && !pen_in_range) {
		MTY_Event evt = {
			.type = MTY_EVENT_MOTION,
			.window = ctx->window,
			.motion.relative = true,
		};

		CGGetLastMouseDelta(&evt.motion.x, &evt.motion.y);

		ctx->app->event_func(&evt, ctx->app->opaque);

	} else {
		NSPoint p = {0};
		struct window *cur = window_find_mouse(ctx, &p);

		if (cur) {
			if (ctx->app->grab_mouse && ctx->app->detach == MTY_DETACH_STATE_NONE && !cur->nsw.isKeyWindow) {
				window_confine_cursor();

			} else if (cur->nsw.occlusionState & NSWindowOcclusionStateVisible) {
				CGFloat scale = mty_screen_scale(cur->nsw.screen);

				MTY_Event evt = {
					.type = MTY_EVENT_MOTION,
					.window = ctx->window,
					.motion.relative = false,
					.motion.x = lrint(scale * p.x),
					.motion.y = lrint(scale * p.y),
				};

				ctx->app->event_func(&evt, ctx->app->opaque);
			}

		} else if (ctx->app->grab_mouse && ctx->app->detach == MTY_DETACH_STATE_NONE) {
			window_confine_cursor();
		}
	}
}

static void window_motion_event(struct window *ctx, NSEvent *event)
{
	bool pen_in_range = event.subtype == NSEventSubtypeTabletPoint;

	if (ctx->app->pen_enabled && pen_in_range) {
		window_pen_event(ctx, event, false);

	} else {
		window_mouse_motion_event(ctx, event, pen_in_range);
	}
}

static void window_scroll_event(struct window *ctx, NSEvent *event)
{
	CGFloat scale = mty_screen_scale(ctx->nsw.screen);
	int32_t delta = event.hasPreciseScrollingDeltas ? scale : scale * 80.0f;

	MTY_Event evt = {
		.type = MTY_EVENT_SCROLL,
		.window = ctx->window,
		.scroll.x = lrint(-event.scrollingDeltaX * delta),
		.scroll.y = lrint(event.scrollingDeltaY * delta),
	};

	ctx->app->event_func(&evt, ctx->app->opaque);
}


// Keyboard

static void window_text_event(struct window *ctx, const char *text)
{
	// Make sure visible ASCII
	if (text && text[0] && text[0] >= 0x20 && text[0] != 0x7F) {
		MTY_Event evt = {
			.type = MTY_EVENT_TEXT,
			.window = ctx->window,
		};

		snprintf(evt.text, 8, "%s", text);

		ctx->app->event_func(&evt, ctx->app->opaque);
	}
}

static void window_keyboard_event(struct window *ctx, uint16_t key_code, NSEventModifierFlags flags,
	bool pressed, bool repeat)
{
	MTY_Event evt = {
		.type = MTY_EVENT_KEY,
		.window = ctx->window,
		.key.key = keymap_keycode_to_key(key_code),
		.key.vkey = key_code,
		.key.mod = keymap_modifier_flags_to_keymod(flags),
		.key.pressed = pressed,
	};

	if (!mty_app_dedupe_key(ctx->app, evt.key.key, pressed, repeat))
		return;

	mty_app_kb_to_hotkey(ctx->app, &evt, MTY_EVENT_HOTKEY);

	if ((evt.type == MTY_EVENT_HOTKEY && pressed) || (evt.type == MTY_EVENT_KEY && evt.key.key != MTY_KEY_NONE))
		ctx->app->event_func(&evt, ctx->app->opaque);
}

static bool window_flags_changed(uint16_t key_code, NSEventModifierFlags flags)
{
	switch (key_code) {
		case kVK_Shift: return flags & NS_MOD_LSHIFT;
		case kVK_Control: return flags & NS_MOD_LCTRL;
		case kVK_Option: return flags & NS_MOD_LALT;
		case kVK_Command: return flags & NS_MOD_LCMD;
		case kVK_RightShift: return flags & NS_MOD_RSHIFT;
		case kVK_RightControl: return flags & NS_MOD_RCTRL;
		case kVK_RightOption: return flags & NS_MOD_RALT;
		case kVK_RightCommand: return flags & NS_MOD_RCMD;
		case kVK_CapsLock: return flags & NSEventModifierFlagCapsLock;
	}

	return false;
}


// Class: Window

static BOOL window_canBecomeKeyWindow(NSWindow *self, SEL _cmd)
{
	return YES;
}

static BOOL window_canBecomeMainWindow(NSWindow *self, SEL _cmd)
{
	return YES;
}

static NSRect window_windowWillUseStandardFrame_defaultFrame(NSWindow *self, SEL _cmd, NSWindow *window, NSRect newFrame)
{
	struct window *ctx = OBJC_CTX();

	if (!NSEqualRects(window.frame, newFrame))
		ctx->normal_frame = window.frame;

	return newFrame;
}

static BOOL window_performKeyEquivalent(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	bool cmd = event.modifierFlags & NSEventModifierFlagCommand;
	bool ctrl = event.modifierFlags & NSEventModifierFlagControl;

	bool cmd_tab = event.keyCode == kVK_Tab && cmd;
	bool ctrl_tab = event.keyCode == kVK_Tab && ctrl;
	bool cmd_q = event.keyCode == kVK_ANSI_Q && cmd;
	bool cmd_w = event.keyCode == kVK_ANSI_W && cmd;
	bool cmd_space = event.keyCode == kVK_Space && cmd;

	// While keyboard is grabbed, make sure we pass through special OS hotkeys
	if (ctx->app->grab_kb && (cmd_tab || ctrl_tab || cmd_q || cmd_w || cmd_space)) {
		if (!(ctx->app->flags & MTY_APP_FLAG_HID_KEYBOARD)) {
			window_keyboard_event(ctx, event.keyCode, event.modifierFlags, true, true);
			window_keyboard_event(ctx, event.keyCode, event.modifierFlags, false, true);
		}

		return YES;
	}

	return NO;
}

static BOOL window_windowShouldClose(NSWindow *self, SEL _cmd, NSWindow *sender)
{
	struct window *ctx = OBJC_CTX();

	MTY_Event evt = {
		.type = MTY_EVENT_CLOSE,
		.window = ctx->window,
	};

	ctx->app->event_func(&evt, ctx->app->opaque);

	return NO;
}

static void window_windowDidResignKey(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	MTY_Event evt = {
		.type = MTY_EVENT_FOCUS,
		.window = ctx->window,
		.focus = false,
	};

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

	ctx->app->event_func(&evt, ctx->app->opaque);
}

static void window_windowDidBecomeKey(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	MTY_Event evt = {
		.type = MTY_EVENT_FOCUS,
		.window = ctx->window,
		.focus = true,
	};

	ctx->app->event_func(&evt, ctx->app->opaque);
}

static void window_windowDidChangeScreen(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	// This event fires at the right time to re-apply the window level above the dock
	if (ctx->top && self.isKeyWindow && (self.styleMask & NSWindowStyleMaskFullScreen)) {
		[self setLevel:NSMainMenuWindowLevel - 1];
		app_apply_cursor(ctx->app);
	}
}

static void window_windowDidResize(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	MTY_Event evt = {
		.type = MTY_EVENT_SIZE,
		.window = ctx->window,
	};

	ctx->app->event_func(&evt, ctx->app->opaque);

	if (ctx->cmn.webview)
		mty_webview_event(ctx->cmn.webview, &evt);
}

static void window_windowDidMove(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	MTY_Event evt = {
		.type = MTY_EVENT_MOVE,
		.window = ctx->window,
	};

	ctx->app->event_func(&evt, ctx->app->opaque);
}

static void window_keyUp(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	window_keyboard_event(ctx, event.keyCode, event.modifierFlags, false, false);
}

static void window_keyDown(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	window_text_event(ctx, [event.characters UTF8String]);
	window_keyboard_event(ctx, event.keyCode, event.modifierFlags, true, event.isARepeat);
}

static void window_flagsChanged(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	// Simulate full button press for the Caps Lock key
	if (event.keyCode == kVK_CapsLock) {
		if (!(ctx->app->flags & MTY_APP_FLAG_HID_KEYBOARD)) {
			window_keyboard_event(ctx, event.keyCode, event.modifierFlags, true, true);
			window_keyboard_event(ctx, event.keyCode, event.modifierFlags, false, true);
		}

	} else {
		bool pressed = window_flags_changed(event.keyCode, event.modifierFlags);
		window_keyboard_event(ctx, event.keyCode, event.modifierFlags, pressed, false);
	}
}

static void window_mouseUp(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_button_event(OBJC_CTX(), event, event.buttonNumber, false);
}

static void window_mouseDown(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_button_event(OBJC_CTX(), event, event.buttonNumber, true);
}

static void window_rightMouseUp(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_button_event(OBJC_CTX(), event, event.buttonNumber, false);
}

static void window_rightMouseDown(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_button_event(OBJC_CTX(), event, event.buttonNumber, true);
}

static void window_otherMouseUp(NSWindow *self, SEL _cmd, NSEvent *event)
{
	// Ignore pen event for the middle / X buttons
	window_mouse_button_event(OBJC_CTX(), event.buttonNumber, false);
}

static void window_otherMouseDown(NSWindow *self, SEL _cmd, NSEvent *event)
{
	// Ignore pen event for the middle / X buttons
	window_mouse_button_event(OBJC_CTX(), event.buttonNumber, true);
}

static void window_mouseMoved(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_motion_event(OBJC_CTX(), event);
}

static void window_mouseDragged(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_motion_event(OBJC_CTX(), event);
}

static void window_rightMouseDragged(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_motion_event(OBJC_CTX(), event);
}

static void window_otherMouseDragged(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_motion_event(OBJC_CTX(), event);
}

static void window_mouseEntered(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	ctx->app->cursor_outside = false;
	app_apply_cursor(ctx->app);
}

static void window_mouseExited(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	ctx->app->cursor_outside = true;
	app_apply_cursor(ctx->app);
}

static void window_scrollWheel(NSWindow *self, SEL _cmd, NSEvent *event)
{
	window_scroll_event(OBJC_CTX(), event);
}

static void window_tabletProximity(NSWindow *self, SEL _cmd, NSEvent *event)
{
	struct window *ctx = OBJC_CTX();

	ctx->app->eraser = event.pointingDeviceType == NSPointingDeviceTypeEraser;
	ctx->app->pen_left = !event.enteringProximity;
	app_apply_cursor(ctx->app);
}

static NSApplicationPresentationOptions window_window_willUseFullScreenPresentationOptions(NSWindow *self, SEL _cmd,
	NSWindow *window, NSApplicationPresentationOptions proposedOptions)
{
	struct window *ctx = OBJC_CTX();

	if (![self isZoomed]) {
		ctx->normal_frame = window.frame;
		ctx->was_maximized = false;

	} else {
		ctx->was_maximized = true;
	}

	return ctx->top ? NSApplicationPresentationFullScreen | NSApplicationPresentationHideDock |
		NSApplicationPresentationHideMenuBar : proposedOptions;
}

static void window_windowDidEnterFullScreen(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	if (ctx->top)
		[self setLevel:NSMainMenuWindowLevel - 1];
}

static void window_windowWillExitFullScreen(NSWindow *self, SEL _cmd, NSNotification *notification)
{
	struct window *ctx = OBJC_CTX();

	ctx->top = false;
	[self setLevel:NSNormalWindowLevel];
}

static Class window_class(void)
{
	Class cls = objc_getClass(WINDOW_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSWindow", WINDOW_CLASS_NAME);

	// NSWindowDelegate
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(NSWindowDelegate));
	if (proto) {
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowWillUseStandardFrame:defaultFrame:),
			window_windowWillUseStandardFrame_defaultFrame);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowShouldClose:), window_windowShouldClose);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidResignKey:), window_windowDidResignKey);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidBecomeKey:), window_windowDidBecomeKey);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidChangeScreen:), window_windowDidChangeScreen);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidResize:), window_windowDidResize);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidMove:), window_windowDidMove);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowDidEnterFullScreen:), window_windowDidEnterFullScreen);
		OBJC_POVERRIDE(cls, proto, NO, @selector(windowWillExitFullScreen:), window_windowWillExitFullScreen);
		OBJC_POVERRIDE(cls, proto, NO, @selector(window:willUseFullScreenPresentationOptions:),
			window_window_willUseFullScreenPresentationOptions);
	}

	// Overrides
	OBJC_OVERRIDE(cls, @selector(canBecomeKeyWindow), window_canBecomeKeyWindow);
	OBJC_OVERRIDE(cls, @selector(canBecomeMainWindow), window_canBecomeMainWindow);
	OBJC_OVERRIDE(cls, @selector(performKeyEquivalent:), window_performKeyEquivalent);
	OBJC_OVERRIDE(cls, @selector(keyUp:), window_keyUp);
	OBJC_OVERRIDE(cls, @selector(keyDown:), window_keyDown);
	OBJC_OVERRIDE(cls, @selector(flagsChanged:), window_flagsChanged);
	OBJC_OVERRIDE(cls, @selector(mouseUp:), window_mouseUp);
	OBJC_OVERRIDE(cls, @selector(mouseDown:), window_mouseDown);
	OBJC_OVERRIDE(cls, @selector(rightMouseUp:), window_rightMouseUp);
	OBJC_OVERRIDE(cls, @selector(rightMouseDown:), window_rightMouseDown);
	OBJC_OVERRIDE(cls, @selector(otherMouseUp:), window_otherMouseUp);
	OBJC_OVERRIDE(cls, @selector(otherMouseDown:), window_otherMouseDown);
	OBJC_OVERRIDE(cls, @selector(mouseMoved:), window_mouseMoved);
	OBJC_OVERRIDE(cls, @selector(mouseDragged:), window_mouseDragged);
	OBJC_OVERRIDE(cls, @selector(rightMouseDragged:), window_rightMouseDragged);
	OBJC_OVERRIDE(cls, @selector(otherMouseDragged:), window_otherMouseDragged);
	OBJC_OVERRIDE(cls, @selector(mouseEntered:), window_mouseEntered);
	OBJC_OVERRIDE(cls, @selector(mouseExited:), window_mouseExited);
	OBJC_OVERRIDE(cls, @selector(scrollWheel:), window_scrollWheel);
	OBJC_OVERRIDE(cls, @selector(tabletProximity:), window_tabletProximity);

	objc_registerClassPair(cls);

	return cls;
}


// Class: View

static BOOL view_acceptsFirstMouse(NSView *self, SEL _cmd, NSEvent *event)
{
	return YES;
}

static void view_updateTrackingAreas(NSView *self, SEL _cmd)
{
	struct window *ctx = OBJC_CTX();

	if (ctx->area)
		[self removeTrackingArea:ctx->area];

	NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways;
	ctx->area = [[NSTrackingArea alloc] initWithRect:self.bounds options:options owner:self.window userInfo:nil];

	[self addTrackingArea:ctx->area];

	OBJC_METHOD super_updateTrackingAreas = (OBJC_METHOD) class_getMethodImplementation(self.superclass, _cmd);
	super_updateTrackingAreas(self, _cmd);
}

static Class view_class(void)
{
	Class cls = objc_getClass(VIEW_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSView", VIEW_CLASS_NAME);

	// Overrides
	OBJC_OVERRIDE(cls, @selector(acceptsFirstMouse:), view_acceptsFirstMouse);
	OBJC_OVERRIDE(cls, @selector(updateTrackingAreas), view_updateTrackingAreas);

	return cls;
}


// App Public

static void app_hid_connect(struct hid_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	mty_hid_driver_init(device);

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_hid_disconnect(struct hid_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_hid_report(struct hid_dev *device, const void *buf, size_t size, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONTROLLER;

	if (mty_hid_driver_state(device, buf, size, &evt.controller)) {
		if (ctx->flags & MTY_APP_FLAG_HID_EVENTS)
			ctx->event_func(&(MTY_Event) {
				.type = MTY_EVENT_HID,
				.hid.size = size,
				.hid.report = buf,
				.hid.type = evt.controller.type,
				.hid.vid = evt.controller.vid,
				.hid.pid = evt.controller.pid,
				.hid.id = evt.controller.id,
			}, ctx->opaque);

		// Prevent gamepad input while in the background, dedupe
		if (MTY_AppIsActive(ctx) && mty_hid_dedupe(ctx->deduper, &evt.controller))
			ctx->event_func(&evt, ctx->opaque);
	}
}

static void app_hid_key(uint32_t usage, bool down, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Key key = keymap_usage_to_key(usage);
	if (key == MTY_KEY_NONE)
		return;

	MTY_Mod mod = keymap_usage_to_mod(usage);

	if (down) {
		ctx->hid_kb_mod |= mod;

	} else {
		ctx->hid_kb_mod &= ~mod;
	}

	if (!MTY_AppIsActive(ctx))
		return;

	struct window *window0 = ctx->windows[0];

	if (window0 && window0->cmn.webview && mty_webview_is_visible(window0->cmn.webview))
		return;

	MTY_Event evt = {
		.type = MTY_EVENT_KEY,
		.key.key = key,
		.key.mod = ctx->hid_kb_mod,
		.key.pressed = down,
	};

	if (!mty_app_dedupe_key(ctx, evt.key.key, down, false))
		return;

	mty_app_kb_to_hotkey(ctx, &evt, MTY_EVENT_HOTKEY);
	ctx->event_func(&evt, ctx->opaque);
}

static void app_pump_events(MTY_App *ctx, NSDate *until)
{
	while (ctx->cont) {
		@autoreleasepool {
			NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
				inMode:NSDefaultRunLoopMode dequeue:YES];

			if (!event)
				break;

			[NSApp sendEvent:event];
		}
	}
}

MTY_App *MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));

	ctx->flags = flags;
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;
	ctx->cursor_showing = true;
	ctx->cont = true;

	ctx->hid = mty_hid_create(app_hid_connect, app_hid_disconnect, app_hid_report,
		ctx->flags & MTY_APP_FLAG_HID_KEYBOARD ? app_hid_key : NULL, ctx);

	ctx->hotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);

	ctx->cb_seq = [[NSPasteboard generalPasteboard] changeCount];

	[NSApplication sharedApplication];

	ctx->nsapp = OBJC_NEW(app_class(), ctx);
	[NSApp setDelegate:ctx->nsapp];

	// Ensure applicationDidFinishLaunching fires before this function returns
	[NSApp finishLaunching];
	app_pump_events(ctx, nil);

	return ctx;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	MTY_App *ctx = *app;

	MTY_AppStayAwake(ctx, false);

	if (ctx->kb_mode) {
		CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), CGSGlobalHotKeyEnable);
		PopSymbolicHotKeyMode(ctx->kb_mode);
	}

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		MTY_WindowDestroy(ctx, x);

	mty_hid_destroy(&ctx->hid);

	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_HashDestroy(&ctx->deduper, MTY_Free);

	[NSApp terminate:ctx->nsapp];
	ctx->nsapp = nil;
	ctx->custom_cursor = nil;
	ctx->cursor = nil;

	MTY_Free(ctx);
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
	app_schedule_func(ctx);
	app_pump_events(ctx, [NSDate distantFuture]);
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
	ctx->timeout = (float) timeout / 1000.0f;
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return [NSApp isActive];
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	if (active) {
		[NSApp unhide:ctx->nsapp];

	} else {
		[NSApp hide:ctx->nsapp];
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
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

	ctx->cb_seq = [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
	[pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	if (ctx->assertion) {
		IOPMAssertionRelease(ctx->assertion);
		ctx->assertion = 0;
	}

	if (enable) {
		IOPMAssertionID assertion = 0;
		IOPMAssertionCreateWithDescription(kIOPMAssertPreventUserIdleDisplaySleep,
			CFSTR("MTY_AppStayAwake"), NULL, NULL, NULL, 0, NULL, &assertion);

		ctx->assertion = assertion;
	}
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return ctx->detach;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
	ctx->detach = state;

	app_apply_cursor(ctx);
	app_apply_relative(ctx);
	app_apply_keyboard_state(ctx);
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return ctx->grab_mouse;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
	ctx->grab_mouse = grab;
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return ctx->relative;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
	ctx->relative = relative;
	app_apply_relative(ctx);
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	NSCursor *cursor = nil;

	if (image) {
		NSData *data = [NSData dataWithBytes:image length:size];
		NSImage *nsi = [[NSImage alloc] initWithData:data];

		cursor = [[NSCursor alloc] initWithImage:nsi hotSpot:NSMakePoint(hotX, hotY)];
	}

	ctx->custom_cursor = cursor;

	app_apply_cursor(ctx);
}

void MTY_AppSetCursor(MTY_App *ctx, MTY_Cursor cursor)
{
	ctx->scursor = cursor;

	app_apply_cursor(ctx);
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
	app_show_cursor(ctx, show);
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return true;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return ctx->grab_kb;
}

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	ctx->grab_kb = grab;
	app_apply_keyboard_state(ctx);
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	mod &= 0xFF;

	return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	mod &= 0xFF;
	MTY_HashSetInt(ctx->hotkey, (mod << 16) | key, (void *) (uintptr_t) id);
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	MTY_HashDestroy(&ctx->hotkey, NULL);
	ctx->hotkey = MTY_HashCreate(0);
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
	mty_hid_driver_rumble(ctx->hid, id, low, high);
}

const char *MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id)
{
	struct hid_dev *device = mty_hid_get_device_by_id(ctx->hid, id);
	if (!device)
		return NULL;

	return mty_hid_device_get_name(device);
}

MTY_CType MTY_AppGetControllerType(MTY_App *ctx, uint32_t id)
{
	struct hid_dev *device = mty_hid_get_device_by_id(ctx->hid, id);
	if (!device)
		return MTY_CTYPE_DEFAULT;

	return hid_driver(device);
}

void MTY_AppSubmitHIDReport(MTY_App *ctx, uint32_t id, const void *report, size_t size)
{
	struct hid_dev *dev = mty_hid_get_device_by_id(ctx->hid, id);
	if (!dev)
		return;

	mty_hid_device_write(dev, report, size);
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	return ctx->pen_enabled;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
	ctx->pen_enabled = enable;
}

MTY_InputMode MTY_AppGetInputMode(MTY_App *ctx)
{
	return MTY_INPUT_MODE_UNSPECIFIED;
}

void MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode)
{
}

void MTY_AppSetWMsgFunc(MTY_App *ctx, MTY_WMsgFunc func)
{
}


// Window Public

static void window_revert_levels(void)
{
	NSArray<NSWindow *> *windows = [NSApp windows];

	for (uint32_t x = 0; x < windows.count; x++)
		[windows[x] setLevel:NSNormalWindowLevel];
}

MTY_Window MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index)
{
	MTY_Window window = -1;
	bool r = true;

	MTY_Frame dframe = {0};

	if (!frame) {
		dframe = APP_DEFAULT_FRAME();
		frame = &dframe;
	}

	NSView *content = nil;
	NSScreen *screen = screen_from_display_id(atoi(frame->screen));

	window = app_find_open_window(app, index);
	if (window == -1) {
		r = false;
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		goto except;
	}

	window_revert_levels();

	NSRect rect = NSMakeRect(frame->x, frame->y, frame->size.w, frame->size.h);
	NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

	// Window
	struct window *ctx = MTY_Alloc(1, sizeof(struct window));
	ctx->window = window;
	ctx->app = app;

	ctx->nsw = [OBJC_NEW(window_class(), ctx) initWithContentRect:rect styleMask:style
		backing:NSBackingStoreBuffered defer:NO screen:screen];
	ctx->nsw.title = [NSString stringWithUTF8String:title ? title : "MTY_Window"];

	[ctx->nsw setDelegate:ctx->nsw];
	[ctx->nsw setAcceptsMouseMovedEvents:YES];
	[ctx->nsw setReleasedWhenClosed:NO];
	[ctx->nsw setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

	// View
	content = [OBJC_NEW(view_class(), ctx) initWithFrame:[ctx->nsw contentRectForFrameRect:ctx->nsw.frame]];
	[content setWantsBestResolutionOpenGLSurface:YES];
	[ctx->nsw setContentView:content];

	ctx->app->windows[window] = ctx;

	if (frame->type & MTY_WINDOW_MAXIMIZED)
		[ctx->nsw zoom:ctx->nsw];

	if (frame->type & MTY_WINDOW_FULLSCREEN)
		MTY_WindowSetFullscreen(app, window, true);

	if (!(frame->type & MTY_WINDOW_HIDDEN))
		MTY_WindowActivate(app, window, true);

	except:

	if (!r) {
		MTY_WindowDestroy(app, window);
		window = -1;
	}

	return window;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx->app->windows[window] = NULL;

	[ctx->nsw close];

	// XXX Make sure this is freed after the window is closed, events
	// can continue to fire until that point
	mty_webview_destroy(&ctx->cmn.webview);

	ctx->nsw = nil;
	ctx->area = nil;

	MTY_Free(ctx);
}

MTY_Size MTY_WindowGetSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	NSSize size = ctx->nsw.contentView.frame.size;
	CGFloat scale = mty_screen_scale(ctx->nsw.screen);

	return (MTY_Size) {
		.w = lrint(size.width * scale),
		.h = lrint(size.height * scale),
	};
}

MTY_Frame MTY_WindowGetFrame(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Frame) {0};

	NSRect s = ctx->nsw.screen.frame;
	NSRect w = ctx->nsw.frame;

	MTY_WindowType type = MTY_WINDOW_NORMAL;

	if ([ctx->nsw isZoomed]) {
		w = ctx->normal_frame;

		if (ctx->nsw.styleMask & NSWindowStyleMaskFullScreen) {
			type = MTY_WINDOW_FULLSCREEN;

			if (ctx->was_maximized)
				type |= MTY_WINDOW_MAXIMIZED;

		} else {
			type = MTY_WINDOW_MAXIMIZED;
		}
	}

	NSRect r = [ctx->nsw contentRectForFrameRect: (NSRect) {
		.origin.x = w.origin.x - s.origin.x,
		.origin.y = w.origin.y - s.origin.y,
		.size.width = w.size.width,
		.size.height = w.size.height,
	}];

	MTY_Frame frame = {
		.type = type,
		.x = r.origin.x,
		.y = r.origin.y,
		.size.w = r.size.width,
		.size.h = r.size.height,
	};

	snprintf(frame.screen, MTY_SCREEN_MAX, "%d", screen_get_display_id(ctx->nsw.screen));

	return frame;
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	NSScreen *screen = screen_from_display_id(atoi(frame->screen));
	NSRect s = screen.frame;

	NSRect r = [ctx->nsw frameRectForContentRect: (NSRect) {
		.origin.x = frame->x + s.origin.x,
		.origin.y = frame->y + s.origin.y,
		.size.width = frame->size.w,
		.size.height = frame->size.h,
	}];

	[ctx->nsw setFrame:r display:YES];

	if (frame->type & MTY_WINDOW_MAXIMIZED)
		[ctx->nsw zoom:ctx->nsw];

	if (frame->type & MTY_WINDOW_FULLSCREEN)
		MTY_WindowSetFullscreen(app, window, true);
}

void MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	[ctx->nsw setMinSize:NSMakeSize(minWidth, minHeight)];
}

MTY_Size MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	NSSize size = ctx->nsw.screen.frame.size;
	CGFloat scale = mty_screen_scale(ctx->nsw.screen);

	return (MTY_Size) {
		.w = lrint(size.width * scale),
		.h = lrint(size.height * scale),
	};
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return 1.0f;

	// macOS scales the display as though it switches resolutions,
	// so all we need to report is the high DPI device multiplier
	return mty_screen_scale(ctx->nsw.screen);
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx->nsw.title = [NSString stringWithUTF8String:title];
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx->nsw.occlusionState & NSWindowOcclusionStateVisible;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx->nsw.isKeyWindow;
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (active) {
		[ctx->nsw makeKeyAndOrderFront:ctx->nsw];

	} else {
		[ctx->nsw orderOut:ctx->nsw];
	}
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) != NULL;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return ctx->nsw.styleMask & NSWindowStyleMaskFullScreen;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	bool is_fullscreen = MTY_WindowIsFullscreen(app, window);

	if ((!is_fullscreen && fullscreen) || (is_fullscreen && !fullscreen)) {
		ctx->top = fullscreen;
		[ctx->nsw toggleFullScreen:ctx->nsw];
	}
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	window_warp_cursor(ctx->nsw, x, y);
	MTY_AppSetRelativeMouse(app, false);
}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}

void *MTY_WindowGetNative(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return (__bridge void *) ctx->nsw;
}


// App, Window Private

MTY_EventFunc mty_app_get_event_func(MTY_App *ctx, void **opaque)
{
	*opaque = ctx->opaque;

	return ctx->event_func;
}

MTY_Hash *mty_app_get_hotkey_hash(MTY_App *ctx)
{
	return ctx->hotkey;
}

bool mty_app_dedupe_key(MTY_App *ctx, MTY_Key key, bool pressed, bool repeat)
{
	bool was_down = ctx->keys[key];
	bool should_fire = (pressed && (repeat || !was_down)) || (!pressed && was_down);

	ctx->keys[key] = pressed;

	return should_fire;
}

struct window_common *mty_window_get_common(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return &ctx->cmn;
}


// Misc

static MTY_Atomic32 APP_GLOCK;
static char APP_KEYS[MTY_KEY_MAX][16];

MTY_Frame MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight)
{
	NSScreen *screen = screen_get_primary();

	NSSize size = screen.frame.size;

	return mty_window_adjust(size.width, size.height, 1.0f, maxHeight, x, -y, w, h);
}

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
