#pragma once

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
void wintab_recreate(struct wintab **ctx);
void wintab_destroy(struct wintab **ctx, bool unload);

void wintab_override_controls(struct wintab *ctx, bool override);

void wintab_get_packet(struct wintab *ctx, WPARAM wparam, LPARAM lparam, PACKET *pkt);
void wintab_get_packetext(struct wintab *ctx, WPARAM wparam, LPARAM lparam, PACKETEXT *pktext);

void wintab_on_packet(struct wintab *ctx, MTY_Event *evt, PACKET *pkt, MTY_Window window);
void wintab_on_packetext(struct wintab *ctx, MTY_Event *evt, PACKETEXT *pktext);
bool wintab_on_proximity(struct wintab *ctx, MTY_Event *evt, LPARAM lparam);
