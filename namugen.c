#include <stdio.h>
#include <assert.h>
#include "namugen.h"
#include "escaper.inc"
#define SAFELY_SDS_FREE(expr) if (expr) sdsfree(expr); expr = NULL;

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
    struct list chunk_list;

    sds fast_buf;
};

struct _internal_link_slot {
    struct list_elem elem;

    sds href;
    sds section;
    sds link;
    sds alias;

    struct sdschunk *chunk;
};

static struct _internal_link_slot* _make_internal_link(sds href, sds section, sds link, sds alias, struct sdschunk *chunk) {
    struct _internal_link_slot *slot = malloc(sizeof(struct _internal_link_slot));
    slot->href = href;
    slot->section = section;
    slot->link = link;
    slot->alias = alias;
    slot->chunk = chunk;
    return slot;
}

static void _remove_internal_link(struct _internal_link_slot *slot) {
    SAFELY_SDS_FREE(slot->href);
    SAFELY_SDS_FREE(slot->section);
    SAFELY_SDS_FREE(slot->link);
    SAFELY_SDS_FREE(slot->alias);
    free(slot);
}


static struct sdschunk* make_lazy_sdschunk() {
    struct sdschunk* chk = malloc(sizeof(struct sdschunk));
    chk->buf = NULL;
    chk->is_lazy = true;
    return chk;
}

// steal buf
static void fill_lazy_sdschunk(struct sdschunk* chk, sds buf) {
    assert(chk->is_lazy);
    assert(chk->buf == NULL);
    chk->buf = buf;
    chk->is_lazy = false;
}

static struct sdschunk* make_sdschunk_steal(sds buf) {
    struct sdschunk* chk = malloc(sizeof(struct sdschunk));
    chk->buf = buf;
    chk->is_lazy = false;
    return chk;
}

static void remove_sdschunk(struct sdschunk* chk) {
    if (chk->buf)
        sdsfree(chk->buf);
    free(chk);
}

static void remove_chunk_list(struct list *chunk_list) {
    while (!list_empty(chunk_list)) {
        struct sdschunk *chk = list_entry(list_pop_back(chunk_list), struct sdschunk, elem);
        remove_sdschunk(chk);
    }
}

static inline void flush_fast_buf(sds *p_fast_buf, struct list* chunk_list) {
    sds fast_buf = *p_fast_buf;
    if (sdslen(fast_buf) > 0) {
        bool attached = false;
        if (!list_empty(chunk_list)) { 
            struct sdschunk *back = list_entry(list_back(chunk_list), struct sdschunk, elem);
            if (!back->is_lazy) {
                back->buf = sdscatsds(back->buf, fast_buf);
                sdsclear(fast_buf);
                attached = true;
            }
        }
        if (!attached) {
            struct sdschunk *fast_buf_chunk = make_sdschunk_steal(fast_buf);
            list_push_back(chunk_list, &fast_buf_chunk->elem);
            *p_fast_buf = sdsempty();
        }
    }
}

static inline void inl_flush_fast_buf(struct namuast_inline *dest) {
    flush_fast_buf(&dest->fast_buf, &dest->chunk_list);
}

static inline void ctx_flush_main_fast_buf(struct namugen_ctx* ctx) {
    flush_fast_buf(&ctx->main_fast_buf, &ctx->main_chunk_list);
}


static inline struct sdschunk* inl_append_lazy_chunk(struct namuast_inline *inl) {
    inl_flush_fast_buf(inl);
    struct sdschunk* chk = make_lazy_sdschunk();
    list_push_back(&inl->chunk_list, &chk->elem);
    return chk;

}

static inline void inl_append(struct namuast_inline *inl, char* s) {
    inl->fast_buf = sdscat(inl->fast_buf, s);

}

static inline void inl_append_steal(struct namuast_inline *inl, sds s) {
    inl->fast_buf = sdscatsds(inl->fast_buf, s);
    sdsfree(s);
}

static inline void inl_append_html_char_to_last(struct namuast_inline* inl, char c) {
    inl->fast_buf = append_html_content_char(inl->fast_buf, c);

}

static inline void ctx_append(struct namugen_ctx* ctx, char* s) {
    ctx->main_fast_buf = sdscat(ctx->main_fast_buf, s);

}

