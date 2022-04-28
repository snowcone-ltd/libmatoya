#pragma once

#include <windows.h>
#include <stdbool.h>

// Those defines are required to configure wintab/*.h headers
#define PACKETDATA       (PK_CONTEXT | PK_STATUS | PK_TIME | PK_CHANGED | PK_SERIAL_NUMBER | PK_CURSOR | PK_BUTTONS |\
                          PK_X | PK_Y | PK_Z | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ORIENTATION | PK_ROTATION)
#define PACKETEXPKEYS    PKEXT_ABSOLUTE
#define PACKETTOUCHSTRIP PKEXT_ABSOLUTE
#define PACKETTOUCHRING  PKEXT_ABSOLUTE

#include "wintab/MSGPACK.H"
#include "wintab/WINTAB.H"
#include "wintab/PKTDEF.H"

struct wintab;

struct wintab *wintab_create(HWND hwnd);
void wintab_recreate(struct wintab **ctx, HWND hwnd);
void wintab_destroy(struct wintab **wintab, bool unload_symbols);

void wintab_override_controls(struct wintab *ctx, bool override);

void wintab_get_packet(struct wintab *ctx, WPARAM wparam, LPARAM lparam, void *pkt);

void wintab_on_packet(struct wintab *ctx, MTY_Event *evt, const PACKET *pkt, MTY_Window window);
void wintab_on_packetext(struct wintab *ctx, MTY_Event *evt, const PACKETEXT *pktext);
bool wintab_on_proximity(struct wintab *ctx, MTY_Event *evt, LPARAM lparam);
