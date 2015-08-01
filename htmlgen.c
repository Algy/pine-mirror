#include <assert.h>

#include "htmlgen.h"
#include "namugen.h"
#include "varray.h"

/*
 * HTML Sanitizer
 */
#include "tidy-html5/include/tidy.h"
#include "tidy-html5/include/buffio.h"

const char *allowed_tags[] = {
    "p", 
    "pre", 
    "blockquote", 
    "ul", 
    "div", 
    "a", 
    "em", 
    "strong", 
    "s", 
    "sub", 
    "i", 
    "b", 
    "u", 
    "ruby", 
    "rt", 
    "rp", 
    "span", 
    "br", 
    "ins", 
    "del", 
    "img", 
    "iframe", 
    "embed", 
    "object", 
    "param", 
    "video",
    0
};

const char *allowed_attributes[] = {
    "href",
    "src",
    "style",
    "class",
    "width",
    "height", 
    "alt", 
    "align",
    0
};


const char *allowed_link_schema [] = {
    "http://",
    "https://",
    "//", 
    "/",
    0
};

static const char* iter_entity(const char *p, const char* end, int32_t *cp_out) {
    if (p < end) {
        if (p + 1 < end && *p == '&' && *(p + 1) == '#') {
            const char *testp = p + 2;
            int base = 10;
            if (testp < end && *testp == 'x') {
                testp++;
                base = 16;
            }
            int32_t codepoint = 0;
            bool entered = false;
            while (testp < end) {
                char cn = *testp;
                if ('0' <= cn && cn <= '9') {
                    codepoint = codepoint * base + (cn - '0');
                    entered = true;
                } else if (base == 16) {
                    if (cn >= 'a' && cn <= 'f') {
                        codepoint = codepoint * base + (cn - 'a') + 10;
                        entered = true;
                    } else if (cn >= 'A' && cn <= 'F') {
                        codepoint = codepoint * base + (cn - 'A') + 10;
                        entered = true;
                    } else
                        break;
                } else
                    break;
                testp++;
            }
            if (!entered)
                goto as_char;
            if (testp < end && *testp == ';')
                testp++;
            p = testp;
            *cp_out = codepoint;
            return testp;
        }
as_char:
        *cp_out = (int32_t)*p;
        return p + 1;
    } else {
        *cp_out = -1;
        return end;
    }
}

static size_t filter_html_entities(char* src, size_t len) {
    char* acc = src;
    const char* p = src;
    const char* end = src + len;
    while (p < end) {
        switch (*p) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
            p++;
            break;
        default:
            {
                int32_t cp;
                const char* next_p = iter_entity(p, end, &cp);
                if (cp >= ' ' && cp <= '~') {
                    *acc++ = (char)cp;
                    p = next_p;
                } else {
                    *acc++ = *p++;
                }
            }
            break;
        }
    }
    *acc = 0;
    return acc - src;
}

static bool acceptable_src_href(const char* src, size_t len) {
    sds buf = sdsnewlen(src, len);
    filter_html_entities(buf, len);
    sdsupdatelen(buf);
    len = sdslen(buf);

    const char* p = buf;
    const char *end = buf + len;

    while (p < end) {
        int32_t cp;
        const char* next_p = iter_entity(p, end, &cp);
        if (cp <= ' ') {
            p = next_p;
        } else
            break;
    }
    const char **schema;
    bool success = false;
    for (schema = allowed_link_schema; *schema; schema++) {
        if (!strncasecmp(*schema, p, strlen(*schema))) {
            success = true;
            break;
        }
    }
    sdsfree(buf);
    return success;
}

static bool acceptable_style(const char* style, size_t len) {
    sds buf = sdsnewlen(style, len);
    filter_html_entities(buf, len);
    sdsupdatelen(buf);
    len = sdslen(buf);

    char* p = buf;
    char *end = buf + len;

    while (p < end) {
        if (!strncasecmp(p, "url", 3)) {
            p += 3;
            while (p < end && *p == ' ') p++;
            if (p < end && *p == '(') {
                p++;
                if (!acceptable_src_href(p, len))
                    goto error;
                while (p < end && *p != ')')
                    p++;
                if (p < end) p++;
            }
        } else if (!strncasecmp(p, "expression", 10)) {
            p += 10;
            while (p < end && *p == ' ') p++;
            if (p < end && *p == '(') {
                goto error;
            }
        } else
            p++;
    }
    sdsfree(buf);
    return true;
error:
    sdsfree(buf);
    return false;
}

