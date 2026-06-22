// Owns + drives the mod-facing upgrade ring (Task C7).
//
// The widget (dMenu_UpgradeRing_c) is opened by the MOD through the API,
// OUTSIDE dMw_c's own item-ring state machine. So the fork must own the
// instance and drive its _move()/_draw() itself from a reliable per-frame
// point in the same 2D pass the vanilla item ring uses.
//
// Allocation + per-frame driving both live inside dMw_c (which holds the
// mpHeap / mpStick / mpCStick that dMw_ring_create uses). The public
// extern "C" entry points below only stash the model + callbacks and flip a
// "want open" flag; dMw_c calls DuskUpgradeRing_Drive()/DuskUpgradeRing_DoDraw()
// each frame, constructing the widget on its heap on the first frame after Open.
#include "dusk/mod_api.h"
#include "d/d_menu_upgrade_ring.h"
#include "JSystem/JKernel/JKRHeap.h"

struct CSTControl;
class STControl;

namespace {
    // Lifecycle phases mirror the vanilla RING_OPEN -> RING_MOVE -> RING_CLOSE
    // flow, but driven by us instead of dMw_c's menu proc table.
    enum Phase {
        PHASE_OPEN,   // open animation playing (isOpen() until true)
        PHASE_MOVE,   // interactive; _move() + isMoveEnd()
        PHASE_CLOSE,  // close animation playing (isClose() until done)
    };

    dMenu_UpgradeRing_c*     g_ring = nullptr;
    DuskUpgradeRingCallbacks g_cb{};
    DuskUpgradeRingModel     g_model{};   // copy so the pointer the widget holds stays valid
    bool                     g_open = false;        // mod-visible "is the ring up?" flag
    bool                     g_wantConstruct = false;  // Open() requested; build on next dMw_c frame
    Phase                    g_phase = PHASE_OPEN;
}

// ---------------------------------------------------------------------------
// Public API (wired into DuskModAPIv1 by mod_loader.cpp). Called from the mod.
// ---------------------------------------------------------------------------
extern "C" bool DuskUpgradeRing_Open(const DuskUpgradeRingModel* m, const DuskUpgradeRingCallbacks* cb) {
    if (g_open || g_wantConstruct || !m || !cb) return false;
    g_cb = *cb;
    g_model = *m;                          // shallow copy; categories/nodes arrays stay owned by the mod
    g_wantConstruct = true;
    g_open = true;                         // report open immediately so the mod routes input here
    g_phase = PHASE_OPEN;
    return true;
}

extern "C" void DuskUpgradeRing_Update(const DuskUpgradeRingModel* m) {
    if (!g_open || !m) return;
    g_model = *m;
    if (g_ring) {
        g_ring->setModel(&g_model);
        // Re-skin AND refresh count/layout for the (possibly new) current
        // category -- node_count may differ between categories.
        g_ring->reskinForCategory();
    }
}

extern "C" void DuskUpgradeRing_Close(void) {
    // Explicit close from the mod. If the widget hasn't even been built yet
    // (Open then Close in the same frame), just drop the pending construct.
    if (!g_open) return;
    if (!g_ring) {
        g_wantConstruct = false;
        g_open = false;
        return;
    }
    // Hand off to the close animation; actual free happens in DuskUpgradeRing_Drive
    // once isClose() completes, on dMw_c's heap-scoped frame.
    if (g_phase != PHASE_CLOSE) {
        g_phase = PHASE_CLOSE;
    }
}

extern "C" bool DuskUpgradeRing_IsOpen(void) { return g_open; }

// ---------------------------------------------------------------------------
// Engine-side driving. Called by dMw_c every frame (Step 2). dMw_c owns the
// heap + stick controls and calls these with the current heap already set to
// mpHeap, so JKR_NEW / heap allocations land where dMw_ring_create's do.
// ---------------------------------------------------------------------------

// Per-frame update. Constructs the widget on the first frame after Open(),
// advances the open/close animation + status machine, and frees the widget
// once the close finishes (or the mod-requested close completes).
extern "C" void DuskUpgradeRing_Drive(JKRExpHeap* heap, STControl* stick, CSTControl* cStick) {
    if (g_wantConstruct) {
        g_wantConstruct = false;
        if (heap && stick) {
            // Construct on dMw_c's 2D heap, exactly like dMw_ring_create:
            // the current heap is already mpHeap when dMw_c calls us, so
            // JKR_NEW allocates here.
            g_ring = JKR_NEW dMenu_UpgradeRing_c(heap, stick, cStick, /*origin*/ 2, &g_model);
            if (g_ring) {
                g_ring->setCallbacks(&g_cb);
                g_ring->_create();
                g_phase = PHASE_OPEN;
            } else {
                g_open = false;  // allocation failed -> mod falls back to ImGui
            }
        } else {
            g_open = false;
        }
    }

    if (!g_ring) return;

    switch (g_phase) {
    case PHASE_OPEN:
        // Mirror ring_open_proc: advance the open animation until settled.
        if (g_ring->isOpen()) {
            g_phase = PHASE_MOVE;
        }
        break;
    case PHASE_MOVE:
        // Mirror ring_move_proc: run the status machine; isMoveEnd() commits
        // to closing (and fires on_close exactly once, guarded inside C6).
        g_ring->_move();
        if (g_ring->isMoveEnd()) {
            g_phase = PHASE_CLOSE;
        }
        break;
    case PHASE_CLOSE:
        // Mirror ring_close_proc: advance the close animation, then free once.
        if (g_ring->isClose()) {
            g_ring->_delete();
            JKR_DELETE(g_ring);
            g_ring = nullptr;
            g_open = false;
        }
        break;
    }
}

// 2D draw pass. dMw_c calls this from its leafdraw _draw() with the graf port
// set up for the menu 2D pass, so the widget renders identically to the item
// ring. The widget draws directly into the graf port (not via set2DOpa), and
// uses a two-pass mDrawFlag cycle -- so call _draw() twice with drawFlag0()
// first, matching the vanilla `drawFlag0(); set2DOpa(); set2DOpa();` pattern.
extern "C" void DuskUpgradeRing_DoDraw(void) {
    if (!g_ring) return;
    g_ring->drawFlag0();
    g_ring->_draw();   // pass 1: mDrawFlag == 0 (background, icons, name, etc.)
    g_ring->_draw();   // pass 2: mDrawFlag == 1 (cursor / selected item)
}
