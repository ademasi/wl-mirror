#ifndef WLM_MIRROR_TARGET_H_
#define WLM_MIRROR_TARGET_H_

#include <wayland-client-protocol.h>
#include <wlm/wayland.h>
#include <wlm/transform.h>

#define WLM_MIRROR_TARGET_PREFIX_OUTPUT "output:"
#define WLM_MIRROR_TARGET_PREFIX_TOPLEVEL "toplevel:"

typedef struct ctx ctx_t;

typedef enum {
    WLM_MIRROR_TARGET_TYPE_NULL = 0,
    WLM_MIRROR_TARGET_TYPE_OUTPUT,
    WLM_MIRROR_TARGET_TYPE_TOPLEVEL,
} wlm_mirror_target_type_t;

typedef struct {
    wlm_mirror_target_type_t type;
    struct ext_image_capture_source_v1 * source;
    enum wl_output_transform transform;
} wlm_mirror_target_t;

typedef struct {
    wlm_mirror_target_t header;
    wlm_wayland_output_entry_t * output;
} wlm_mirror_target_output_t;

typedef struct {
    wlm_mirror_target_t header;
    wlm_wayland_toplevel_entry_t * toplevel;
} wlm_mirror_target_toplevel_t;

wlm_mirror_target_t * wlm_mirror_target_parse(ctx_t * ctx, const char * target);
bool wlm_mirror_target_spec_is_toplevel(const char * spec);
const char * wlm_mirror_target_spec_output_name(const char * spec);
wlm_mirror_target_t * wlm_mirror_target_resolve(ctx_t * ctx, region_t * region);
wlm_mirror_target_t * wlm_mirror_target_find_output(ctx_t * ctx, const char * name);
wlm_mirror_target_t * wlm_mirror_target_find_toplevel(ctx_t * ctx, const char * identifier);

// TODO: remove this
wlm_mirror_target_t * wlm_mirror_target_create_output(ctx_t * ctx, wlm_wayland_output_entry_t * output_node);

wlm_mirror_target_type_t wlm_mirror_target_get_type(wlm_mirror_target_t * target);
wlm_wayland_output_entry_t * wlm_mirror_target_get_output_node(wlm_mirror_target_t * target);
wlm_wayland_toplevel_entry_t * wlm_mirror_target_get_toplevel_node(wlm_mirror_target_t * target);
const char * wlm_mirror_target_get_name(wlm_mirror_target_t * target);
struct ext_image_capture_source_v1 * wlm_mirror_target_get_capture_source(wlm_mirror_target_t * target);
enum wl_output_transform wlm_mirror_target_get_transform(wlm_mirror_target_t * target);

void wlm_mirror_target_destroy(wlm_mirror_target_t * target);

#endif
