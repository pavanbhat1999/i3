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
#include "log.h"
#include "windowtitle.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*
 * Start parsing the received JSON string
 *
 */
void parse_windowtitle_str(char *str) {

    /* FIXME: Fasciliate stream processing, i.e. allow starting to interpret
     * JSON in chunks */
    // struct windowtitle_json_params params;

    if(strlen(str) == 0) str = "desktop\0";

    windowtitle * title =(windowtitle *) malloc(sizeof(windowtitle));
    title->name = i3string_from_utf8(str);
    i3string_set_markup(title->name, false);
    title->name_width = predict_text_width(title->name);

    set_current_windowtitle(title);

    FREE(title);

}
