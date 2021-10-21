/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 *
 */
#include "common.h"
#include "libi3.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <yajl/yajl_parse.h>

struct windowtitle_json_params {
    char *json;
    char *cur_key;
    windowtitle *title;
};

/*
 * Parse a string (change)
 *
 */
static int windowtitle_string_cb(void *params_, const unsigned char *val, size_t len) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;

    if (!strcmp(params->cur_key, "windowtitle")) {
        char* str;
        sasprintf(&(str), "%.*s", len, val);
        params->title->name = i3string_from_utf8(str);
        params->title->name_width = predict_text_width(params->title->name);

        FREE(str);
        FREE(params->cur_key);
        return 1;
    }

    FREE(params->cur_key);
    return 0;
}

static int windowtitle_integer_cb(void *params_, long long int val) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;
    if (strcmp(params->cur_key, "xcb_window_id") == 0) {
        params->title->id = val;
        return 1;
    }
    return 0;
}

static int windowtitle_boolean_cb(void *params_, int val) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;
    if (strcmp(params->cur_key, "is_pango_markup") == 0) {
        i3string_set_markup(params->title->name, val);
        return 1;
    }
    return 0;
}

/*
 * Parse a key.
 *
 * Essentially we just save it in the parsing state
 *
 */
static int windowtitle_map_key_cb(void *params_, const unsigned char *keyVal, size_t keyLen) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;
    FREE(params->cur_key);
    sasprintf(&(params->cur_key), "%.*s", keyLen, keyVal);
    return 1;
}

static int windowtitle_end_map_cb(void *params_) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;

    DLOG("Got windowtitle change: %s\n", i3string_as_utf8(params->title->name));
    FREE(params->cur_key);

    return 1;
}

/* A datastructure to pass all these callbacks to yajl */
static yajl_callbacks windowtitle_callbacks = {
    .yajl_string = windowtitle_string_cb,
    .yajl_boolean = windowtitle_boolean_cb,
    .yajl_integer = windowtitle_integer_cb,
    .yajl_map_key = windowtitle_map_key_cb,
    .yajl_end_map = windowtitle_end_map_cb,
};


void parse_windowtitle_icon(struct windowtitle_json_params *params) {
    if (!params->title->id || params->title->id <= 0) {
        // ELOG("return before build: %d\n", params->title->id);
        return;
    }

    uint32_t *data = NULL;
    uint32_t width, height;
    uint64_t len = 0;
    const uint32_t pref_size = (uint32_t)(36 - logical_px(2));
    const char* atom_name = "_NET_WM_ICON";
    xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (!reply) {
        ELOG("Could not get atom %s\n", atom_name);
        exit(-1);
    }
    xcb_atom_t A__NET_WM_ICON = reply->atom;
    free(reply);

    xcb_get_property_cookie_t wm_icon_cookie;
    wm_icon_cookie = xcb_get_property(conn, false, params->title->id, A__NET_WM_ICON, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);

    xcb_get_property_reply_t * prop =  xcb_get_property_reply(conn, wm_icon_cookie, NULL);

    if (!prop || prop->type != XCB_ATOM_CARDINAL || prop->format != 32) {
        ELOG("_NET_WM_ICON is not set\n");
        params->title->icon = NULL;
        FREE(prop);
        return;
    }

    uint32_t prop_value_len = xcb_get_property_value_length(prop);
    uint32_t *prop_value = (uint32_t *)xcb_get_property_value(prop);

    /* Find an icon matching the preferred size.
     * If there is no such icon, take the smallest icon having at least
     * the preferred size.
     * If all icons are smaller than the preferred size, chose the largest.
     */
    while (prop_value_len > (sizeof(uint32_t) * 2) && prop_value &&
        prop_value[0] && prop_value[1]) {
        const uint32_t cur_width = prop_value[0];
        const uint32_t cur_height = prop_value[1];
        /* Check that the property is as long as it should be (in bytes),
           handling integer overflow. "+2" to handle the width and height
           fields. */
        const uint64_t cur_len = cur_width * (uint64_t)cur_height;
        const uint64_t expected_len = (cur_len + 2) * 4;

        if (expected_len > prop_value_len) {
            break;
        }

        DLOG("Found _NET_WM_ICON of size: (%d,%d)\n", cur_width, cur_height);

        const bool at_least_preferred_size = (cur_width >= pref_size &&
            cur_height >= pref_size);
        const bool smaller_than_current = (cur_width < width ||
            cur_height < height);
        const bool larger_than_current = (cur_width > width ||
            cur_height > height);
        const bool not_yet_at_preferred = (width < pref_size ||
            height < pref_size);
        if (len == 0 ||
            (at_least_preferred_size &&
                (smaller_than_current || not_yet_at_preferred)) ||
            (!at_least_preferred_size &&
                not_yet_at_preferred &&
                larger_than_current)) {
            len = cur_len;
            width = cur_width;
            height = cur_height;
            data = prop_value;
        }

        if (width == pref_size && height == pref_size) {
            break;
        }

        /* Find pointer to next icon in the reply. */
        prop_value_len -= expected_len;
        prop_value = (uint32_t *)(((uint8_t *)prop_value) + expected_len);
    }

    if (!data) {
        DLOG("Could not get _NET_WM_ICON\n");
        FREE(prop);
        return;
    }

    DLOG("Using icon of size (%d,%d) (preferred size: %d)\n",
        width, height, pref_size);

    uint32_t *icon = smalloc(len * 4);

    for (uint64_t i = 0; i < len; i++) {
        uint8_t r, g, b, a;
        const uint32_t pixel = data[2 + i];
        a = (pixel >> 24) & 0xff;
        r = (pixel >> 16) & 0xff;
        g = (pixel >> 8) & 0xff;
        b = (pixel >> 0) & 0xff;

        /* Cairo uses premultiplied alpha */
        r = (r * a) / 0xff;
        g = (g * a) / 0xff;
        b = (b * a) / 0xff;

        icon[i] = ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
    }

    params->title->icon = cairo_image_surface_create_for_data((unsigned char *)icon,
                                                              CAIRO_FORMAT_ARGB32,
                                                              width,
                                                              height,
                                                              width * 4);
    params->title->icon_size = width;

    static cairo_user_data_key_t free_data;
    cairo_surface_set_user_data(params->title->icon, &free_data, icon, free);

    FREE(prop);
}

/*
 * Start parsing the received JSON string
 *
 */
void parse_windowtitle_json(char *json) {
    /* FIXME: Fasciliate stream processing, i.e. allow starting to interpret
     * JSON in chunks */
    struct windowtitle_json_params params;

    windowtitle title;

    params.cur_key = NULL;
    params.json = json;
    params.title = &title;

    yajl_handle handle;
    yajl_status state;

    handle = yajl_alloc(&windowtitle_callbacks, NULL, (void *)&params);

    state = yajl_parse(handle, (const unsigned char *)json, strlen(json));

    /* FIXME: Proper error handling for JSON parsing */
    switch (state) {
    case yajl_status_ok:
        break;
    case yajl_status_client_canceled:
    case yajl_status_error:
        ELOG("Could not parse windowtitle event!\n");
        exit(EXIT_FAILURE);
        break;
    }

    parse_windowtitle_icon(&params);

    /* Set the new title windowtitle */
    set_current_windowtitle(&title);

    yajl_free(handle);

    FREE(params.cur_key);
}
