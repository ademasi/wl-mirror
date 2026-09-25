#include <wayland-client-protocol.h>
#include <wlm/context.h>
#include <wlm/wayland.h>
#include <wlm/mirror/target.h>
#include <wlm/proto/ext-image-capture-source-v1.h>
#include <stdlib.h>
#include <string.h>

// --- static helper functions

static wlm_mirror_target_t * create_null_target(ctx_t * ctx) {
    wlm_mirror_target_t * target = calloc(1, sizeof *target);
    target->type = WLM_MIRROR_TARGET_TYPE_NULL;
    target->source = NULL;

    (void)ctx;
    return target;
}

static wlm_mirror_target_t * create_output_target(ctx_t * ctx, wlm_wayland_output_entry_t * output_node) {
    wlm_mirror_target_output_t * output_target = calloc(1, sizeof *output_target);
    output_target->header.type = WLM_MIRROR_TARGET_TYPE_OUTPUT;
    output_target->header.source = NULL;
    output_target->header.transform = output_node->transform;
    output_target->output = output_node;

    if (ctx->wl.output_capture_source_manager != NULL) {
        output_target->header.source = ext_output_image_capture_source_manager_v1_create_source(ctx->wl.output_capture_source_manager, output_node->output);
    }

    return (wlm_mirror_target_t *)output_target;
}

static wlm_mirror_target_t * create_toplevel_target(ctx_t * ctx, wlm_wayland_toplevel_entry_t * toplevel_node) {
    wlm_mirror_target_toplevel_t * toplevel_target = calloc(1, sizeof *toplevel_target);
    toplevel_target->header.type = WLM_MIRROR_TARGET_TYPE_TOPLEVEL;
    toplevel_target->header.source = NULL;
    // toplevel buffers are captured upright, there is no output transform to undo
    toplevel_target->header.transform = WL_OUTPUT_TRANSFORM_NORMAL;
    toplevel_target->toplevel = toplevel_node;

    if (ctx->wl.toplevel_capture_source_manager != NULL) {
        toplevel_target->header.source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(ctx->wl.toplevel_capture_source_manager, toplevel_node->handle);
    } else {
        wlm_log_warn("mirror-target::create_toplevel_target(): compositor does not support ext_foreign_toplevel_image_capture_source_manager_v1\n");
    }

    return (wlm_mirror_target_t *)toplevel_target;
}

// --- public functions ---

wlm_mirror_target_t * wlm_mirror_target_parse(ctx_t * ctx, const char * target_str) {
    // find prefix
    const char * prefix_separator = strchr(target_str, ':');

    // no prefix
    if (prefix_separator == NULL) {
        // match output using the whole target string
        // fallback to legacy wl-mirror behaviour
        return wlm_mirror_target_find_output(ctx, target_str);
    }

    // find prefix length and pointer to target name after prefix
    int prefix_len = prefix_separator - target_str;
    const char * prefix_str = target_str;
    const char * target_name_str = prefix_separator + 1;

    // match different prefix kinds
    if (strncmp(prefix_str, "null:", prefix_len) == 0 && strcmp(target_name_str, "") == 0) {
        return create_null_target(ctx);
    } else if (strncmp(prefix_str, "output:", prefix_len) == 0) {
        // match output
        return wlm_mirror_target_find_output(ctx, target_name_str);
    } else if (strncmp(prefix_str, "toplevel:", prefix_len) == 0) {
        // match toplevel
        return wlm_mirror_target_find_toplevel(ctx, target_name_str);
    } else {
        // unknown prefix
        wlm_log_error("mirror-target::parse(): invalid target prefix '%.*s'\n", prefix_len, prefix_str);
        return NULL;
    }
}

bool wlm_mirror_target_spec_is_toplevel(const char * spec) {
    return spec != NULL && strncmp(spec, WLM_MIRROR_TARGET_PREFIX_TOPLEVEL, strlen(WLM_MIRROR_TARGET_PREFIX_TOPLEVEL)) == 0;
}

const char * wlm_mirror_target_spec_output_name(const char * spec) {
    // 'output:NAME' and legacy bare 'NAME' both name an output
    if (spec != NULL && strncmp(spec, WLM_MIRROR_TARGET_PREFIX_OUTPUT, strlen(WLM_MIRROR_TARGET_PREFIX_OUTPUT)) == 0) {
        return spec + strlen(WLM_MIRROR_TARGET_PREFIX_OUTPUT);
    }

    return spec;
}

