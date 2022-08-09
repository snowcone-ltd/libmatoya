// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "glcorearb.h"


// X interface

// Reference: https://code.woboq.org/qt5/include/X11/

#define _Xconst                   const

#define Success                   0

#define None                      0L
#define NoSymbol                  0L
#define AnyPropertyType           0L
#define Bool                      int
#define True                      1
#define False                     0
#define Status                    int
#define AllocNone                 0

#define RevertToNone              ((int) None)

#define InputOutput               1
#define InputOnly                 2

#define NoEventMask               0L
#define KeyPressMask              (1L<<0)
#define KeyReleaseMask            (1L<<1)
#define ButtonPressMask           (1L<<2)
#define ButtonReleaseMask         (1L<<3)
#define EnterWindowMask           (1L<<4)
#define LeaveWindowMask           (1L<<5)
#define PointerMotionMask         (1L<<6)
#define PointerMotionHintMask     (1L<<7)
#define Button1MotionMask         (1L<<8)
#define Button2MotionMask         (1L<<9)
#define Button3MotionMask         (1L<<10)
#define Button4MotionMask         (1L<<11)
#define Button5MotionMask         (1L<<12)
#define ButtonMotionMask          (1L<<13)
#define KeymapStateMask           (1L<<14)
#define ExposureMask              (1L<<15)
#define VisibilityChangeMask      (1L<<16)
#define StructureNotifyMask       (1L<<17)
#define ResizeRedirectMask        (1L<<18)
#define SubstructureNotifyMask    (1L<<19)
#define SubstructureRedirectMask  (1L<<20)
#define FocusChangeMask           (1L<<21)
#define PropertyChangeMask        (1L<<22)
#define ColormapChangeMask        (1L<<23)
#define OwnerGrabButtonMask       (1L<<24)

#define PropModeReplace           0
#define PropModePrepend           1
#define PropModeAppend            2

#define ShiftMask                 (1<<0)
#define LockMask                  (1<<1)
#define ControlMask               (1<<2)
#define Mod1Mask                  (1<<3)
#define Mod2Mask                  (1<<4)
#define Mod3Mask                  (1<<5)
#define Mod4Mask                  (1<<6)
#define Mod5Mask                  (1<<7)

#define KeyPress                  2
#define KeyRelease                3
#define ButtonPress               4
#define ButtonRelease             5
#define MotionNotify              6
#define EnterNotify               7
#define LeaveNotify               8
#define FocusIn                   9
#define FocusOut                  10
#define KeymapNotify              11
#define Expose                    12
#define GraphicsExpose            13
#define NoExpose                  14
#define VisibilityNotify          15
#define CreateNotify              16
#define DestroyNotify             17
#define UnmapNotify               18
#define MapNotify                 19
#define MapRequest                20
#define ReparentNotify            21
#define ConfigureNotify           22
#define ConfigureRequest          23
#define GravityNotify             24
#define ResizeRequest             25
#define CirculateNotify           26
#define CirculateRequest          27
#define PropertyNotify            28
#define SelectionClear            29
#define SelectionRequest          30
#define SelectionNotify           31
#define ColormapNotify            32
#define ClientMessage             33
#define MappingNotify             34
#define GenericEvent              35
#define LASTEvent                 36

#define CWBackPixmap              (1L<<0)
#define CWBackPixel               (1L<<1)
#define CWBorderPixmap            (1L<<2)
#define CWBorderPixel             (1L<<3)
#define CWBitGravity              (1L<<4)
#define CWWinGravity              (1L<<5)
#define CWBackingStore            (1L<<6)
#define CWBackingPlanes           (1L<<7)
#define CWBackingPixel            (1L<<8)
#define CWOverrideRedirect        (1L<<9)
#define CWSaveUnder               (1L<<10)
#define CWEventMask               (1L<<11)
#define CWDontPropagate           (1L<<12)
#define CWColormap                (1L<<13)
#define CWCursor                  (1L<<14)

#define IsUnmapped                0
#define IsUnviewable              1
#define IsViewable                2

