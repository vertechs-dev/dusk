#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define DUSK_MOD_EXPORT __declspec(dllexport)
#else
#define DUSK_MOD_EXPORT __attribute__((visibility("default")))
#endif

#define DUSK_MOD_API_VERSION 1

typedef void* DuskPanelHandle;
typedef void* DuskElemHandle;

typedef enum {
    DUSK_UPG_AVAILABLE   = 0,
    DUSK_UPG_OWNED       = 1,
    DUSK_UPG_LOCKED      = 2,
    DUSK_UPG_CANT_AFFORD = 3,
} DuskUpgradeState;

typedef struct {
    const char* name;          /* raw string -> name box                     */
    const char* description;   /* raw string -> description window           */
    uint16_t    cost;          /* shown near the name box                    */
    uint8_t     state;         /* DuskUpgradeState                           */
    uint8_t     icon_kind;     /* 0 = vanilla archive index, 1 = custom .bti */
    uint16_t    icon_index;    /* vanilla: index into the item-icon archive  */
    const void* icon_bti;      /* custom: pointer to .bti (ResTIMG) bytes    */
    uint32_t    icon_bti_len;  /* custom: byte length                        */
} DuskUpgradeNode;

typedef struct {
    const char*            title;
    const DuskUpgradeNode*  nodes;
    uint32_t                node_count;
} DuskUpgradeCategory;

typedef struct {
    const DuskUpgradeCategory* categories;
    uint32_t                   category_count;
    uint32_t                   current_category;
    int32_t                    currency;
    const void* checkmark_bti;       /* shared "owned" overlay .bti bytes, or NULL */
    uint32_t    checkmark_bti_len;   /* byte length (0 = no overlay)               */
    const void* dot_highlight_bti;   /* pagination dot for the current page, or NULL */
    uint32_t    dot_highlight_bti_len;
    const void* dot_neutral_bti;     /* pagination dot for other pages, or NULL    */
    uint32_t    dot_neutral_bti_len;
} DuskUpgradeRingModel;

typedef struct {
    void (*on_select)(uint32_t category, uint32_t node);
    void (*on_purchase)(uint32_t category, uint32_t node);
    void (*on_category_change)(int32_t delta);
    void (*on_close)(void);
} DuskUpgradeRingCallbacks;

// Place this once at file scope in your mod to declare the minimum API version required.
// The loader will refuse to initialize the mod if the engine's API version is older.
#define DUSK_REQUIRE_API_VERSION                                                                   \
    extern "C" DUSK_MOD_EXPORT uint32_t mod_api_version = DUSK_MOD_API_VERSION;

struct DuskModAPIv1 {
    uint32_t api_version;
    const char* mod_dir;

    void (*log_info)(const char* fmt, ...);
    void (*log_warn)(const char* fmt, ...);
    void (*log_error)(const char* fmt, ...);

    void* (*load_resource)(const char* relative_path, size_t* out_size);
    void (*free_resource)(void* data);

    void (*register_tab_content)(
        void (*build_fn)(DuskPanelHandle panel, void* userdata), void* userdata);
    void (*register_tab_update)(void (*update_fn)(void* userdata), void* userdata);

    void (*panel_add_section)(DuskPanelHandle panel, const char* text);
    void (*panel_add_button)(
        DuskPanelHandle panel, const char* label, void (*cb)(void* userdata), void* userdata);
    DuskElemHandle (*panel_add_badge_row)(DuskPanelHandle panel, const char* label, int ok);
    DuskElemHandle (*panel_add_dyn_text)(DuskPanelHandle panel, const char* text);
    DuskElemHandle (*panel_add_progress)(DuskPanelHandle panel, float value);

    void (*elem_set_badge)(DuskElemHandle elem, int ok);
    void (*elem_set_text)(DuskElemHandle elem, const char* text);
    void (*elem_set_progress)(DuskElemHandle elem, float value);

    void (*hook_install)(void* fn_addr, void* tramp_fn, void** orig_store);
    void (*hook_pre)(void* fn_addr, int32_t (*fn)(void* args));
    void (*hook_post)(void* fn_addr, void (*fn)(void* args, void* retval));
    void (*hook_replace)(void* fn_addr, void (*fn)(void* args, void* retval));

    bool (*hook_dispatch_pre)(void* fn_addr, void* args, void* retval);
    void (*hook_dispatch_post)(void* fn_addr, void* args, void* retval);

    void (*service_publish)(const char* name, void* ptr);
    void* (*service_get)(const char* name);

    bool (*upgrade_ring_open)(const DuskUpgradeRingModel*, const DuskUpgradeRingCallbacks*);
    void (*upgrade_ring_update)(const DuskUpgradeRingModel*);
    void (*upgrade_ring_close)(void);
    bool (*upgrade_ring_is_open)(void);
};

using DuskModAPI = DuskModAPIv1;

extern "C" {
void mod_init(DuskModAPI* api);
void mod_tick(DuskModAPI* api);
}