static void filter_html_buffer(char *p, size_t len) {
    char *acc = p;
    char *end = p + len;
    while (p < end) {
        while (p < end && *p != '<') {
            *acc++ = *p++;
        }
        // acc: before <
        if (p >= end) 
            break;
        char *brace_st = p;
        p++; // consume '<'

        if (*p == '/') {
            p++;
        }
        char *tag_st = p;
        while (p < end && *p != ' ' && *p != '>') {
            p++;
        }
        char *tag_ed = p;

        bool valid_tag = false;
        const char** ptag;
        for (ptag = allowed_tags; *ptag; ptag++) {
            if (!strncasecmp(*ptag, tag_st, tag_ed - tag_st)) {
                valid_tag = true;
                break;
            }
        }
        if (!valid_tag) {
            while (p < end && *p != '>')
                p++;
            if (p < end)
                p++;
        } else {
            const char *after_tag_name = p;
            const char *tmp;
            for (tmp = brace_st; tmp < after_tag_name; tmp++)
                *acc++ = *tmp;
            // acc: after tag name

            while (p < end) {
                while (p < end && (*p == ' ' || *p == '\n')) {
                    p++;
                }
                if (p < end && *p == '>') {
                    p++;
                    break;
                }
                char *attr_name_st = p;
                while (p < end && *p != '=' && *p != ' ' && *p != '>')
                    p++;
                char *attr_name_ed = p;
                char *attr_value_st = NULL, *attr_value_ed;
                if (p < end && *p == '=') {
                    p++;
                    while (p < end && (*p == ' ' || *p == '\n')) {
                        p++;
                    }

                    if (*p == '\'') {
                        p++;
                        attr_value_st = p;
                        while (p < end && *p != '\'')
                            p++;
                        attr_value_ed = p;
                        if (p < end)
                            p++;
                    } else if (*p == '"') {
                        p++;
                        attr_value_st = p;
                        while (p < end && *p != '"')
                            p++;
                        attr_value_ed = p;
                        if (p < end)
                            p++;
                    }
                }
                char *attr_end = p;

                const char** aattr;
                bool valid_attr = false;
                for (aattr = allowed_attributes; *aattr; aattr++) {
                    if (!strncasecmp(attr_name_st, *aattr, attr_name_ed - attr_name_st)) {
                        valid_attr = true;
                        break;
                    }
                }

                if (valid_attr && attr_value_st) {
                    if (!strncasecmp(attr_name_st, "src", attr_name_ed - attr_name_st) || !strncasecmp(attr_name_st, "href", attr_name_ed - attr_name_st)) {
                        if (!acceptable_src_href(attr_value_st, attr_value_ed - attr_value_st)) {
                            valid_attr = false;
                        }
                    } else if (!strncasecmp(attr_name_st, "style", attr_name_ed - attr_name_st)) {
                        if (!acceptable_style(attr_value_st, attr_value_ed - attr_value_st)) {
                            valid_attr = false;
                        }
                    }
                }
                if (valid_attr) {
                    *acc++ = ' ';
                    for (tmp = attr_name_st; tmp < attr_end; tmp++) {
                        *acc++ = *tmp;
                    }
                }
            }
            *acc++ = '>';
        }
    }
    *acc = 0;
}

static sds sdscat_sanitize_src_href(sds buf, const sds src) {
    if (acceptable_src_href(src, sdslen(src))) {
        buf = sdscatsds(buf, src);
    }
    return buf;
}

static sds sdscat_sanitize_style(sds buf, const sds style) {
    if (acceptable_style(style, sdslen(style))) {
        buf = sdscatsds(buf, style);
    }
    return buf;
}

static sds sdscat_sanitize_html(sds buf, const sds src) {
    sds src_copy = sdsdup(src);
    filter_html_entities(src_copy, sdslen(src_copy));
    sdsupdatelen(src_copy);

    TidyBuffer output, errbuf;
    tidyBufInit(&output);
    tidyBufInit(&errbuf);

    TidyDoc tdoc = tidyCreate();
    tidySetErrorBuffer(tdoc, &errbuf);
    
    tidyOptSetValue(tdoc, TidyBodyOnly, "yes");
    tidyOptSetValue(tdoc, TidyInCharEncoding, "utf8");
    tidyOptSetValue(tdoc, TidyOutCharEncoding, "utf8");
    tidyOptSetValue(tdoc, TidyLiteralAttribs, "no");
    tidyOptSetValue(tdoc, TidyDropEmptyParas, "no");
    tidyOptSetValue(tdoc, TidyDropEmptyElems, "no");
    tidyParseString(tdoc, src_copy);
    tidyCleanAndRepair(tdoc);
    tidySaveBuffer(tdoc, &output);

    sds p;
    if (output.size > 0) {
        p = sdsnew((char *)output.bp);
    } else {
        p = sdsempty();
    }
    sdsfree(src_copy);
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);

    filter_html_buffer(p, sdslen(p));
    sdsupdatelen(p);
    buf = sdscatsds(buf, p);
    sdsfree(p);
    return buf;
}


/*
 * Macro system
 */

static sds inclusion_converter(htmlgen_ctx *ctx, htmlgen_macro_record *_, struct namuast_inl_macro *macro, struct namuast_container *cur_ast, sds buf);
static sds simple_macro_converter(htmlgen_ctx *ctx, htmlgen_simple_macro_record *record, struct namuast_inl_macro *macro, sds buf);

#include "escaper.inc"

static htmlgen_macro_record htmlgen_internal_macros[] = {
    {"include", inclusion_converter},
    {0, 0}
};

static htmlgen_simple_macro_record htmlgen_internal_simple_macros[]  = {
    {"youtube", "<iframe class='wiki-youtube' allowfullscreen='' width='640' height='360' frameborder='0' src='//www.youtube.com/embed/{{0}}'></iframe>"},
    {"anchor", "<a id='{{0}}'></a>"},
    {0, 0}
};


#define HTML_OP(x, op, ...) (namuast_html_ops[((struct namuast_base *)(x))->ast_type].op(((struct namuast_base *)(x)), __VA_ARGS__))
#define INL_HTML_OP(x, op, ...) (namuast_inl_html_ops[((struct namuast_inline *)(x))->inl_type].op(((struct namuast_inline *)(x)), __VA_ARGS__))

struct {
    sds (*to_html)(namuast_base *, htmlgen_ctx *, sds buf);
    void (*get_lname)(namuast_base *, varray* link_names);
} namuast_html_ops[namuast_type_N];

struct {
    sds (*to_html)(namuast_inline*, htmlgen_ctx *, sds buf);
    void (*get_lname)(namuast_inline *, varray* link_names);
} namuast_inl_html_ops[namuast_inltype_N];