// the single place where the mirror target is found and created from the options
wlm_mirror_target_t * wlm_mirror_target_resolve(ctx_t * ctx, region_t * region) {
    if (wlm_mirror_target_spec_is_toplevel(ctx->opt.output)) {
        const char * spec = ctx->opt.output + strlen(WLM_MIRROR_TARGET_PREFIX_TOPLEVEL);

        if (ctx->opt.has_region) {
            wlm_log_error("mirror-target::resolve(): regions are not supported for toplevel targets\n");
            return NULL;
        }

        wlm_mirror_target_t * target = wlm_mirror_target_find_toplevel(ctx, spec);
        if (target == NULL) {
            wlm_log_error("mirror-target::resolve(): no toplevel matches '%s' (see --list-toplevels)\n", spec);
            return NULL;
        }

        *region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
        return target;
    }

    wlm_wayland_output_entry_t * output_node = NULL;
    if (!wlm_opt_find_output(ctx, &output_node, region)) {
        return NULL;
    }

    return create_output_target(ctx, output_node);
}

// TODO: remove this
wlm_mirror_target_t * wlm_mirror_target_create_output(ctx_t * ctx, wlm_wayland_output_entry_t * output_node) {
    return create_output_target(ctx, output_node);
}

wlm_mirror_target_t * wlm_mirror_target_find_output(ctx_t * ctx, const char * name) {
    // try to match output by name
    wlm_wayland_output_entry_t * output_node = NULL;
    if (!wlm_wayland_find_output(ctx, name, &output_node)) {
        return NULL;
    }

    // kanshi syntax:
    //   match when:
    //     name == '*' or
    //     name == output name or
    //     name == '{output make} {output model} {output serial}'
    //
    // sway syntax
    //   match when
    //     name == '*' or
    //     name == output name or
    //     name == '{output make} {output model} {output serial}'
    //
    // wl-mirror syntax
    //   match when
    //     name == output name or
    //     name == '{output make} {output model} {output serial}' or

    // TODO: double-check that this syntax is ok

    return create_output_target(ctx, output_node);
}

wlm_mirror_target_t * wlm_mirror_target_find_toplevel(ctx_t * ctx, const char * identifier) {
    // matching rules are documented in wlm_wayland_find_toplevel()
    wlm_wayland_toplevel_entry_t * toplevel_node = NULL;
    if (!wlm_wayland_find_toplevel(ctx, identifier, &toplevel_node)) {
        return NULL;
    }

    return create_toplevel_target(ctx, toplevel_node);
}

wlm_mirror_target_type_t wlm_mirror_target_get_type(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return WLM_MIRROR_TARGET_TYPE_NULL;
    }

    return target->type;
}

wlm_wayland_toplevel_entry_t * wlm_mirror_target_get_toplevel_node(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return NULL;
    }

    if (target->type != WLM_MIRROR_TARGET_TYPE_TOPLEVEL) {
        return NULL;
    }

    wlm_mirror_target_toplevel_t * toplevel_target = (wlm_mirror_target_toplevel_t *)target;
    return toplevel_target->toplevel;
}

const char * wlm_mirror_target_get_name(wlm_mirror_target_t * target) {
    wlm_wayland_output_entry_t * output_node = wlm_mirror_target_get_output_node(target);
    if (output_node != NULL) {
        return output_node->name != NULL ? output_node->name : "";
    }

    wlm_wayland_toplevel_entry_t * toplevel_node = wlm_mirror_target_get_toplevel_node(target);
    if (toplevel_node != NULL) {
        if (toplevel_node->app_id != NULL && toplevel_node->app_id[0] != '\0') return toplevel_node->app_id;
        if (toplevel_node->title != NULL) return toplevel_node->title;
        if (toplevel_node->identifier != NULL) return toplevel_node->identifier;
    }

    return "";
}

wlm_wayland_output_entry_t * wlm_mirror_target_get_output_node(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return NULL;
    }

    if (target->type != WLM_MIRROR_TARGET_TYPE_OUTPUT) {
        return NULL;
    }

    wlm_mirror_target_output_t * output_target = (wlm_mirror_target_output_t *)target;
    return output_target->output;
}

struct ext_image_capture_source_v1 * wlm_mirror_target_get_capture_source(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return NULL;
    }

    return target->source;
}

enum wl_output_transform wlm_mirror_target_get_transform(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return WL_OUTPUT_TRANSFORM_NORMAL;
    }

    return target->transform;
}

void wlm_mirror_target_destroy(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return;
    }

    if (target->source != NULL) {
        ext_image_capture_source_v1_destroy(target->source);
    }

    free(target);
}
