#ifndef _ESCAPER_INC
#define _ESCAPER_INC

#include "sds/sds.h"
#include <string.h>

#ifndef NULL
#  define NULL ((void *)0)
#endif
static sds escape_html_content(char* content) {
    size_t len = strlen(content);
    sds ret = sdsnewlen(NULL, len + len / 2);
    sdsupdatelen(ret);

    char *p;
    char *safe_chunk_st = content;

    p = content;
    char ch;
    while ((ch = *p)) {
        char* esc_entity = NULL;
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
        if (esc_entity) {
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
        return sdscatlen(appendee, &ch, 1);
}


static sds escape_html_attr(char* attr) {
    sds ret = sdsnewlen(NULL, strlen(attr) * 2);
    sdsupdatelen(ret);
    char *p;

    for (p = attr; *p; p++) {
        char ch = *p;
        switch (ch) {
        case '&':  
            ret = sdscat(ret, "&amp;");
            break;
        case '\"': 
            ret = sdscat(ret, "&quot;");
            break;
        case '\'':
            ret = sdscat(ret, "&apos;");
            break;
        case '<':
            ret = sdscat(ret, "&lt;");
            break;
        case '>':
            ret = sdscat(ret, "&gt;");
            break;
        default:
            ret = sdscatlen(ret, &ch, 1);
            break;
        }
    }
#undef __TO_HEX__
    return ret;
}

#endif