void htmlgen_init(htmlgen_ctx *ctx, const char *cur_doc_name, struct namugen_doc_itfc *doc_itfc) {
    ctx->ne_docs = NULL;
    ctx->ne_docs_count = 0;
    ctx->doc_itfc = doc_itfc;
    ctx->last_emitted_fnt = NULL;
    ctx->cur_doc_name = sdsnew(cur_doc_name);
    ctx->includer_info = NULL;
}

void htmlgen_remove(htmlgen_ctx *ctx) {
    sdsfree(ctx->cur_doc_name);
}

static bool htmlgen_already_included(htmlgen_ctx *ctx, const char *doc_name) {
    htmlgen_ctx *p;

    for (p = ctx; p->includer_info; p = p->includer_info->includer_ctx) {
        if (!strcmp(p->cur_doc_name, doc_name)) {
            return true;
        }
    }
    return false;
}

static char* align_to_str (enum nm_align_type t) {
    switch (t) {
    case nm_align_none:
    case nm_valign_none:
        return "";
    case nm_align_left:
        return "left";
    case nm_align_right:
        return "right";
    case nm_align_center:
    case nm_valign_center:
        return "center";
    case nm_valign_top:
        return "top";
    case nm_valign_bottom:
        return "bottom";
    default:
        return "unknown";
    }
}

static char* span_type_to_str(enum nm_span_type type, bool opening_tag) { 
    if (opening_tag) {
        switch (type) {
        case nm_span_bold:
            return "<b>";
        case nm_span_italic:
            return "<i>";
        case nm_span_strike:
            return "<del>";
        case nm_span_underline:
            return "<u>";
        case nm_span_superscript:
            return "<sup>";
        case nm_span_subscript:
            return "<sub>";
        default:
            return "<span>";
        }
    } else {
        switch (type) {
        case nm_span_bold:
            return "</b>";
        case nm_span_italic:
            return "</i>";
        case nm_span_strike:
            return "</del>";
        case nm_span_underline:
            return "</u>";
        case nm_span_superscript:
            return "</sup>";
        case nm_span_subscript:
            return "</sub>";
        default:
            return "</span>";
        }
    }
}

static int _s_sdscmp(const void *lhs, const void *rhs) {
    return sdscmp(*(const sds *)lhs, *(const sds *)rhs);
}

static bool doc_doesnt_exist(htmlgen_ctx *ctx, sds docname) {
    if (ctx->ne_docs)
        return bsearch(&docname, ctx->ne_docs, ctx->ne_docs_count, sizeof(sds), _s_sdscmp) != NULL;
    else
        return false;
}

/*
 * to_html operations
 */

static sds container_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    namuast_container *container = (namuast_container *)base;

    size_t idx = 0;
    size_t len = container->len;
    while (idx < len) {
        if (idx + 1 < len && 
            container->children[idx]->ast_type == namuast_type_inline &&
            container->children[idx + 1]->ast_type == namuast_type_return) {

            buf = sdscat(buf, "<p>");
            buf = HTML_OP(container->children[idx], to_html, ctx, buf);
            buf = sdscat(buf, "</p>");
            idx += 2;
        } else {
            buf = HTML_OP(container->children[idx], to_html, ctx, buf);
            idx += 1;
        }
    }
    return buf;
}


static sds return_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    return sdscat(buf, "<br>");
}

static sds hr_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    return sdscat(buf, "<hr class='wiki-hr'>");
}

static sds quotation_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    struct namuast_quotation *quote = (struct namuast_quotation *)base;
    buf = sdscat(buf, "<blockquote class='wiki-quote'>");
    buf = INL_HTML_OP(quote->content, to_html, ctx, buf);
    buf = sdscat(buf, "</blockquote>");
    return buf;
}

static sds block_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    struct namuast_block *block = (struct namuast_block *)base;
    switch (block->block_type) {
    case block_type_html:
        buf = sdscat_sanitize_html(buf, block->data.html);
        break;
    case block_type_raw:
        {
            sds raw = block->data.raw;
            size_t rawlen = sdslen(raw);
            size_t idx;
            buf = sdscat(buf, "<pre>");
            for (idx = 0; idx < rawlen; idx++) {
                buf = append_html_content_char(buf, raw[idx]);
            }
            buf = sdscat(buf, "</pre>");
        }
        break;
    default:
        // NON-REACHABLE
        assert(0);
    }
    return buf;
}

static inline sds add_html_attr(sds s, const char* key, char* value) {
    // if value is NULL, don't append
    // if value is "", append key only
    if (value) {
        s = sdscat(s, " ");
        s = sdscat(s, key);
        if (*value) {
            sds esc_val = escape_html_attr(value);
            s = sdscat(s, "='");
            s = sdscat(s, esc_val);
            s = sdscat(s, "'");
            sdsfree(esc_val);
        }
    }
    return s;
}

static inline sds add_html_style(sds s, const char* key, char* value) {
    // if value is NULL, don't append
    if (value) {
        sds esc_val = escape_html_attr(value);
        s = sdscat(s, key);
        s = sdscat(s, ": ");
        s = sdscat(s, esc_val);
        s = sdscat(s, "; ");
        sdsfree(esc_val);
    }
    return s;
}

#define APPEND(buf, x) (buf = sdscat(buf, x))

