#include "namugen.h"


struct heading {
    struct namuast_inline *content;
    sds section_name;

    int seq;

    int h_num;
    struct list_elem elem;

    struct list children;
    struct heading* parent;
};

struct fnt {
    int id;
    bool is_named;
    union {
        sds name;
        int anon_num;
    } repr;
    struct namuast_inline* content;
    struct list_elem elem;
};

struct namuast_inline {
    struct namugen_ctx *ctx;
    sds buf;
};


#include "escaper.inc"

// consume all arugments of type of sds given to this
static sds generate_link(sds buf, bool exist, char *arg_href, char *section, char *title, char *pcontent, char* raw_content, char *extra_class, char *extra_attr) {
    sds escaped_title = escape_html_attr(title);
    sds href = sdsnew(arg_href);

    sds content;
    if (raw_content)
        content = sdsnew(raw_content);
    else
        content = escape_html_content(pcontent);

    if (section) {
        sds val = escape_html_attr(section);
        href = sdscat(href, "#s-");
        href = sdscatsds(href, val);
        sdsfree(val);
    }
    sds link_class = sdsempty();

    if (!exist) {
        link_class = sdscat(link_class, " not-exist");
    }

    if (extra_class) {
        link_class = sdscat(link_class, " ");
        link_class = sdscat(link_class, extra_class);
    }

    if (!extra_attr) {
        extra_attr = "";
    }

    buf = sdscatprintf(buf, "<a class='%s' href='%s' title='%s'%s>%s</a>", link_class, href, escaped_title, extra_attr, content);

    sdsfree(link_class);
    sdsfree(href);
    sdsfree(escaped_title);
    sdsfree(content);
    return buf;
}


static struct fnt* get_footnote_by_id(struct namugen_ctx *ctx, int id) {
    struct list_elem *e;
    struct list *fnt_list = &ctx->fnt_list;
    for (e = list_rbegin (fnt_list); e != list_rend(fnt_list); e = list_prev(e)) {
        struct fnt *fnt = list_entry(e, struct fnt, elem);
        if (fnt->id == id) {
            return fnt;
        }
    }
    return NULL;
}

static struct fnt* make_fnt(struct namugen_ctx* ctx, struct namuast_inline* content, char *extra) {
    int my_footnote_id = ++ctx->last_footnote_id;
    struct fnt *fnt = malloc(sizeof(struct fnt));
    fnt->id = my_footnote_id;
    if (extra) {
        fnt->is_named = true;
        fnt->repr.name = sdsnew(extra);
    } else {
        fnt->is_named = false;
        fnt->repr.anon_num = ++ctx->last_anon_fnt_num;
    }
    fnt->content = content;
    return fnt;

}

static void remove_fnt(struct fnt *fnt) {
    if (fnt->is_named)
        sdsfree(fnt->repr.name);
    namuast_remove_inline(fnt->content);
    free(fnt);
}

static void remove_fnt_list(struct list *fnt_list) {
    while (!list_empty(fnt_list)) {

        struct fnt *fnt = list_entry(list_pop_back(fnt_list), struct fnt, elem);
        remove_fnt(fnt);
    }
}

int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inline* content, char* extra) {
    struct fnt *fnt = make_fnt(ctx, content, extra);
    list_push_back(&ctx->fnt_list, &fnt->elem);
    if (extra)
        free(extra);
    return fnt->id;
}

static void emit_fnt_item(struct namuast_inline* inl, struct fnt *fnt) {
    sds id_def = sdscatprintf(sdsempty(), " id='fn-%d'", fnt->id);
    sds href = sdscatprintf(sdsempty(), "#rfn-%d", fnt->id);

    sds mark;
    if (fnt->is_named) {
        sds val = escape_html_content(fnt->repr.name);
        mark = sdscatprintf(sdsempty(), "[%s]", fnt->repr.name);
        sdsfree(val);
    } else {
        mark = sdscatprintf(sdsempty(), "[%d]", fnt->repr.anon_num);
    }

    sds inl_buf = inl->buf;

    inl_buf = sdscat(inl_buf, "<span class='footnote-list'>");
    inl_buf = generate_link(inl_buf, true, href, NULL, "", NULL, mark, "wiki-fn-content", id_def);
    inl_buf = sdscatsds(inl_buf, fnt->content->buf);
    inl_buf = sdscat(inl_buf, "</span>");

    inl->buf = inl_buf;
    sdsfree(mark);
    sdsfree(id_def);
    sdsfree(href);
}