#define QueuedAlready             0
#define QueuedAfterReading        1
#define QueuedAfterFlush          2

#define XNInputStyle              "inputStyle"
#define XNClientWindow            "clientWindow"

#define XIMPreeditNothing         0x0008L
#define XIMStatusNothing          0x0400L

#define CurrentTime               0L

#define GrabModeSync              0
#define GrabModeAsync             1

#define NotifyNormal              0
#define NotifyGrab                1
#define NotifyUngrab              2
#define NotifyWhileGrabbed        3

#define XA_PRIMARY                ((Atom) 1)
#define XA_ATOM                   ((Atom) 4)
#define XA_CARDINAL               ((Atom) 6)
#define XA_CUT_BUFFER0            ((Atom) 9)
#define XA_STRING                 ((Atom) 31)

#define USPosition                (1L << 0)
#define USSize                    (1L << 1)
#define PPosition                 (1L << 2)
#define PSize                     (1L << 3)
#define PMinSize                  (1L << 4)
#define PMaxSize                  (1L << 5)
#define PResizeInc                (1L << 6)
#define PAspect                   (1L << 7)
#define PBaseSize                 (1L << 8)
#define PWinGravity               (1L << 9)

#define InputHint                 (1L << 0)
#define WindowGroupHint           (1L << 6)

#define _NET_WM_STATE_REMOVE      0
#define _NET_WM_STATE_ADD         1
#define _NET_WM_STATE_TOGGLE      2

typedef unsigned long VisualID;
typedef unsigned long XID;
typedef unsigned long Time;
typedef unsigned long Atom;
typedef char *XPointer;
typedef XID Window;
typedef XID Drawable;
typedef XID Colormap;
typedef XID Pixmap;
typedef XID Cursor;
typedef XID KeySym;
typedef struct _XDisplay Display;
typedef struct _XVisual Visual;
typedef struct _XScreen Screen;
typedef unsigned char KeyCode;

typedef struct {
	unsigned long pixel;
	unsigned short red, green, blue;
	char flags;
	char pad;
} XColor;

typedef struct {
	Visual *visual;
	VisualID visualid;
	int screen;
	unsigned int depth;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

typedef struct {
	Pixmap background_pixmap;
	unsigned long background_pixel;
	Pixmap border_pixmap;
	unsigned long border_pixel;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixel;
	Bool save_under;
	long event_mask;
	long do_not_propagate_mask;
	Bool override_redirect;
	Colormap colormap;
	Cursor cursor;
} XSetWindowAttributes;

typedef struct {
	int x;
	int y;
	int width;
	int height;
	int border_width;
	int depth;
	Visual *visual;
	Window root;
	int class;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixel;
	Bool save_under;
	Colormap colormap;
	Bool map_installed;
	int map_state;
	long all_event_masks;
	long your_event_mask;
	long do_not_propagate_mask;
	Bool override_redirect;
	Screen *screen;
} XWindowAttributes;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	int extension;
	int evtype;
	unsigned int cookie;
	void *data;
} XGenericEventCookie;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x;
	int y;
	int x_root;
	int y_root;
	unsigned int state;
	unsigned int keycode;
	Bool same_screen;
} XKeyEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Atom message_type;
	int format;
	union {
		char b[20];
		short s[10];
		long l[5];
	} data;
} XClientMessageEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window owner;
	Window requestor;
	Atom selection;
	Atom target;
	Atom property;
	Time time;
} XSelectionRequestEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window requestor;
	Atom selection;
	Atom target;
	Atom property;
	Time time;
} XSelectionEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x;
	int y;
	int x_root;
	int y_root;
	unsigned int state;
	unsigned int button;
	Bool same_screen;
} XButtonEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x;
	int y;
	int x_root;
	int y_root;
	unsigned int state;
	char is_hint;
	Bool same_screen;
} XMotionEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	int mode;
	int detail;
} XFocusChangeEvent;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window event;
	Window window;
	int x, y;
	int width, height;
	int border_width;
	Window above;
	Bool override_redirect;
} XConfigureEvent;