static sds table_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    struct namuast_table *tbl = (struct namuast_table *)base;
    sds tbl_style = sdsempty();
    sds tbl_attr = sdsempty();
    char* tbl_class = NULL;

    switch (tbl->align) {
    case nm_align_left:
        tbl_class = "wiki-table table-left";
        break;
    case nm_align_right:
        tbl_class = "wiki-table table-right";
        break;
    case nm_align_center:
        tbl_class = "wiki-table table-center";
        break;
    default:
        tbl_class = "";
        break;
    }

    

    tbl_style = add_html_style(tbl_style, "background-color", tbl->bg_webcolor);

    tbl_attr = add_html_attr(tbl_attr, "width", tbl->width);
    tbl_attr = add_html_attr(tbl_attr, "bordercolor", tbl->border_webcolor);
    tbl_attr = add_html_attr(tbl_attr, "height", tbl->height);

    buf = sdscat(buf, "<div class='wiki-table-wrap'>");
        buf = sdscat(buf, "<table class='");
            buf = sdscat(buf, tbl_class);
        buf = sdscat(buf, "' style='");
            buf = sdscat_sanitize_style(buf, tbl_style);
        buf = sdscat(buf, "' ");
            buf = sdscatsds(buf, tbl_attr);
        buf = sdscat(buf, ">");

        if (tbl->caption) {
            buf = sdscat(buf, "<caption>");
            buf = INL_HTML_OP(tbl->caption, to_html, ctx, buf);
            buf = sdscat(buf, "</caption>");
        }

        buf = sdscat(buf, "<tbody>");
    sdsfree(tbl_style);
    sdsfree(tbl_attr);

    size_t idx, kdx;
    for (idx = 0; idx < tbl->row_count; idx++) {
        struct namuast_table_row *row = &tbl->rows[idx];

        sds row_head = sdsnew("<tr");

        if (row->bg_webcolor) {
            row_head = sdscat(row_head, " style='");

            sds row_style = sdsnew("background-color: "); 
            row_style = sdscat_escape_html_attr(row_style, row->bg_webcolor);

            row_head = sdscat_sanitize_style(row_head, row_style); 
            row_head = sdscat(row_head, "'"); 
            sdsfree(row_style);
        }
        row_head = sdscat(row_head, ">");
        buf = sdscatsds(buf, row_head);
        sdsfree(row_head);

        for (kdx = 0; kdx < row->col_count; kdx++) {
            struct namuast_table_cell *cell = &row->cols[kdx];
            sds cell_attr = sdsempty();
            sds cell_style = sdsempty();

            if (cell->rowspan > 1) {
                sds n = sdsfromlonglong(cell->rowspan);
                cell_attr = add_html_attr(cell_attr, "rowspan", n);
                sdsfree(n);
            }
            if (cell->colspan > 1) {
                sds n = sdsfromlonglong(cell->colspan);
                cell_attr = add_html_attr(cell_attr, "colspan", n);
                sdsfree(n);
            }

            cell_attr = add_html_attr(cell_attr, "width", cell->width);
            cell_attr = add_html_attr(cell_attr, "height", cell->height);
            if (cell->align != nm_align_none)
                cell_style = add_html_style(cell_style, "text-align", align_to_str(cell->align));

            if (cell->valign != nm_valign_none)
                cell_style = add_html_style(cell_style, "vertical-align", align_to_str(cell->valign));
            cell_style = add_html_style(cell_style, "background-color", cell->bg_webcolor);

            buf = sdscat(buf, "<td style='");
                buf = sdscat_sanitize_style(buf, cell_style);
                buf = sdscat(buf, "' ");
                buf = sdscatsds(buf, cell_attr);
            buf = sdscat(buf, ">");
                buf = INL_HTML_OP(cell->content, to_html, ctx, buf);
            buf = sdscat(buf, "</td>");

            sdsfree(cell_attr);
            sdsfree(cell_style);
        }
        buf = sdscat(buf, "</tr>");
    }

    buf = sdscat(buf, 
            "</tbody>"
        "</table>"
    "</div>");
    return buf;
}

static sds emit_list_rec(struct namuast_list* li, htmlgen_ctx *ctx, sds buf) {
    struct namuast_list* p;

    char *li_st_tag;
    char *li_ed_tag;

    sds ul_st_tag;
    char* ul_ed_tag;
    if (li->type == nm_list_indent) {
        li_st_tag = "";
        li_ed_tag = "<br>";

        ul_st_tag = sdsnew("<div class='wiki-indent'>");
        ul_ed_tag = "</div>";
    } else if (li->type == nm_list_unordered) {
        li_st_tag = "<li>";
        li_ed_tag = "</li>";

        ul_st_tag = sdsnew("<ul class='wiki-list'>");
        ul_ed_tag = "</ul>";
    } else {
        li_st_tag = "<li>";
        li_ed_tag = "</li>";

        ul_st_tag = sdsnew("<ol class='wiki-list' start='1' style='list-style-type: ");

        char *list_style_type;
        switch (li->type) {
        case nm_list_ordered:
            list_style_type = "decimal"; 
            break;
        case nm_list_upper_alpha:
            list_style_type = "upper-alpha"; 
            break;
        case nm_list_lower_alpha:
            list_style_type = "lower-alpha"; 
            break;
        case nm_list_upper_roman:
            list_style_type = "upper-roman"; 
            break;
        case nm_list_lower_roman:
            list_style_type = "lower-roman"; 
            break;
        default:
            list_style_type = "unknown";
            break;
        }
        ul_st_tag = sdscat(ul_st_tag, list_style_type);
        ul_st_tag = sdscat(ul_st_tag, "'>");

        ul_ed_tag = "</ol>";
    } 

    buf = sdscatsds(buf, ul_st_tag);
    sdsfree(ul_st_tag);
    for (p = li; p; p = p->next) {
        buf = sdscat(buf, li_st_tag);
        buf = INL_HTML_OP(p->content, to_html, ctx, buf);
        buf = sdscat(buf, li_ed_tag);

        if (p->sublist) {
            buf = emit_list_rec(p->sublist, ctx, buf);
        }
    }
    buf = sdscat(buf, ul_ed_tag);
    return buf;
}


