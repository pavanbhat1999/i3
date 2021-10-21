/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * window_title.c -- Show window 
 *
 */
#pragma once

#include <config.h>

#include <xcb/xproto.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>


/* Name of current windowtitle and its render width */
struct _windowtitle {
    xcb_window_t id;

    i3String *name;
    int name_width;

    cairo_surface_t *icon;
    uint32_t icon_size;
};

typedef struct _windowtitle windowtitle;

/*
 * Start parsing the received JSON string
 *
 */
void parse_windowtitle_json(char *str);
