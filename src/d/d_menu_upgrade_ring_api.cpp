// Thin signal layer between the mod and dMw_c's menu state machine.
// dMw_c owns + drives the actual dMenu_UpgradeRing_c through its RING_* states;
// this file only relays open/update/close requests and the open flag.
#include "dusk/mod_api.h"

namespace {
    bool                     g_open      = false;  // mod-visible: true from Open() until NotifyClosed()
    bool                     g_openReq   = false;  // pending open for dMw_c to consume
    bool                     g_closeReq  = false;  // pending close
    bool                     g_updateReq = false;  // pending model re-skin
    DuskUpgradeRingModel     g_model{};            // copied; the mod owns the category/node arrays
    DuskUpgradeRingCallbacks g_cb{};

    // Header layout, order: titleSize, marginX, marginY, dotSize, dotStep,
    // rowGap, labelSize, labelGap. Defaults match drawPageHeader's originals.
    float g_hdr[8] = {24.0f, 48.0f, 64.0f, 14.0f, 18.0f, 46.0f, 18.0f, 6.0f};
    // Cost-text colour (RGB 0..1); default white. drawCost reads via the getter.
    float g_costColor[3] = {1.0f, 1.0f, 1.0f};
}

// ---- public API (wired into DuskModAPIv1; called by the mod) ----
extern "C" bool DuskUpgradeRing_Open(const DuskUpgradeRingModel* m, const DuskUpgradeRingCallbacks* cb) {
    if (g_open || g_openReq || !m || !cb) return false;
    g_model = *m; g_cb = *cb;
    g_openReq = true; g_open = true;
    return true;
}
extern "C" void DuskUpgradeRing_Update(const DuskUpgradeRingModel* m) {
    if (!g_open || !m) return;
    g_model = *m; g_updateReq = true;
}
extern "C" void DuskUpgradeRing_Close(void) { if (g_open) g_closeReq = true; }
extern "C" bool DuskUpgradeRing_IsOpen(void) { return g_open; }

// ---- engine-side (called by dMw_c) ----
extern "C" bool DuskUpgradeRing_PollOpen(const DuskUpgradeRingModel** m, const DuskUpgradeRingCallbacks** cb) {
    if (!g_openReq) return false;
    g_openReq = false; *m = &g_model; *cb = &g_cb; return true;
}
extern "C" bool DuskUpgradeRing_PollUpdate(const DuskUpgradeRingModel** m) {
    if (!g_updateReq) return false;
    g_updateReq = false; *m = &g_model; return true;
}
extern "C" bool DuskUpgradeRing_PollClose(void) {
    if (!g_closeReq) return false;
    g_closeReq = false; return true;
}
extern "C" void DuskUpgradeRing_NotifyClosed(void) { g_open = false; g_closeReq = false; g_updateReq = false; }

// ---- header layout (mod sets, drawPageHeader reads) ----
extern "C" void DuskUpgradeRing_SetHeaderLayout(float titleSize, float marginX, float marginY,
                                                float dotSize, float dotStep, float rowGap,
                                                float labelSize, float labelGap) {
    g_hdr[0]=titleSize; g_hdr[1]=marginX; g_hdr[2]=marginY; g_hdr[3]=dotSize;
    g_hdr[4]=dotStep;   g_hdr[5]=rowGap;  g_hdr[6]=labelSize; g_hdr[7]=labelGap;
}
extern "C" const float* DuskUpgradeRing_GetHeaderLayout(void) { return g_hdr; }

// ---- cost-text colour (mod sets, drawCost reads) ----
extern "C" void DuskUpgradeRing_SetCostColor(float r, float g, float b) {
    g_costColor[0] = r; g_costColor[1] = g; g_costColor[2] = b;
}
extern "C" const float* DuskUpgradeRing_GetCostColor(void) { return g_costColor; }