static sds list_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    return emit_list_rec((struct namuast_list *)base, ctx, buf);
}

static sds heading_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    struct namuast_heading* hd = (struct namuast_heading*)base;
    buf = sdscatprintf(buf, 
        "<h%d><a class='wiki-heading' href='#toc' id='s-%s'>%s</a>. ", 
        hd->h_num, hd->section_name, hd->section_name);
    buf = INL_HTML_OP(hd->content, to_html, ctx, buf);
    buf = sdscatprintf(buf, "</h%d>", hd->h_num);
    return buf;
}

static sds inline_to_html(namuast_base *base, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inline *inl = (struct namuast_inline *)base;
    return INL_HTML_OP(inl, to_html, ctx, buf);
}

static sds str_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    return sdscat_escape_html_content(buf, ((struct namuast_inl_str *)inl)->str);
}

static sds link_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_link *link = (struct namuast_inl_link *)inl;
    assert (link->_base.inl_type == namuast_inltype_link);
    assert (link->name != NULL);
    bool doesnt_exist = doc_doesnt_exist(ctx, link->name);

    sds raw_href = ctx->doc_itfc->doc_href(ctx->doc_itfc, link->name);
    sds href = sdscat_sanitize_src_href(sdsempty(), raw_href);
    buf = sdscat(buf, "<a class='internal-link ");
    if (doesnt_exist)
        buf = sdscat(buf, " not-exist");
    buf = sdscat(buf, "' href='");
    buf = sdscatsds(buf, href);
    if (link->section) {
        buf = sdscat(buf, "#s-");
        buf = sdscatsds(buf, link->section);
    }

    buf = sdscat(buf, "' data-internal-link-ref='");
    buf = sdscat_escape_html_attr(buf, link->name);
    buf = sdscat(buf, "' data-internal-link-section='");
    if (link->section)
        buf = sdscat_escape_html_attr(buf, link->section);
    buf = sdscat(buf, "'>");

    sdsfree(href);
    sdsfree(raw_href);

    if (link->alias) {
        buf = INL_HTML_OP(link->alias, to_html, ctx, buf);
    } else {
        buf = sdscat_escape_html_content(buf, link->name);
    }
    buf = sdscat(buf, "</a>");
    return buf;
}

static sds extlink_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_extlink *extlink = (struct namuast_inl_extlink *)inl;

    buf = sdscatprintf(buf, "<a href='");
    sds sanitized_link = sdscat_sanitize_src_href(sdsempty(), extlink->href);
    buf = sdscat_escape_html_attr(buf, sanitized_link);
    buf = sdscat(buf, "' class='external-link'>");

    sdsfree(sanitized_link);
    
    if (extlink->alias) {
        buf = INL_HTML_OP(extlink->alias, to_html, ctx, buf);
    } else {
        buf = sdscat_escape_html_content(buf, extlink->href);
    }
    buf = sdscat(buf, "</a>");
    return buf;
}

static sds image_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_image *image = (struct namuast_inl_image *)inl;

    buf = sdscat(buf, "<div class='image-wrapper'>");
    buf = sdscat(buf, "<img src='");
    sds sanitized_link = sdscat_sanitize_src_href(sdsempty(), image->src);
    buf = sdscat_escape_html_attr(buf, sanitized_link);
    buf = sdscat(buf, "'");
    sdsfree(sanitized_link);
    if (image->width) {
        buf = sdscat(buf, " width='");
        buf = sdscat_escape_html_attr(buf, image->width);
        buf = sdscat(buf, "'");
    }
    
    if (image->height) {
        buf = sdscat(buf, " height='");
        buf = sdscat_escape_html_attr(buf, image->height);
        buf = sdscat(buf, "'");
    }
    buf = sdscat(buf, " class='");
    switch (image->align) {
    case nm_align_left:
        buf = sdscat(buf, "align-left");
        break;
    case nm_align_right:
        buf = sdscat(buf, "align-right");
        break;
    case nm_align_center:
        buf = sdscat(buf, "align-center");
        break;
    default:
        break;
    }
    buf = sdscat(buf, "'");
    buf = sdscat(buf, ">");
    buf = sdscat(buf, "</div>");
    return buf;
}

static sds block_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_block *inl_block = (struct namuast_inl_block *)inl;
    switch (inl_block->inl_block_type) {
    case inlblock_type_color:
        buf = sdscat(buf, "<span class='wiki-color' style='color: ");
        buf = sdscat_escape_html_attr(buf, inl_block->data.webcolor);
        buf = sdscat(buf, "'>");
        buf = INL_HTML_OP(inl_block->content, to_html, ctx, buf);
        buf = sdscat(buf, "</span>");
        break;
    case inlblock_type_highlight:
        buf = sdscatprintf(buf, "<span class='wiki-size wiki-%d'>", inl_block->data.highlight_level);
        buf = INL_HTML_OP(inl_block->content, to_html, ctx, buf);
        buf = sdscat(buf, "</span>");
        break;
    case inlblock_type_raw:
        buf = sdscat(buf, "<code>");
        buf = sdscat_escape_html_content(buf, inl_block->data.raw);
        buf = sdscat(buf, "</code>");
        break;
    }
    return buf;
}