static void emit_fnt(struct namuast_inline *inl) {
    inl->buf = sdscat(inl->buf, "<div class='wiki-macro-footnote'>");
    struct list* fnt_list = &inl->ctx->fnt_list;
    while (!list_empty(fnt_list)) {
        struct fnt *fnt = list_entry(list_pop_front(fnt_list), struct fnt, elem);
        emit_fnt_item(inl, fnt);
        remove_fnt(fnt);
    }
    inl->buf = sdscat(inl->buf, "</div>");
}

static void emit_toc(struct namuast_inline *inl) {
    // lazy emission
    struct namugen_ctx *ctx = inl->ctx;
    if (ctx->toc_count < MAX_TOC_COUNT) {
        ctx->toc_positions[ctx->toc_count++] = sdslen(ctx->main_buf);
    }
}

static sds dfs_toc(sds buf, struct heading* hd) {
    if (hd->parent) { 
        // when it is not the root: sentinel node
        buf = sdscat(buf, "<span class='toc-item'>");
        buf = sdscatprintf(buf, "<a href='#s-%s'>%s</a>.", hd->section_name, hd->section_name);
        buf = sdscatsds(buf, hd->content->buf);
        buf = sdscat(buf, "</span>");
    }

    if (!list_empty(&hd->children)) {
        buf = sdscat(buf, "<span class='toc-indent'>");
        struct list_elem *e;
        for (e = list_begin(&hd->children); e != list_end(&hd->children); e = list_next(e)) {
            struct heading *child = list_entry(e, struct heading, elem);
            buf = dfs_toc(buf, child);
        }
        buf = sdscat(buf, "</span>");
    }
    return buf;
}

static sds get_toc_sds(struct heading *root) {
    sds buf = sdsnew("<div class='wiki-macro-toc' id='toc'>");
    buf = dfs_toc(buf, root);
    buf = sdscat(buf, "</div>");
    return buf;
}

static void do_emit_toc(struct namugen_ctx* ctx) {
    if (ctx->toc_count > 0) {
        sds toc_buf = get_toc_sds(ctx->root_heading);
        size_t main_buf_len = sdslen(ctx->main_buf);
        size_t total_len = ctx->toc_count * sdslen(toc_buf) + main_buf_len;
        sds new_buf = sdsnewlen(NULL, total_len);
        sdsupdatelen(new_buf);

        int idx;
        size_t prev_position = 0;
        for (idx = 0; idx < ctx->toc_count; idx++) {
            size_t pos = ctx->toc_positions[idx];
            new_buf = sdscatlen(new_buf, ctx->main_buf + prev_position,  pos - prev_position);
            new_buf = sdscatsds(new_buf, toc_buf);
            prev_position = pos;
        }
        new_buf = sdscatlen(new_buf, ctx->main_buf + prev_position, main_buf_len - prev_position);
        sdsfree(ctx->main_buf);
        ctx->main_buf = new_buf;
        sdsfree(toc_buf);
        ctx->toc_count = 0;
    }
}

static struct heading* make_heading(struct heading* parent, struct namuast_inline *content, int h_num);

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inline* content) {
    struct heading* p = ctx->root_heading;
    // fine 'rightmost' node
    while (!list_empty(&p->children)) {
        struct list_elem *e = list_back(&p->children);
        p = list_entry(e, struct heading, elem);
    }

    while (p->parent) {
        if (p->h_num < h_num && (list_empty(&p->children) || h_num <= (list_entry(list_back(&p->children), struct heading, elem))->h_num)) {
            break;
        }
        p = p->parent;
    }
    struct heading* hd = make_heading(p, content, h_num);

    sds section_name = hd->section_name; // borrowed
    ctx->main_buf = sdscatprintf(ctx->main_buf, "<h%d><a class='wiki-heading' href='#toc' id='s-%s'>%s.</a>%s</h%d>", h_num, section_name, section_name, content->buf, h_num);
}

static sds make_section_name(struct heading *hd) {
    struct heading *parent;
    if ((parent = hd->parent)) {
        if (parent->parent) {
            return sdscatprintf(make_section_name(parent), ".%d", hd->seq);
        } else
            return sdsfromlonglong(hd->seq);
    } else
        return sdsempty();
}

