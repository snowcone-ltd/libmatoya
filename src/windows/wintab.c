#define _USE_MATH_DEFINES
#include <math.h>

#include "matoya.h"
#include "wintab.h"

typedef UINT(API *WTINFOA)(UINT, UINT, LPVOID);
typedef HCTX(API *WTOPENA)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL(API *WTCLOSE)(HCTX);
typedef BOOL(API *WTPACKET)(HCTX, UINT, LPVOID);
typedef BOOL(API *WTEXTGET)(HCTX, UINT, LPVOID);
typedef BOOL(API *WTEXTSET)(HCTX, UINT, LPVOID);
typedef BOOL(API *WTOVERLAP)(HCTX, BOOL);

static struct wt {
	MTY_SO *instance;

	WTINFOA  info;
	WTOPENA  open;
	WTCLOSE  close;
	WTPACKET packet;
	WTEXTGET ext_get;
	WTEXTSET ext_set;
	WTOVERLAP overlap;
} wt;

struct wintab
{
	HCTX context;
	MTY_PenEvent prev_evt;
	PACKETEXT prev_pktext;
	DWORD prev_buttons;

	int32_t max_pressure;
	bool override;
	bool pen_in_range;
	bool has_double_clicked;
};

static bool wintab_find_extension(struct wintab *ctx, uint32_t searched_tag, uint32_t *index)
{
	uint32_t current = 0;

	for (uint32_t i = 0; wt.info(WTI_EXTENSIONS + i, EXT_TAG, &current); i++) {
		if (current != searched_tag)
			continue;

		*index = i;
		return true;
	}

	return false;
}

static void wintab_override_extension(struct wintab *ctx, uint8_t tablet_i, uint32_t extension)
{
	// Retrieve the number of controls for the given extension
	EXTPROPERTY props = {0};
	props.propertyID = TABLET_PROPERTY_CONTROLCOUNT;
	props.tabletIndex = tablet_i;

	if (!wt.ext_get(ctx->context, extension, &props))
		return;

	uint8_t controls = props.data[0];

	for (uint8_t control_i = 0; control_i < controls; control_i++) {
		// Retrieve the number of functions for the current control
		props.propertyID = TABLET_PROPERTY_FUNCCOUNT;
		props.controlIndex = control_i;

		if (!wt.ext_get(ctx->context, extension, &props))
			continue;

		uint8_t functions = props.data[0];

		for (uint8_t function_i = 0; function_i < functions; function_i++) {
			// Mark the current function as overriden by the application
			props.propertyID = TABLET_PROPERTY_OVERRIDE;
			props.functionIndex = function_i;
			props.dataSize = sizeof(bool);
			props.data[0] = ctx->override;

			wt.ext_set(ctx->context, extension, &props);
		}
	}
}

struct wintab *wintab_create(HWND hwnd, bool override)
{
	bool r = false;

	struct wintab *ctx = MTY_Alloc(1, sizeof(struct wintab));
	if (!ctx)
		goto except;

	ctx->override = override;