static sds fnt_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_fnt *fnt = (struct namuast_inl_fnt *)inl;
    buf = sdscatprintf(buf, "<a class='wiki-fn-link' id='rfn-%d' href='#fn-%d' title='", fnt->id, fnt->id);
    buf = sdscat_escape_html_attr(buf, fnt->raw);
    buf = sdscat(buf, "'>");
    if (fnt->is_named) {
        buf = sdscat(buf, "[");
        buf = sdscat_escape_html_content(buf, fnt->repr.name);
        buf = sdscat(buf, "]");
    } else {
        buf = sdscatprintf(buf, "[%d]", fnt->repr.anon_num);
    }
    buf = sdscat(buf, "</a>");
    return buf;
}

static sds fnt_content_to_html(struct namuast_inl_fnt *fnt, htmlgen_ctx *ctx, sds buf) {
    buf = sdscat(buf, "<li class='footnote-list'>");

    buf = sdscatprintf(buf, "<a class='wiki-fn-content' id='fn-%d' href='#rfn-%d'>", fnt->id, fnt->id);
    if (fnt->is_named) {
        buf = sdscat(buf, "[");
        buf = sdscat_escape_html_content(buf, fnt->repr.name);
        buf = sdscat(buf, "]");
    } else {
        buf = sdscatprintf(buf, "[%d]", fnt->repr.anon_num);
    }
    buf = sdscat(buf, "</a> ");
    buf = INL_HTML_OP(fnt->content, to_html, ctx, buf);
    buf = sdscat(buf, "</li>");

    return buf;
}

static sds fnt_section_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_fnt_section *fnt_section = (struct namuast_inl_fnt_section *)inl;

    struct namuast_container *ast = ctx->ast_being_used;
    struct list *fnt_list = &ast->fnt_list;
    struct list_elem *e;
    if (ctx->last_emitted_fnt) {
        e = list_next(&ctx->last_emitted_fnt->elem);
    } else {
        e = list_begin(fnt_list);
    }

    if (e != list_end(fnt_list)) {
        buf = sdscat(buf, "<ol class='wiki-macro-footnote'>");
        for (; e != list_end(fnt_list); e = list_next(e)) {
            struct namuast_inl_fnt *fnt = list_entry(e, struct namuast_inl_fnt, elem);
            if (fnt->id > fnt_section->cur_footnote_id) {
                break;
            }
            buf = fnt_content_to_html(fnt, ctx, buf);
            ctx->last_emitted_fnt = fnt;
        }
        buf = sdscat(buf, "</ol>");
    }

    return buf;
} 

static sds dfs_toc(struct htmlgen_ctx *ctx, sds buf, struct namuast_heading* hd) {
    if (hd->parent) { 
        // when it is not the root: sentinel node
        buf = sdscat(buf, "<span class='toc-item'>");
        buf = sdscatprintf(buf, "<a class='tocs-section-link' href='#s-%s'>%s</a>.", hd->section_name, hd->section_name);
        buf = INL_HTML_OP(hd->content, to_html, ctx, buf);
        buf = sdscat(buf, "</span>");
    }

    if (!list_empty(&hd->children)) {
        buf = sdscat(buf, "<span class='toc-indent'>");
        struct list_elem *e;
        for (e = list_begin(&hd->children); e != list_end(&hd->children); e = list_next(e)) {
            struct namuast_heading *child = list_entry(e, struct namuast_heading, elem);
            buf = dfs_toc(ctx, buf, child);
        }
        buf = sdscat(buf, "</span>");
    }
    return buf;
}

static sds toc_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_container *ast = ctx->ast_being_used;
    struct namuast_heading *root_heading = ast->root_heading;

    buf = sdscat(buf, "<div class='wiki-macro-toc' id='toc'>");
    buf = dfs_toc(ctx, buf, root_heading);
    buf = sdscat(buf, "</div>");
    return buf;
}

static sds span_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_span* span = (struct namuast_inl_span*)inl;
    enum nm_span_type span_type = span->span_type;
    buf = sdscat(buf, span_type_to_str(span_type, true));
    buf = INL_HTML_OP(span->content, to_html, ctx, buf);
    buf = sdscat(buf, span_type_to_str(span_type, false));
    return buf;
}

static sds return_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    return sdscat(buf, "<br>");
}

static sds container_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    size_t idx, len;
    struct namuast_inl_container *inl_container = (struct namuast_inl_container *)inl;

    for (idx = 0, len = inl_container->len; idx < len; idx++) {
        struct namuast_inline *inl = inl_container->children[idx];
        buf = INL_HTML_OP(inl, to_html, ctx, buf);
    }
    return buf;
}

static sds macro_inl_to_html(namuast_inline *inl, htmlgen_ctx *ctx, sds buf) {
    struct namuast_inl_macro *macro = (struct namuast_inl_macro *)inl;

    bool done = false;

    htmlgen_macro_record *r;
    htmlgen_simple_macro_record *sr;
    // general macro first
    for (r = htmlgen_internal_macros; r->name; r++) {
        if (!strcasecmp(r->name, macro->name)) {
            buf = r->converter(ctx, r, macro, ctx->ast_being_used, buf);
            done = true;
            break;
        }
    }

    if (!done) {
        for (sr = htmlgen_internal_simple_macros; sr->name; sr++) {
            if (!strcasecmp(sr->name, macro->name)) {
                buf = simple_macro_converter(ctx, sr, macro, buf);
                done = true;
                break;
            }
        }
    }

    if (!done) {
        buf = htmlgen_macro_fallback(ctx, macro, buf);
    }

    return buf;
}