static struct heading* make_heading(struct heading* parent, struct namuast_inline *content, int h_num) {
    struct heading* ret = malloc(sizeof(struct heading));
    ret->content = content;
    ret->h_num = h_num;

    list_init(&ret->children);
    if (parent) {
        ret->parent = parent;
        if (list_empty(&parent->children)) {
            ret->seq = 1;
        } else {
            struct heading* last = list_entry(list_back(&parent->children), struct heading, elem);
            ret->seq = last->seq + 1;
        }
        list_push_back(&parent->children, &ret->elem);
    } else
        ret->parent = NULL;
    ret->section_name = make_section_name(ret);
    return ret;
}

static void remove_heading(struct heading* root) {
    if (root->content)
        namuast_remove_inline(root->content);
    sdsfree(root->section_name);
    
    while (!list_empty(&root->children)) {
        struct heading *child = list_entry(list_pop_front(&root->children), struct heading, elem);
        remove_heading(child);
    }
    free(root);
}


static char* align_to_str (enum nm_align_type t) {
    switch (t) {
    case nm_align_none:
    case nm_valign_none:
        return "";
    case nm_align_left:
        return "left";
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

struct namuast_inline* namuast_make_inline(struct namugen_ctx* ctx) {
    struct namuast_inline *inl = malloc(sizeof(struct namuast_inline));
    inl->buf = sdsempty();
    inl->ctx = ctx;
    return inl;
}

void namuast_remove_inline(struct namuast_inline *inl) {
    sdsfree(inl->buf);
    free(inl);
}

// TODO's
struct nm_block_emitters block_emitter_ops_inline;
struct nm_block_emitters block_emitter_ops_paragraphic;

void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx->main_buf = sdscatsds(ctx->main_buf, inl->buf);
    namuast_remove_inline(inl);
}

void nm_emit_return(struct namugen_ctx* ctx) {
    ctx->main_buf = sdscat(ctx->main_buf, "<br>");
}

void nm_emit_hr(struct namugen_ctx* ctx, int hr_num) {
    ctx->main_buf = sdscatprintf(ctx->main_buf, "<hr class=\"wiki-hr-%d\">", hr_num);
}

void nm_begin_footnote(struct namugen_ctx* ctx) {
    ctx->is_in_footnote = true;
}

void nm_end_footnote(struct namugen_ctx* ctx)  {
    ctx->is_in_footnote = false;
}

bool nm_in_footnote(struct namugen_ctx* ctx) {
    return ctx->is_in_footnote;
}


void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx->main_buf = sdscatprintf(ctx->main_buf, "<blockquote class=\"wiki-quote\">%s</blockquote>", inl->buf);
    namuast_remove_inline(inl);
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

void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl) {
    sds main_buf = ctx->main_buf;

    sds tbl_style = sdsempty();
    sds tbl_attr = sdsempty();
    sds tbl_caption = sdsempty();
    sds tbl_class = sdsnew("wiki-table");

    switch (tbl->align) {
    case nm_align_left:
        tbl_class = sdscat(tbl_class, " table-left");
        break;
    case nm_align_right:
        tbl_class = sdscat(tbl_class, " table-right");
        break;
    case nm_align_center:
        tbl_class = sdscat(tbl_class, " table-center");
        break;
    }

    if (tbl->caption) {
        tbl_caption = sdscatprintf(tbl_caption, "<caption>%s</caption>", tbl->caption->buf);
    } 

    tbl_style = add_html_style(tbl_style, "background-color", tbl->bg_webcolor);

    tbl_attr = add_html_attr(tbl_attr, "width", tbl->width);
    tbl_attr = add_html_attr(tbl_attr, "bordercolor", tbl->border_webcolor);
    tbl_attr = add_html_attr(tbl_attr, "height", tbl->height);

    main_buf = sdscatprintf(main_buf, 
    "<div class='wiki-table-wrap'>"
        "%s"
        "<table class='%s' style='%s' %s>"
            "<tbody>", 
    tbl_caption, tbl_class, tbl_style, tbl_attr);

    int idx, kdx;
    for (idx = 0; idx < tbl->row_count; idx++) {
        struct namuast_table_row *row = &tbl->rows[idx];
        sds row_attr = sdsempty();

        if (row->bg_webcolor) {
            row_attr = sdscat(row_attr, " style='background-color:"); 
            sds val = escape_html_attr(row->bg_webcolor);
            row_attr = sdscatsds(row_attr, val); 
            row_attr = sdscat(row_attr, "'"); 
            sdsfree(val);
        }

        main_buf = sdscatprintf(main_buf, "<tr%s>", row_attr);
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
            main_buf = sdscatprintf(main_buf, "<td style='%s'%s>%s</td>", cell_style, cell_attr, cell->content->buf);
            sdsfree(cell_attr);
            sdsfree(cell_style);
        }

        main_buf = sdscat(main_buf, "</tr>");
        sdsfree(row_attr);
    }
    main_buf = sdscat(main_buf, 
            "</tbody>"
        "</table>"
    "</div>");
    ctx->main_buf = main_buf;

    sdsfree(tbl_style);
    sdsfree(tbl_caption);
    sdsfree(tbl_attr);
    sdsfree(tbl_class);

    namuast_remove_table(tbl);
}