static inline void ctx_append_steal(struct namugen_ctx* ctx, sds s) {
    ctx->main_fast_buf = sdscatsds(ctx->main_fast_buf, s);
    sdsfree(s);
}

static inline void merge_chunk_list(struct list* dest_chunk_list, struct list* src_chunk_list) {
    struct sdschunk* dest_back;
    if (list_empty(dest_chunk_list)) {
        dest_back = NULL;
    } else {
        dest_back = list_entry(list_back(dest_chunk_list), struct sdschunk, elem);
    }
    bool back_is_lazy = dest_back == NULL || dest_back->is_lazy;
    while (!list_empty(src_chunk_list)) {
        struct sdschunk *src_chk = list_entry(list_pop_front(src_chunk_list), struct sdschunk, elem);
        if (!back_is_lazy && !src_chk->is_lazy) {
            dest_back->buf = sdscatsds(dest_back->buf, src_chk->buf);
            remove_sdschunk(src_chk);
        } else {
            list_push_back(dest_chunk_list, &src_chk->elem);
            dest_back = src_chk;
            back_is_lazy = src_chk->is_lazy;
        }
    }
}

static inline void inl_move_chunks(struct namuast_inline *dest, struct namuast_inline *src) {
    inl_flush_fast_buf(dest);
    inl_flush_fast_buf(src);
    merge_chunk_list(&dest->chunk_list, &src->chunk_list);
}

static inline void ctx_steal_chunks_from_inline(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx_flush_main_fast_buf(ctx);
    inl_flush_fast_buf(inl);
    merge_chunk_list(&ctx->main_chunk_list, &inl->chunk_list);
}


static sds sdscat_chunk_list(sds dest, struct list* chunk_list) {
    struct list_elem* e, *end;
    size_t overall_size = 0;
    for (e = list_begin(chunk_list); e != list_end(chunk_list); e = list_next(e)) {
        struct sdschunk* chk = list_entry (e, struct sdschunk, elem);
        if (chk->buf)
            overall_size += sdslen(chk->buf);
    }
    dest = sdsMakeRoomFor(dest, overall_size);
    char *p = dest + sdslen(dest);
    end = list_end(chunk_list);
    for (e = list_begin(chunk_list); e != end; e = list_next(e)) {
        struct sdschunk* chk = list_entry (e, struct sdschunk, elem);
        if (chk->buf) {
            size_t chk_buf_len = sdslen(chk->buf);
            memcpy(p, chk->buf, sizeof(char) * chk_buf_len);
            p += chk_buf_len;
        }
    }
    sdsIncrLen(dest, overall_size);
    return dest;
}
static sds sdscat_inline(sds buf, struct namuast_inline* inl) {
    buf = sdscat_chunk_list(buf, &inl->chunk_list);
    buf = sdscatsds(buf, inl->fast_buf);
    return buf;
}


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

/*
 * Side-effect: getting rid of fnt
 */
static inline void inl_steal_fnt_item(struct namuast_inline* inl, struct fnt *fnt) {
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

    inl_append(inl, "<span class='footnote-list'>");
    inl_append_steal(inl, generate_link(sdsempty(), true, href, NULL, "", NULL, mark, "wiki-fn-content", id_def));
    inl_move_chunks(inl, fnt->content);
    inl_append(inl, "</span>");

    sdsfree(mark);
    sdsfree(id_def);
    sdsfree(href);
    remove_fnt(fnt);
}

static void emit_fnt(struct namuast_inline *inl) {
    inl_append(inl, "<div class='wiki-macro-footnote'>");
    struct list* fnt_list = &inl->ctx->fnt_list;
    while (!list_empty(fnt_list)) {
        struct fnt *fnt = list_entry(list_pop_front(fnt_list), struct fnt, elem);
        inl_steal_fnt_item(inl, fnt);
        // `inl` stole `fnt`
    }
    inl_append(inl, "</div>");
}

static void emit_toc(struct namuast_inline *inl) {
    // lazy emission
    struct namugen_ctx *ctx = inl->ctx;
    struct sdschunk* chk = inl_append_lazy_chunk(inl);

    if (ctx->toc_count < MAX_TOC_COUNT) {
        ctx->toc_positions[ctx->toc_count++] = chk;
    } else
        remove_sdschunk(chk);
}