	// Symbols are preserved between runtime destructions, so we only initialize them once
	if (!wt.instance) {
		wt.instance = MTY_SOLoad("Wintab32.dll");
		if (!wt.instance)
			goto except;

		#define MAP(type, sym, func) \
			wt.##func = (type) MTY_SOGetSymbol(wt.instance, "WT" #sym); \
			if (!wt.##func) goto except;

		MAP(WTINFOA,  InfoA , info);
		MAP(WTOPENA,  OpenA , open);
		MAP(WTCLOSE,  Close , close);
		MAP(WTPACKET, Packet, packet);
		MAP(WTEXTGET, ExtGet, ext_get);
		MAP(WTEXTSET, ExtSet, ext_set);
		MAP(WTOVERLAP, Overlap, overlap);

		// We check that the driver running is currently running
		if (!wt.info(0, 0, NULL))
			goto except;
	}

	// Retrieve a default system context from Wintab
	LOGCONTEXTA lc = {0};
	if (!wt.info(WTI_DEFSYSCTX, 0, &lc))
		goto except;

	uint32_t touch_strip_i = 0,    touch_ring_i = 0,    exp_keys_i = 0;
	uint32_t touch_strip_mask = 0, touch_ring_mask = 0, exp_keys_mask = 0;

	if (wintab_find_extension(ctx, WTX_TOUCHSTRIP, &touch_strip_i))
		wt.info(WTI_EXTENSIONS + touch_strip_i, EXT_MASK, &touch_strip_mask);

	if (wintab_find_extension(ctx, WTX_TOUCHRING, &touch_ring_i))
		wt.info(WTI_EXTENSIONS + touch_ring_i, EXT_MASK, &touch_ring_mask);

	if (wintab_find_extension(ctx, WTX_EXPKEYS2, &exp_keys_i))
		wt.info(WTI_EXTENSIONS + exp_keys_i, EXT_MASK, &exp_keys_mask);

	lc.lcOptions = CXO_MESSAGES | CXO_SYSTEM;
	lc.lcPktData = PACKETDATA | touch_strip_mask | touch_ring_mask | exp_keys_mask;
	lc.lcOutExtY = -lc.lcOutExtY;

	ctx->context = wt.open(hwnd, &lc, TRUE);
	if (!ctx->context)
		goto except;

	// Wintab only supports up to 16 devices working at the same time
	for (uint8_t i = 0; i < 16; i++) {
		if (!wt.info(WTI_DDCTXS + i, 0, NULL))
			break;

		wintab_override_extension(ctx, i, WTX_TOUCHSTRIP);
		wintab_override_extension(ctx, i, WTX_TOUCHRING);
		wintab_override_extension(ctx, i, WTX_EXPKEYS2);
	}

	AXIS ncaps = {0};
	wt.info(WTI_DEVICES, DVC_NPRESSURE, &ncaps);
	ctx->max_pressure = ncaps.axMax;

	r = true;

	except:

	if (!r) {
		MTY_Log("Wintab is not available, fallback to WinInk");
		wintab_destroy(&ctx, true);
	}

	return ctx;
}

void wintab_recreate(struct wintab **ctx, HWND hwnd, bool override)
{
	wintab_destroy(ctx, false);
	*ctx = wintab_create(hwnd, override);
}

void wintab_destroy(struct wintab **wintab, bool unload_symbols)
{
	if (!wintab || !*wintab)
		return;

	struct wintab *ctx = *wintab;

	if (ctx->context) {
		wt.close(ctx->context);
		ctx->context = NULL;
	}

	if (unload_symbols && wt.instance) {
		MTY_SOUnload(&wt.instance);
		wt = (struct wt) {0};
	}

	MTY_Free(ctx);
	*wintab = NULL;
}

void wintab_overlap_context(struct wintab *ctx, bool overlap)
{
	wt.overlap(ctx->context, overlap);
}

void wintab_get_packet(struct wintab *ctx, WPARAM wparam, LPARAM lparam, void *pkt)
{
	wt.packet((HCTX) lparam, (UINT) wparam, pkt);
}

void wintab_on_packet(struct wintab *ctx, MTY_Event *evt, const PACKET *pkt, MTY_Window window)
{
	bool double_clicked = false;
	bool has_double_clicked = ctx->has_double_clicked;

	evt->type = MTY_EVENT_PEN;
	evt->window = window;

	evt->pen.x = (uint16_t) MTY_MAX(pkt->pkX, 0);
	evt->pen.y = (uint16_t) MTY_MAX(pkt->pkY, 0);
	evt->pen.z = (uint16_t) MTY_MAX(pkt->pkZ, 0);

	evt->pen.pressure = (uint16_t) ((double) pkt->pkNormalPressure / (double) ctx->max_pressure * 1024.0);
	evt->pen.rotation = (uint16_t) pkt->pkOrientation.orTwist;

	#define sind(x) sin(fmod(x, 360) * M_PI / 180)
	#define cosd(x) cos(fmod(x, 360) * M_PI / 180)
	double azimuth  = 90.0 + pkt->pkOrientation.orAzimuth  / 10.0;
	double altitude = 90.0 - pkt->pkOrientation.orAltitude / 10.0;
	evt->pen.tiltX = (int8_t) (-sind(altitude) * cosd(azimuth) * 90.0);
	evt->pen.tiltY = (int8_t) (-sind(altitude) * sind(azimuth) * 90.0);

	if (pkt->pkStatus & TPS_INVERT)
		evt->pen.flags |= MTY_PEN_FLAG_INVERTED;

	BYTE buttons = 0;
	wt.info(WTI_CURSORS + pkt->pkCursor, CSR_BUTTONS, &buttons);

	BYTE mapping[32] = {0};
	wt.info(WTI_CURSORS + pkt->pkCursor, CSR_SYSBTNMAP, &mapping);

	for (uint8_t i = 0; i < buttons; i++) {
		bool pressed      = pkt->pkButtons & (1 << i);
		bool prev_pressed = ctx->prev_buttons & (1 << i);

		bool left_click   = mapping[i] == SBN_LCLICK    || mapping[i] == SBN_LDBLCLICK;
		bool right_click  = mapping[i] == SBN_RCLICK    || mapping[i] == SBN_RDBLCLICK;
		bool double_click = mapping[i] == SBN_LDBLCLICK || mapping[i] == SBN_RDBLCLICK;

		if ((left_click || right_click) && pressed)
			evt->pen.flags |= MTY_PEN_FLAG_TOUCHING;

		if (double_click)
			ctx->has_double_clicked = double_clicked = pressed;

		if (pressed || prev_pressed)
			evt->pen.flags |= 
				i == 0 ? MTY_PEN_FLAG_TIP :
				i == 1 ? MTY_PEN_FLAG_BARREL_1 :
				i == 2 ? MTY_PEN_FLAG_BARREL_2 :
				0;
	}

	if (evt->pen.flags & MTY_PEN_FLAG_TOUCHING && evt->pen.flags & MTY_PEN_FLAG_INVERTED)
		evt->pen.flags |= MTY_PEN_FLAG_ERASER;

	if (double_clicked && !has_double_clicked)
		evt->pen.flags |= MTY_PEN_FLAG_DOUBLE_CLICK;

	ctx->prev_evt     = evt->pen;
	ctx->prev_buttons = pkt->pkButtons;
}

static uint16_t wintab_transform_position(DWORD position)
{
	// The position value isn't documented, so here is what we discovered:
	// * Default logical range is 0-72 and application-specific settings range is 182-255
	// * In case of custom settings, we convert the position back to the 0-72 range to guarantee consistent event values
	// * If the position is not within one of those known ranges, we prefer returning 0 to avoid undesired behaviors

	if (position > UINT8_MAX) {
		return 0;

	} else if (position >= 73) {
		return (uint16_t) ((int16_t) position - UINT8_MAX + 73);
	
	} else {
		return (uint16_t) position;
	}
}

void wintab_on_packetext(struct wintab *ctx, MTY_Event *evt, const PACKETEXT *pktext)
{
	evt->type = MTY_EVENT_WINTAB;

	evt->wintab.device = pktext->pkExpKeys.nTablet;

	evt->wintab.type    = MTY_WINTAB_TYPE_KEY;
	evt->wintab.control = pktext->pkExpKeys.nControl;
	evt->wintab.state   = (uint8_t) pktext->pkExpKeys.nState;

	const SLIDERDATA *curr_ring = &pktext->pkTouchRing;
	const SLIDERDATA *prev_ring = &ctx->prev_pktext.pkTouchRing;
	if (curr_ring->nPosition != prev_ring->nPosition || curr_ring->nMode != prev_ring->nMode) {
		evt->wintab.type     = MTY_WINTAB_TYPE_RING;
		evt->wintab.control  = curr_ring->nControl;
		evt->wintab.state    = curr_ring->nMode;
		evt->wintab.position = wintab_transform_position(curr_ring->nPosition);
	}

	const SLIDERDATA *curr_strip = &pktext->pkTouchStrip;
	const SLIDERDATA *prev_strip = &ctx->prev_pktext.pkTouchStrip;
	if (curr_strip->nPosition != prev_strip->nPosition || curr_strip->nMode != prev_strip->nMode) {
		evt->wintab.type     = MTY_WINTAB_TYPE_STRIP;
		evt->wintab.control  = curr_strip->nControl;
		evt->wintab.state    = curr_strip->nMode;
		evt->wintab.position = wintab_transform_position(curr_strip->nPosition);
	}

	ctx->prev_pktext = *pktext;
}

bool wintab_on_proximity(struct wintab *ctx, MTY_Event *evt, LPARAM lparam)
{
	bool pen_in_range = LOWORD(lparam) != 0;

	if (!pen_in_range && ctx->pen_in_range) {
		evt->type = MTY_EVENT_PEN;
		evt->pen = ctx->prev_evt;
		evt->pen.flags |= MTY_PEN_FLAG_LEAVE;
	}

	ctx->pen_in_range = pen_in_range;

	return ctx->pen_in_range;
}

void wintab_on_infochange(struct wintab **ctx, HWND hwnd)
{
	wintab_recreate(ctx, hwnd, ctx && *ctx && (*ctx)->override);
}