static sds emit_list_rec(sds main_buf, struct namuast_list* li) {
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

    main_buf = sdscat(main_buf, ul_st_tag);
    for (p = li; p; p = p->next) {
        main_buf = sdscat(main_buf, li_st_tag);
        main_buf = sdscatsds(main_buf, p->content->buf);
        main_buf = sdscat(main_buf, li_ed_tag);
        if (p->sublist) {
            main_buf = emit_list_rec(main_buf, p->sublist);
        }
    }
    main_buf = sdscat(main_buf, ul_ed_tag);
    if (ul_st_tag)
        sdsfree(ul_st_tag);
    return main_buf;
}


void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li) {
    ctx->main_buf = emit_list_rec(ctx->main_buf, li);
    namuast_remove_list(li);
}

void nm_on_start(struct namugen_ctx* ctx) {
}

void nm_on_finish(struct namugen_ctx* ctx) {
    struct namuast_inline *inl = namuast_make_inline(ctx);
    emit_fnt(inl);
    // no need to remove inl cuz nm_ prefixed functions always deal with it.
    nm_emit_inline(ctx, inl);
    
    do_emit_toc(ctx);
}



static char* span_type_to_str(enum nm_span_type type) { 
    switch (type) {
    case nm_span_bold:
        return "b";
    case nm_span_italic:
        return "i";
    case nm_span_strike:
        return "del";
    case nm_span_underline:
        return "u";
    case nm_span_superscript:
        return "sup";
    case nm_span_subscript:
        return "sub";
    default:
        return "span";
    }
}

void nm_inl_emit_span(struct namuast_inline* inl, struct namuast_inline* span, enum nm_span_type type) {
    if (type != nm_span_none) {
        char *span_str = span_type_to_str(type);
        inl->buf = sdscatprintf(inl->buf, "<%s>", span_str);
        inl->buf = sdscatsds(inl->buf, span->buf);
        inl->buf = sdscatprintf(inl->buf, "</%s>", span_str);
    }
    namuast_remove_inline(span);
}

void nm_inl_emit_char(struct namuast_inline* inl, char c) {
    inl->buf = append_html_content_char(inl->buf, c);
}

void nm_inl_emit_link(struct namuast_inline* inl, char *link, bool compatible_mode, char *alias, char *section) {
    struct namugen_ctx *ctx = inl->ctx;
    struct namugen_hook_itfc* hook_itfc = ctx->namugen_hook_itfc;
    struct namugen_doc_itfc* doc_itfc = ctx->doc_itfc;

    if (!compatible_mode && !alias && !section) {
        if (!strcasecmp(link, "br")) {
            inl->buf = sdscat(inl->buf, "<br>");
            goto cleanup;
        } else if (!strcmp(link, "\xeb\xaa\xa9\xec\xb0\xa8") || !strcasecmp(link, "tableofcontents")) {
            emit_toc(inl);
            goto cleanup;
        } else if (!strcmp(link, "\xea\xb0\x81\xec\xa3\xbc") || !strcasecmp(link, "footnote")) {
            emit_fnt(inl);
            goto cleanup;
        } else {
            char* left_par = strstr(link, "(");
            if (left_par) {
                char *right_par = strstr(left_par + 1, ")");
                if (right_par && *(right_par + 1)) {
                    char *fn, *arg;
                    fn = calloc(left_par - link + 1, sizeof(char));
                    arg = calloc(right_par - left_par - 1 + 1, sizeof(char));
                    memcpy(fn, link, sizeof(char) * (left_par - link));
                    memcpy(arg, left_par + 1, sizeof(char) * (right_par - left_par - 1));

                    bool fn_call_success = hook_itfc->hook_fn_call(hook_itfc, ctx, inl, fn, arg);
                    free(fn);
                    free(arg);
                    if (fn_call_success)
                        goto cleanup;
                }
            }
            if (hook_itfc->hook_fn_link(hook_itfc, ctx, inl, link))
                goto cleanup;
        }
    }
    // main process
    bool exist = doc_itfc->doc_exists(doc_itfc, link);
    sds href = doc_itfc->doc_href(doc_itfc, link); 
    inl->buf = generate_link(inl->buf, exist, href, section, link, alias? alias:link, NULL, "wiki-internal-link", NULL);
    sdsfree(href);
cleanup:
    if (link)
        free(link);
    if (alias)
        free(alias);
    if (section)
        free(section);
}