static sds dfs_toc(sds buf, struct heading* hd) {
    if (hd->parent) { 
        // when it is not the root: sentinel node
        buf = sdscat(buf, "<span class='toc-item'>");
        buf = sdscatprintf(buf, "<a href='#s-%s'>%s</a>.", hd->section_name, hd->section_name);
        buf = sdscat_inline(buf, hd->content);
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
        int idx;
        for (idx = 0; idx < ctx->toc_count; idx++) {
            struct sdschunk* chk = ctx->toc_positions[idx];
            fill_lazy_sdschunk(chk, sdsdup(toc_buf));
        }
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

    ctx_append_steal(ctx,
            sdscatprintf(sdsempty(), 
                "<h%d><a class='wiki-heading' href='#toc' id='s-%s'>%s</a>. ", 
                h_num, section_name, section_name));

    // Here, I don't own `content` object. Rather, heading container owns it. So I cannot steal content from it.
    // Therefore, I should stringify it and use its string.
    ctx_append_steal(ctx, sdscat_inline(sdsempty(), content));
    ctx_append_steal(ctx, sdscatprintf(sdsempty(), "</h%d>", h_num));
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
    inl->ctx = ctx;
    inl->fast_buf = sdsempty();
    list_init(&inl->chunk_list);
    return inl;
}

void namuast_remove_inline(struct namuast_inline *inl) {
    struct list* chunk_list = &inl->chunk_list;
    while (!list_empty(chunk_list)) {
        struct sdschunk* chunk = list_entry(list_pop_back(chunk_list), struct sdschunk, elem);
        remove_sdschunk(chunk);
    }
    sdsfree(inl->fast_buf);
    free(inl);
}

// TODO's
struct nm_block_emitters block_emitter_ops_inline;
struct nm_block_emitters block_emitter_ops_paragraphic;

void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx_steal_chunks_from_inline(ctx, inl);
    namuast_remove_inline(inl);
}

void nm_emit_return(struct namugen_ctx* ctx) {
    ctx_append(ctx, "<br>");
}

void nm_emit_hr(struct namugen_ctx* ctx, int hr_num) {
    ctx_append_steal(ctx, sdscatprintf(sdsempty(), "<hr class=\"wiki-hr-%d\">", hr_num));
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
    ctx_append(ctx, "<blockquote class=\"wiki-quote\">");
    ctx_steal_chunks_from_inline(ctx, inl);
    ctx_append(ctx, "</blockquote>");
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
        ctx_append(ctx, "<caption>");
        ctx_steal_chunks_from_inline(ctx, tbl->caption);
        ctx_append(ctx, "</caption>");
    } 

    tbl_style = add_html_style(tbl_style, "background-color", tbl->bg_webcolor);

    tbl_attr = add_html_attr(tbl_attr, "width", tbl->width);
    tbl_attr = add_html_attr(tbl_attr, "bordercolor", tbl->border_webcolor);
    tbl_attr = add_html_attr(tbl_attr, "height", tbl->height);

    ctx_append(ctx, "<div class='wiki-table-wrap'>");
        ctx_append_steal(ctx, tbl_caption);
        ctx_append(ctx, "<table class='");
            ctx_append_steal(ctx, tbl_class);
        ctx_append(ctx, "' style='");
            ctx_append_steal(ctx, tbl_style);
        ctx_append(ctx, "' ");
            ctx_append_steal(ctx, tbl_attr);
        ctx_append(ctx, ">");

        ctx_append(ctx, "<tbody>");

    int idx, kdx;
    for (idx = 0; idx < tbl->row_count; idx++) {
        struct namuast_table_row *row = &tbl->rows[idx];
        sds row_head = sdsnew("<tr");

        if (row->bg_webcolor) {
            row_head = sdscat(row_head, " style='background-color:"); 
            sds val = escape_html_attr(row->bg_webcolor);
            row_head = sdscatsds(row_head, val); 
            row_head = sdscat(row_head, "'"); 
            sdsfree(val);
        }
        row_head = sdscat(row_head, ">");
        ctx_append_steal(ctx, row_head);
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

            ctx_append(ctx, "<td style='");
                ctx_append_steal(ctx, cell_style);
                ctx_append(ctx, "' ");
                ctx_append_steal(ctx, cell_attr);
            ctx_append(ctx, ">");
                ctx_steal_chunks_from_inline(ctx, cell->content);
            ctx_append(ctx, "</td>");
        }
        ctx_append(ctx, "</tr>");
    }

    ctx_append(ctx, 
            "</tbody>"
        "</table>"
    "</div>");

    namuast_remove_table(tbl);
}