/*
 * get_lname operations
 */
static void noop_get_lname(namuast_base *base, varray *arr) { }

static void container_get_lname(namuast_base *base, varray *arr) {
    namuast_container *container = (namuast_container *)base;

    size_t idx;
    for (idx = 0; idx < container->len; idx++) {
        HTML_OP(container->children[idx], get_lname, arr);
    }
}

static void quotation_get_lname(namuast_base *base, varray *arr) {
    struct namuast_quotation *quote = (struct namuast_quotation *)base;
    INL_HTML_OP(quote->content, get_lname, arr);
}

static void table_get_lname(namuast_base *base, varray *arr) {
    struct namuast_table *table = (struct namuast_table *)base;
    if (table->caption) {
        INL_HTML_OP(table->caption, get_lname, arr);
    }
    size_t idx;
    for (idx = 0; idx < table->row_count; idx++) {
        struct namuast_table_row* row = &table->rows[idx];
        size_t jdx;
        for (jdx = 0; jdx < row->col_count; jdx++) {
            struct namuast_table_cell *col = &row->cols[jdx];
            INL_HTML_OP(col->content, get_lname, arr);
        }
    }
}

static void list_get_lname(namuast_base *base, varray *arr) {
    struct namuast_list *list = (struct namuast_list *)base;

    struct namuast_list *p;
    for (p = list; p; p = p->next) {
        INL_HTML_OP(p->content, get_lname, arr);
        if (p->sublist)
            list_get_lname(&p->sublist->_base, arr);
    }
}


static void heading_get_lname(namuast_base *base, varray *arr) {
    struct namuast_heading* heading = (struct namuast_heading *)base;
    INL_HTML_OP(heading->content, get_lname, arr);
}

static void inline_get_lname(namuast_base *base, varray *arr) {
    struct namuast_inline *inl = (struct namuast_inline *)base;
    INL_HTML_OP(inl, get_lname, arr);
}

static void noop_inl_get_lname(namuast_inline *inl, varray *arr) {
}

static void link_inl_get_lname(namuast_inline *inl, varray *arr) {
    struct namuast_inl_link *link = (struct namuast_inl_link *)inl;
    varray_push(arr, link->name);
    if (link->alias)
        INL_HTML_OP(link->alias, get_lname, arr);
}

static void block_inl_get_lname(namuast_inline *inl, varray *arr) {
    struct namuast_inl_block *block = (struct namuast_inl_block *)inl;
    if (block->content) {
        INL_HTML_OP(block->content, get_lname, arr);
    }
}


static void fnt_inl_get_lname(namuast_inline *inl, varray *arr) {
    struct namuast_inl_fnt *fnt = (struct namuast_inl_fnt *)inl;
    INL_HTML_OP(fnt->content, get_lname, arr);
}

static void span_inl_get_lname(namuast_inline *inl, varray *arr) {
    struct namuast_inl_span *span = (struct namuast_inl_span *)inl;
    INL_HTML_OP(span->content, get_lname, arr);
}

static void container_inl_get_lname(namuast_inline *inl, varray *arr) {
    struct namuast_inl_container *container = (struct namuast_inl_container *)inl;
    size_t idx;
    for (idx = 0; idx < container->len; idx++) {
        namuast_inline* subinl = container->children[idx];
        INL_HTML_OP(subinl, get_lname, arr);
    }
}

sds htmlgen_generate(htmlgen_ctx *ctx, namuast_container *ast_container, sds buf) {
    size_t idx;
    varray *link_names = varray_init(); // just borrow all internal links (it doesn't own them)
    HTML_OP(ast_container, get_lname, link_names);

    size_t docname_count = varray_length(link_names);
    bool results[docname_count];
    ctx->doc_itfc->documents_exist(ctx->doc_itfc, docname_count, (char **)link_names->memory, results);

    size_t ne_cnt = 0;
    for (idx = 0; idx < docname_count; idx++) {
        if (!results[idx]) { 
            ne_cnt++;
        }
    }
    
    ctx->ne_docs_count = ne_cnt;

    sds* ne_p;
    ctx->ne_docs = ne_p = calloc(ne_cnt, sizeof(sds));

    for (idx = 0; idx < docname_count; idx++) {
        if (!results[idx]) {
            *ne_p++ = (sds)varray_get(link_names, idx);
        }
    }
    qsort(ctx->ne_docs, ne_cnt, sizeof(sds), _s_sdscmp);
    ctx->ast_being_used = ast_container;
    buf = HTML_OP(ast_container, to_html, ctx, buf);

    free(ctx->ne_docs);
    ctx->ne_docs = NULL;
    ctx->ast_being_used = NULL;
    ctx->ne_docs_count = 0;
    varray_free(link_names, NULL);
    return buf;
}

sds htmlgen_generate_directly(const char *doc_name, struct namugen_doc_itfc *doc_itfc, sds buf, bool *success_out) {
    htmlgen_ctx htmlgen;

    struct namuast_container* ast = doc_itfc->get_ast(doc_itfc, doc_name);
    if (!ast) {
        if (success_out)
            *success_out = false;
        return buf;
    }
    htmlgen_init(&htmlgen, doc_name, doc_itfc);
    buf = htmlgen_generate(&htmlgen, ast, buf);
    htmlgen_remove(&htmlgen);
    RELEASE_NAMUAST(ast);
    if (success_out)
        *success_out = true;
    return buf;
}

