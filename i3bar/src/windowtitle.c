/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 *
 */
#include "common.h"

#include <string.h>
#include <stdlib.h>

#include <yajl/yajl_parse.h>

struct windowtitle_json_params {
    char *json;
    char *cur_key;
    char* name;
    bool pango_markup;
    windowtitle * windowtitle;
};


/*
 * Parse a string (change)
 *
 */
static int windowtitle_string_cb(void *params_, const unsigned char *val, size_t len) {
    struct windowtitle_json_params *params = (struct windowtitle_json_params *)params_;

    if (!strcmp(params->cur_key, "windowtitle")) {
        sasprintf(&(params->name), "%.*s", len, val);
        FREE(params->cur_key);
        return 1;
    }

    FREE(params->cur_key);
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

    /* Save the name */
    params->windowtitle->name = i3string_from_utf8(params->name);
    i3string_set_markup(params->windowtitle->name, params->pango_markup);
    /* Save its rendered width */
    params->windowtitle->name_width = predict_text_width(params->windowtitle->name);

    DLOG("Got windowtitle change: %s\n", i3string_as_utf8(params->windowtitle->name));
    FREE(params->cur_key);

    return 1;
}

/* A datastructure to pass all these callbacks to yajl */
static yajl_callbacks windowtitle_callbacks = {
    .yajl_string = windowtitle_string_cb,
    .yajl_map_key = windowtitle_map_key_cb,
    .yajl_end_map = windowtitle_end_map_cb,
};


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
    params.windowtitle = &title;

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

    /* Set the new title windowtitle */
    set_current_windowtitle(&title);

    yajl_free(handle);

    FREE(params.cur_key);
}