typedef union _XEvent {
	int type;
	XKeyEvent xkey;
	XButtonEvent xbutton;
	XMotionEvent xmotion;
	XFocusChangeEvent xfocus;
	XConfigureEvent xconfigure;
	XSelectionRequestEvent xselectionrequest;
	XSelectionEvent xselection;
	XClientMessageEvent xclient;
	XGenericEventCookie xcookie;
	long pad[24];
} XEvent;

typedef struct Hints {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
} Hints;

typedef struct {
	unsigned char *value;
	Atom encoding;
	int format;
	unsigned long nitems;
} XTextProperty;

typedef struct {
	long flags;
	int x, y;
	int width, height;
	int min_width, min_height;
	int max_width, max_height;
	int width_inc, height_inc;
	struct {
		int x;
		int y;
	} min_aspect, max_aspect;
	int base_width, base_height;
	int win_gravity;
} XSizeHints;

typedef struct {
	long flags;
	Bool input;
	int initial_state;
	Pixmap icon_pixmap;
	Window icon_window;
	int icon_x, icon_y;
	Pixmap icon_mask;
	XID window_group;
} XWMHints;

typedef struct {
	char *res_name;
	char *res_class;
} XClassHint;

typedef XKeyEvent XKeyPressedEvent;
typedef XKeyEvent XKeyReleasedEvent;

struct _XrmHashBucketRec;
typedef struct _XIM *XIM;
typedef struct _XIC *XIC;


// XI2 interface

// Reference: https://code.woboq.org/kde/include/X11/extensions/

#define XIAllMasterDevices 1

#define XI_RawMotion       17

#define XISetMask(ptr, event) \
	(((unsigned char *) (ptr))[(event) >> 3] |= (1 << ((event) & 7)))

#define XIMaskIsSet(ptr, event) \
	(((unsigned char *) (ptr))[(event) >> 3] & (1 << ((event) & 7)))

typedef struct {
	int deviceid;
	int mask_len;
	unsigned char *mask;
} XIEventMask;

typedef struct {
	int mask_len;
	unsigned char *mask;
	double *values;
} XIValuatorState;

typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	int extension;
	int evtype;
	Time time;
	int deviceid;
	int sourceid;
	int detail;
	int flags;
	XIValuatorState valuators;
	double *raw_values;
} XIRawEvent;


// Xcursor interface

typedef int XcursorBool;
typedef unsigned int XcursorUInt;
typedef XcursorUInt XcursorDim;
typedef XcursorUInt XcursorPixel;

typedef struct _XcursorImage {
	XcursorUInt version;
	XcursorDim size;
	XcursorDim width;
	XcursorDim height;
	XcursorDim xhot;
	XcursorDim yhot;
	XcursorUInt delay;
	XcursorPixel *pixels;
} XcursorImage;


// Xrandr interface

typedef struct _XRRScreenConfiguration XRRScreenConfiguration;


// GLX interface

// Reference: https://code.woboq.org/qt5/include/GL/

enum _GLX_CONFIGS {
	GLX_USE_GL           = 1,
	GLX_BUFFER_SIZE      = 2,
	GLX_LEVEL            = 3,
	GLX_RGBA             = 4,
	GLX_DOUBLEBUFFER     = 5,
	GLX_STEREO           = 6,
	GLX_AUX_BUFFERS      = 7,
	GLX_RED_SIZE         = 8,
	GLX_GREEN_SIZE       = 9,
	GLX_BLUE_SIZE        = 10,
	GLX_ALPHA_SIZE       = 11,
	GLX_DEPTH_SIZE       = 12,
	GLX_STENCIL_SIZE     = 13,
	GLX_ACCUM_RED_SIZE   = 14,
	GLX_ACCUM_GREEN_SIZE = 15,
	GLX_ACCUM_BLUE_SIZE  = 16,
	GLX_ACCUM_ALPHA_SIZE = 17,
};

typedef XID GLXDrawable;
typedef struct GLXContext * GLXContext;


// Helper window struct

struct xinfo {
	Display *display;
	XVisualInfo *vis;
	Window window;
};