/*
 * Side-effects:
 *   Note that `ctx` steals all content of `li` after a call for the sake of efficiency.
 */
static void emit_list_rec(struct namugen_ctx *ctx, struct namuast_list* li) {
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

    // Don't free ul_st_tag. It's already stolen by ctx
    ctx_append_steal(ctx, ul_st_tag);
    for (p = li; p; p = p->next) {
        ctx_append(ctx, li_st_tag);
        ctx_steal_chunks_from_inline(ctx, p->content);
        ctx_append(ctx, li_ed_tag);

        if (p->sublist) {
            emit_list_rec(ctx, p->sublist);
        }
    }
    ctx_append(ctx, ul_ed_tag);
}


void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li) {
    emit_list_rec(ctx, li);
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

    // Now connect internal links altogether
    int argc = ctx->internal_link_count;
    char *docnames[argc];
    struct _internal_link_slot *slots[argc];
    bool existencies[argc];

    int idx = 0;
    struct list_elem* e;
    struct list* internal_link_list = &ctx->internal_link_list;
    for (e = list_begin(internal_link_list); e != list_end(internal_link_list); e = list_next(e)) {
        struct _internal_link_slot *slot = list_entry(e, struct _internal_link_slot, elem);
        slots[idx] = slot;
        docnames[idx] = slot->link;
        idx++;
    }
    ctx->doc_itfc->documents_exist(ctx->doc_itfc, argc, docnames, existencies);

    idx = 0;
    while (!list_empty(internal_link_list)) {
        struct _internal_link_slot *slot = list_entry(
                list_pop_front(internal_link_list), 
                struct _internal_link_slot, 
                elem
        );
        fill_lazy_sdschunk(slot->chunk, 
                generate_link(sdsempty(), 
                    existencies[idx], 
                    slot->href, 
                    slot->section, 
                    slot->link, 
                    slot->alias? slot->alias:slot->link, 
                    NULL, 
                    "wiki-internal-link", 
                    NULL)
        );
        _remove_internal_link(slot);
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

void nm_inl_emit_span(struct namuast_inline* inl, struct namuast_inline* span, enum nm_span_type type) {
    if (type != nm_span_none) {
        inl_append(inl, span_type_to_str(type, true));
        inl_move_chunks(inl, span);
        inl_append(inl, span_type_to_str(type, false));
    }
    namuast_remove_inline(span);
}

void nm_inl_emit_char(struct namuast_inline* inl, char c) {
    // TODO: I need more efficient implementation
    inl_append_html_char_to_last(inl, c);
}

void nm_inl_emit_link(struct namuast_inline* inl, char *link, bool compatible_mode, char *alias, char *section) {
    struct namugen_ctx *ctx = inl->ctx;
    struct namugen_hook_itfc* hook_itfc = ctx->namugen_hook_itfc;
    struct namugen_doc_itfc* doc_itfc = ctx->doc_itfc;

    if (!compatible_mode && !alias && !section) {
        if (!strcasecmp(link, "br")) {
            inl_append(inl, "<br>");
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
    sds href = doc_itfc->doc_href(doc_itfc, link); 

    sds sds_section = NULL;
    sds sds_link = NULL;
    sds sds_alias = NULL;
    if (section) sds_section = sdsnew(section);
    if (link) sds_link = sdsnew(link);
    if (sds_alias) sds_alias = sdsnew(alias);

    struct _internal_link_slot *slot = _make_internal_link(href, sds_section, sds_link, sds_alias, inl_append_lazy_chunk(inl));
    list_push_back(&ctx->internal_link_list, &slot->elem);
    ctx->internal_link_count++;
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
        doc_itfc->documents_exist(doc_itfc, 1, &upper_doc_name, &exist);
        href = doc_itfc->doc_href(doc_itfc, upper_doc_name);
    } else {
        upper_doc_name = NULL;
        exist = false;
        href = NULL;
    }

    inl_append_steal(inl, generate_link(sdsempty(), exist, href? href : "", section, upper_doc_name? upper_doc_name : "../", alias? alias : (upper_doc_name? upper_doc_name : "../"), NULL, "wiki-internal-link", NULL));
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
    bool exist;
    doc_itfc->documents_exist(doc_itfc, 1, &docname, &exist);
    inl_append_steal(inl, generate_link(sdsempty(), exist, href, section, link, alias? alias : link, NULL, "wiki-internal-link", NULL));

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
    inl_append_steal(inl, generate_link(sdsempty(), true, link, NULL, "", alias? alias : link, NULL, "wiki-extenral-link", " target='_blank'"));
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
    inl_append_steal(inl, image);

    sdsfree(src);
    if (url)
        free(url);
    if (width)
        free(width);
    if (height)
        free(height);
}

// This function steal the ownership of inl_src
void nm_inl_cat(struct namuast_inline* inl_dest, struct namuast_inline* inl_src, bool insert_br) {
    if (insert_br) {
        inl_append(inl_dest, "<br>");
    }
    if (inl_src) {
        inl_move_chunks(inl_dest, inl_src);
        namuast_remove_inline(inl_src);
    }
}

// borrow fnt_literal
void nm_inl_emit_footnote_mark(struct namuast_inline* inl, int id, char* fnt_literal, size_t len) {
    struct fnt* fnt = get_footnote_by_id(inl->ctx, id);
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

    sds literal = sdsnewlen(fnt_literal, len);
    inl_append_steal(inl, generate_link(sdsempty(), true, href, NULL, literal, NULL, mark, "wiki-rfn-content", id_def));

    sdsfree(mark);
    sdsfree(literal);
    sdsfree(id_def);
    sdsfree(href);
}

static bool p_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    ctx_append(ctx, "<pre>");
    ctx_append_steal(ctx, escape_html_content(raw));
    ctx_append(ctx, "</pre>");
    free(raw);
    return true;
}

static bool p_emit_html(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* html) {
    // TODO: we should sanitize html. Unless appropriate sanitizer found, it is inevitable that putting raw html code is dangerous
    free(html);
    return true;
}

static bool i_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    inl_append(outer_inl, "<code>");
    inl_append_steal(outer_inl, escape_html_content(raw));
    inl_append(outer_inl, "</code>");
    free(raw);
    return true;
}

static bool i_emit_highlighted_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, int level) {
    char numstr[8];
    inl_append(outer_inl, "<span class='wiki-size size-");
    snprintf(numstr, 7, "%d", level);
    inl_append(outer_inl, numstr);
    inl_append(outer_inl, "'>");
    inl_move_chunks(outer_inl, content);
    inl_append(outer_inl, "</span>");
    namuast_remove_inline(content);
    return true;
}

