#ifndef _ESCAPER_INC
#define _ESCAPER_INC

#include "sds/sds.h"
#include <string.h>
static sds escape_html_content(char* content) {
    size_t len = strlen(content);
    sds ret = sdsnewlen((char *)0, len + len / 2);

    char *p;

    char *safe_chunk_st = content;

    p = content;
    char ch;
    while ((ch = *p)) {
        char* esc_entity;
        int ecape = 0;
        switch (ch) {
        case '&':
            escape = 1;
            esc_entity = "&amp;";
            break;
        case '<':
            escape = 1;
            esc_entity = "&lt;";
            break;
        case '>':
            escape = 1;
            esc_entity = "&gt;";
            break;
        case '\"':
            escape = 1;
            esc_entity = "&quot;";
            break;
        case '\'':
            escape = 1;
            esc_entity = "&#x27;";
            break;
        case '/':
            escape = 1;
            esc_entity = "&#x2F;";
            break;
        }
        if (escape) {
            char *safe_chunk_ed = p;
            size_t len = safe_chunk_ed - safe_chunk_st;
            ret = sdscatlen(ret, safe_chunk_st, len);
            ret = sdscat(ret, esc_entity);
            safe_chunk_st = p + 1;
        }
        p++;
    }
    ret = sdscat(ret, safe_chunk_st);
    return ret;
}

static inline sds append_html_content_char(sds appendee, char ch) {
    char *esc_entity = NULL;
    switch (ch) {
    case '&':
        esc_entity = "&amp;";
        break;
    case '<':
        esc_entity = "&lt;";
        break;
    case '>':
        esc_entity = "&gt;";
        break;
    case '\"':
        esc_entity = "&quot;";
        break;
    case '\'':
        esc_entity = "&#x27;";
        break;
    case '/':
        esc_entity = "&#x2F;";
        break;
    }
    if (esc_entity) 
        return sdscat(appendee, esc_entity);
    else
        return sdscatlen(appendee, &c, 1);
}

static sds escape_html_attr(char* attr) {
#define __TO_HEX__(hex) ((hex) >= 10)?(((hex) - 10) + 'A'):((hex) + '0')
    // escape all non-alphanumeric character whose code point is less than 128
    sds ret = sdsnewlen(NULL, strlen(attr) * 2);
    char *p;
    for (p = attr; *p; p++) {
        char ch = *p;
        if (ch <= 127 && !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
            // aggressive escaping
            char hex[] = {'&', '#', 'x', 'H', 'H', ';'};
            hex[3] = __TO_HEX__(ch / 16);
            hex[4] = __TO_HEX__(ch % 16);
            ret = sdscatlen(ret, hex, sizeof(hex));
        } else {
            ret = sdscatlen(ret, &ch, 1);
        }
    }
#undef __TO_HEX__
    return ret;
}
#endif