sds htmlgen_macro_fallback(htmlgen_ctx *ctx, struct namuast_inl_macro* macro, sds buf) {
    buf = sdscat(buf, "<span class='wiki-macro-fallback'>");
    buf = sdscat_escape_html_content(buf, macro->raw);
    buf = sdscat(buf, "</span>");
    return buf;
}

static sds inclusion_converter(htmlgen_ctx *ctx, htmlgen_macro_record *_, struct namuast_inl_macro *macro, struct namuast_container *cur_ast, sds buf) {
    if (macro->pos_args_len != 1) {
        return htmlgen_macro_fallback(ctx, macro, buf);
    }
    sds doc_name = macro->pos_args[0];
    if (!htmlgen_already_included(ctx, doc_name)) {
        struct namuast_container *ast_to_be_included = ctx->doc_itfc->get_ast(ctx->doc_itfc, doc_name);
        if (!ast_to_be_included) {
            return htmlgen_macro_fallback(ctx, macro, buf);
        }

        htmlgen_ctx sub_ctx;
        htmlgen_includer_info includer_info = {.includer_ctx = ctx};
        htmlgen_init(&sub_ctx, doc_name, ctx->doc_itfc);
        sub_ctx.includer_info = &includer_info;

        buf = htmlgen_generate(&sub_ctx, ast_to_be_included, buf);

        htmlgen_remove(&sub_ctx);
        RELEASE_NAMUAST(ast_to_be_included);
    }
    return buf;
}

static int cmp_kw(const void *lhs, const void *rhs) {
    return sdscmp(*(const sds *)lhs, *(const sds *)rhs);
}

static sds simple_macro_converter(htmlgen_ctx *ctx, htmlgen_simple_macro_record *record, struct namuast_inl_macro *macro, sds buf) {
    const char *source = record->temp;
    const char *border = source + strlen(source);

    const char *p = source;
    const char *flushed_p = source;
    while (p < border) {
        UNTIL_REACHING1(p, border, '{') {
            p++;
        }
        buf = sdscatlen(buf, flushed_p, p - flushed_p);
        flushed_p = p;
        if (p < border)
            p++;
        if (EQ(p, border, '{')) {
            p++;
            const char* c_st = p;
            UNTIL_REACHING1(p, border, '}') {
                p++;
            }
            const char* c_ed = p;
            CONSUME_SPACETAB(c_st, c_ed);
            RCONSUME_SPACETAB(c_st, c_ed);

            if (p < border)
                p++;

            if (EQ(p, border, '}')) {
                char* endptr;
                long index = strtol(c_st, &endptr, 10);
                if (endptr == c_ed) {
                    if (index < macro->pos_args_len) {
                        buf = sdscat_escape_html_content(buf, macro->pos_args[index]);
                    }
                } else {
                    sds key = sdsnewlen(c_st, c_ed - c_st);
                    sds* kv = (sds *)bsearch(&key, macro->kw_args, macro->kw_args_len, sizeof(sds) * 2, cmp_kw);
                    if (kv) {
                        buf = sdscat_escape_html_content(buf, *(kv + 1));
                    }
                    sdsfree(key);
                }
                p++;
                flushed_p = p;
            }
        }
    }
    buf = sdscatlen(buf, flushed_p, border - flushed_p);
    return buf;
}

void initmod_htmlgen() {
    /* to_html operatation */
    namuast_html_ops[namuast_type_container].to_html = container_to_html;
    namuast_html_ops[namuast_type_return].to_html = return_to_html;
    namuast_html_ops[namuast_type_hr].to_html = hr_to_html;
    namuast_html_ops[namuast_type_quotation].to_html = quotation_to_html;
    namuast_html_ops[namuast_type_block].to_html = block_to_html;
    namuast_html_ops[namuast_type_table].to_html = table_to_html;
    namuast_html_ops[namuast_type_list].to_html = list_to_html;
    namuast_html_ops[namuast_type_heading].to_html = heading_to_html;
    namuast_html_ops[namuast_type_inline].to_html = inline_to_html;

    namuast_inl_html_ops[namuast_inltype_str].to_html = str_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_link].to_html = link_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_extlink].to_html = extlink_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_image].to_html = image_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_block].to_html = block_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_fnt].to_html = fnt_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_fnt_section].to_html = fnt_section_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_toc].to_html = toc_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_span].to_html = span_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_return].to_html = return_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_container].to_html = container_inl_to_html;
    namuast_inl_html_ops[namuast_inltype_macro].to_html = macro_inl_to_html;

    /* get_lname operations */
    namuast_html_ops[namuast_type_container].get_lname = container_get_lname;
    namuast_html_ops[namuast_type_return].get_lname = noop_get_lname;
    namuast_html_ops[namuast_type_hr].get_lname = noop_get_lname;
    namuast_html_ops[namuast_type_quotation].get_lname = quotation_get_lname;
    namuast_html_ops[namuast_type_block].get_lname = noop_get_lname;
    namuast_html_ops[namuast_type_table].get_lname = table_get_lname;
    namuast_html_ops[namuast_type_list].get_lname = list_get_lname;
    namuast_html_ops[namuast_type_heading].get_lname = heading_get_lname;
    namuast_html_ops[namuast_type_inline].get_lname = inline_get_lname;

    namuast_inl_html_ops[namuast_inltype_str].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_link].get_lname = link_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_extlink].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_image].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_block].get_lname = block_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_fnt].get_lname = fnt_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_fnt_section].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_toc].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_span].get_lname = span_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_return].get_lname = noop_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_container].get_lname = container_inl_get_lname;
    namuast_inl_html_ops[namuast_inltype_macro].get_lname = noop_inl_get_lname;
}