static bool i_emit_colored_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, char* webcolor) {
    sds escaped_color = escape_html_attr(webcolor);

    inl_append(outer_inl, "<span class='wiki-color' style='color: ");
    inl_append(outer_inl, "<span class='wiki-color' style='color: ");
    inl_append_steal(outer_inl, escaped_color);
    inl_append(outer_inl, "'>");
    inl_move_chunks(outer_inl, content);
    inl_append(outer_inl, "</span>");
    namuast_remove_inline(content);

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
    list_init(&ctx->main_chunk_list);
    list_init(&ctx->internal_link_list);
    ctx->internal_link_count = 0;
    ctx->root_heading = make_heading(NULL, NULL, 0);
    ctx->last_anon_fnt_num = 0;
    ctx->toc_count = 0;
    ctx->main_fast_buf = sdsempty();
    ctx->cur_doc_name = sdsnew(cur_doc_name);

    return ctx;
}

sds namugen_ctx_flush_main_buf(struct namugen_ctx* ctx) {
    struct list_elem *e;
    sds ret = sdscat_chunk_list(sdsempty(), &ctx->main_chunk_list);
    ret = sdscatsds(ret, ctx->main_fast_buf);
    remove_chunk_list(&ctx->main_chunk_list);
    sdsclear(ctx->main_fast_buf);
    return ret;
}


void namugen_remove_ctx(struct namugen_ctx* ctx) {
    remove_chunk_list(&ctx->main_chunk_list);
    remove_fnt_list(&ctx->fnt_list);
    remove_heading(ctx->root_heading);
    while (!list_empty(&ctx->internal_link_list)) {
        _remove_internal_link(list_entry(list_pop_back(&ctx->internal_link_list), struct _internal_link_slot, elem));
    }
    sdsfree(ctx->cur_doc_name);
    sdsfree(ctx->main_fast_buf);
    free(ctx);
}