void nm_inl_emit_upper_link(struct namuast_inline* inl, char *alias, char *section) {
    struct namugen_ctx* ctx = inl->ctx;
    struct namugen_doc_itfc *doc_itfc = ctx->doc_itfc;
    sds cur_doc_name = ctx->cur_doc_name; // borrowed

    char *p;
    char *sep = NULL;
    for (p = cur_doc_name; *p; p++) {
        if (*p == '/')
            sep = p;
    }
    bool exist;
    sds upper_doc_name, href;
    if (sep) {
        upper_doc_name = sdsnewlen(cur_doc_name, sep - cur_doc_name);
        exist = doc_itfc->doc_exists(doc_itfc, upper_doc_name);
        href = doc_itfc->doc_href(doc_itfc, upper_doc_name);
    } else {
        upper_doc_name = NULL;
        exist = false;
        href = NULL;
    }

    inl->buf = generate_link(inl->buf, exist, href? href : "", section, upper_doc_name? upper_doc_name : "../", alias? alias : (upper_doc_name? upper_doc_name : "../"), NULL, "wiki-internal-link", NULL);
    if (href)
        sdsfree(href);
    if (upper_doc_name)
        sdsfree(upper_doc_name);
    if (alias)
        free(alias);
    if (section)
        free(section);
}

void nm_inl_emit_lower_link(struct namuast_inline* inl, char *link, char *alias, char *section) {
    struct namugen_ctx* ctx = inl->ctx;
    struct namugen_doc_itfc* doc_itfc = ctx->doc_itfc;
    char *argv[] = {inl->ctx->cur_doc_name, link};
    sds docname = sdsjoin(argv, 2, "/", 1);

    sds href = doc_itfc->doc_href(doc_itfc, docname); 
    bool exist = doc_itfc->doc_exists(doc_itfc, docname);
    inl->buf = generate_link(inl->buf, exist, href, section, link, alias? alias : link, NULL, "wiki-internal-link", NULL);

    sdsfree(docname);
    sdsfree(href);
    if (link)
        free(link);
    if (alias)
        free(alias);
    if (section)
        free(section);
}

void nm_inl_emit_external_link(struct namuast_inline* inl, char *link, char *alias) {
    inl->buf = generate_link(inl->buf, true, link, NULL, "", alias? alias : link, NULL, "wiki-extenral-link", " target='_blank'");
    if (link)
        free(link);
    if (alias)
        free(alias);
}


void nm_inl_emit_image(struct namuast_inline* inl, char *url, char *width, char *height, int align) {
    sds image = sdsnew("<image");
    sds src = escape_html_attr(url);
    image = add_html_attr(image, "src", src);
    image = add_html_attr(image, "width", width);
    image = add_html_attr(image, "height", height);

    if (align != nm_align_none) {
        char *align_repr = align_to_str(align);
        if (!strcmp(align_repr, "center"))
            align_repr = "middle";
        image = sdscat(image, " align=");
        image = sdscat(image, align_repr);
    }
    image = sdscat(image, ">");
    inl->buf = sdscatsds(inl->buf, image);


    sdsfree(image);
    sdsfree(src);
    if (url)
        free(url);
    if (width)
        free(width);
    if (height)
        free(height);
}

