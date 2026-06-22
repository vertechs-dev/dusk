// Stubs for the upgrade-ring mod API. Real behavior arrives in Phase C/D.
#include "dusk/mod_api.h"

namespace {
    bool g_open = false;
}

extern "C" bool DuskUpgradeRing_Open(const DuskUpgradeRingModel*, const DuskUpgradeRingCallbacks*) {
    return false; // not yet implemented -> mod falls back to the ImGui menu
}
extern "C" void DuskUpgradeRing_Update(const DuskUpgradeRingModel*) {}
extern "C" void DuskUpgradeRing_Close(void) { g_open = false; }
extern "C" bool DuskUpgradeRing_IsOpen(void) { return g_open; }