void nm_inl_cat(struct namuast_inline* inl_dest, struct namuast_inline* inl_src, bool insert_br) {
    if (insert_br)
        inl_dest->buf = sdscat(inl_dest->buf, "<br>");
    if (inl_src)
        inl_dest->buf = sdscatsds(inl_dest->buf, inl_src->buf);
}

void nm_inl_emit_footnote_mark(struct namuast_inline* inl, int id, struct namugen_ctx *ctx) {
    struct fnt* fnt = get_footnote_by_id(ctx, id);
    if (!fnt) return;

    sds id_def = sdscatprintf(sdsempty(), "id='rfn-%d'", id);
    sds href = sdscatprintf(sdsempty(), "#fn-%d", id);

    sds mark;
    if (fnt->is_named) {
        sds val = escape_html_content(fnt->repr.name);
        mark = sdscatprintf(sdsempty(), "[%s]", val);
        sdsfree(val);
    } else
        mark = sdscatprintf(sdsempty(), "[%d]", fnt->repr.anon_num);

    inl->buf = generate_link(inl->buf, true, href, NULL, fnt->content->buf, NULL, mark, "wiki-rfn-content", id_def);
    sdsfree(mark);
    sdsfree(id_def);
    sdsfree(href);
}

static bool p_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    ctx->main_buf = sdscatprintf(ctx->main_buf, "<pre>%s</pre>", raw);
    free(raw);
    return true;
}

static bool p_emit_html(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* html) {
    // TODO: we should sanitize html. Unless appropriate sanitizer found, it is inevitable that putting raw html code is dangerous
    free(html);
    return true;
}

static bool i_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    outer_inl->buf = sdscatprintf(outer_inl->buf, "<code>%s</code>", raw);
    free(raw);
    return true;
}

static bool i_emit_highlighted_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, int level) {
    outer_inl->buf = sdscatprintf(outer_inl->buf, "<span class='wiki-size size-%d'>%s</span>", level, content->buf);
    namuast_remove_inline(content);
    return true;
}

static bool i_emit_colored_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, char* webcolor) {
    sds escaped_color = escape_html_attr(webcolor);
    outer_inl->buf = sdscatprintf(outer_inl->buf, "<span class='wiki-color' style='color: %s'>%s</span>", escaped_color, content->buf);
    namuast_remove_inline(content);
    sdsfree(escaped_color);
    free(webcolor);
    return true;
}


struct nm_block_emitters block_emitter_ops_inline = {
    .emit_raw = i_emit_raw,
    .emit_html = NULL,
    .emit_highlighted_block = i_emit_highlighted_block,
    .emit_colored_block = i_emit_colored_block,
};
struct nm_block_emitters block_emitter_ops_paragraphic = {
    .emit_raw = p_emit_raw,
    .emit_html = p_emit_html,
    .emit_highlighted_block = NULL,
    .emit_colored_block = NULL,
};


struct namugen_ctx* namugen_make_ctx(char* cur_doc_name, struct namugen_doc_itfc* doc_itfc, struct namugen_hook_itfc* namugen_hook_itfc) {
    struct namugen_ctx *ctx = malloc(sizeof(struct namugen_ctx));
    ctx->doc_itfc = doc_itfc;
    ctx->namugen_hook_itfc = namugen_hook_itfc;
    ctx->is_in_footnote = false;
    ctx->last_footnote_id = 0;
    ctx->last_anon_fnt_num = 0;
    list_init(&ctx->fnt_list);
    ctx->root_heading = make_heading(NULL, NULL, 0);
    ctx->last_anon_fnt_num = 0;
    ctx->toc_count = 0;
    ctx->cur_doc_name = sdsnew(cur_doc_name);

    ctx->main_buf = sdsnewlen(NULL, INITIAL_MAIN_BUF);
    sdsupdatelen(ctx->main_buf);
    return ctx;
}

sds namugen_ctx_flush_main_buf(struct namugen_ctx* ctx) {
    sds ret = ctx->main_buf;
    ctx->main_buf = NULL;
    return ret;
}

void namugen_remove_ctx(struct namugen_ctx* ctx) {
    if (ctx->main_buf)
        sdsfree(ctx->main_buf);
    remove_fnt_list(&ctx->fnt_list);
    remove_heading(ctx->root_heading);
    sdsfree(ctx->cur_doc_name);
    free(ctx);
}
