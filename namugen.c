#include <stdio.h>
#include <assert.h>
#include "namugen.h"
#include "escaper.inc"
#define SAFELY_SDS_FREE(expr) if (expr) sdsfree(expr); expr = NULL;

#define INLINE_POOL_SIZE 256

struct namuast_hr* namuast_hr_shared;
struct namuast_return* namuast_return_shared;
struct namuast_inl_toc *namuast_inl_toc_shared;

/*
 * Constructor definitions
 */

namuast_base* _init_namuast_base(namuast_base * base, int type) {
    base->ast_type = type;
    base->refcount = 1;
    return base;
}

namuast_inline* _init_namuast_inl_base(namuast_inline *inl, int type) {
    inl->inl_type = type;
    return inl;
}

struct namuast_inl_container* make_inl_container() {
    struct namuast_inl_container* ret = (struct namuast_inl_container*) NEW_INL_NAMUAST(namuast_inltype_container);
    ret->len = 0;
    ret->capacity = 16;
    ret->children = calloc(ret->capacity, sizeof(namuast_inline *));
    return ret;
}

void inl_container_add_steal(struct namuast_inl_container* container, namuast_inline *src) {
    if (container->len >= container->capacity) {
        container->capacity *= 2;
        container->children = realloc(container->children, sizeof(namuast_inline*) * container->capacity);
    }
    container->children[container->len++] = src;
}

void inl_container_add_return(struct namuast_inl_container* container, struct namugen_ctx *ctx) {
    OBTAIN_NAMUAST(ctx->shared_inl_return);
    inl_container_add_steal(container, &ctx->shared_inl_return->_base);
}


static struct namuast_inl_fnt* make_fnt(struct namugen_ctx* ctx, struct namuast_inl_container* content, bndstr extra) {
    int my_footnote_id = ++ctx->last_footnote_id;
    struct namuast_inl_fnt *fnt = (struct namuast_inl_fnt *)NEW_INL_NAMUAST(namuast_inltype_fnt);
    fnt->id = my_footnote_id;
    if (extra.len > 0) {
        fnt->is_named = true;
        fnt->repr.name = sdsnewlen(extra.str, extra.len);
    } else {
        fnt->is_named = false;
        fnt->repr.anon_num = ++ctx->last_anon_fnt_num;
    }
    fnt->content = content;
    fnt->raw = NULL;
    return fnt;
}

static void emit_fnt_section(namuast_inl_container *container, struct namugen_ctx *ctx) {
    struct namuast_inl_fnt_section *fnt_section = (struct namuast_inl_fnt_section *)NEW_INL_NAMUAST(namuast_inltype_fnt_section);
    fnt_section->cur_footnote_id = ctx->last_footnote_id;
    inl_container_add_steal(container, &fnt_section->_base);
}


static struct namuast_inl_fnt* get_footnote_by_id(struct namuast_container *ast_container, int id) {
    struct list_elem *e;
    struct list *fnt_list = &ast_container->fnt_list;
    for (e = list_rbegin (fnt_list); e != list_rend(fnt_list); e = list_prev(e)) {
        struct namuast_inl_fnt *fnt = list_entry(e, struct namuast_inl_fnt, elem);
        if (fnt->id == id) {
            return fnt;
        }
    }
    return NULL;
}

static void namuast_container_add_steal(struct namuast_container* container, namuast_base *src) {
    if (container->len >= container->capacity) {
        container->capacity *= 2;
        container->children = realloc(container->children, sizeof (struct namuast_base*) * container->capacity);
    }
    container->children[container->len++] = src;
}

int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inl_container* content, bndstr extra) {
    struct namuast_inl_fnt *fnt = make_fnt(ctx, content, extra);
    struct namuast_container *ast_container = ctx->result_container;
    list_push_back(&ast_container->fnt_list, &fnt->elem);
    return fnt->id;
}

static struct namuast_heading* make_heading(struct namuast_heading* parent, struct namuast_inl_container *content, int h_num);

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inl_container * content) {
    struct namuast_heading* p = ctx->result_container->root_heading;
    // fine 'rightmost' node
    while (!list_empty(&p->children)) {
        struct list_elem *e = list_back(&p->children);
        p = list_entry(e, struct namuast_heading, elem);
    }

    while (p->parent) {
        if (p->h_num < h_num && (list_empty(&p->children) || h_num <= (list_entry(list_back(&p->children), struct namuast_heading, elem))->h_num)) {
            break;
        }
        p = p->parent;
    }
    struct namuast_heading* hd = make_heading(p, content, h_num); // p owns hd automatically

    OBTAIN_NAMUAST(hd);
    namuast_container_add_steal(ctx->result_container, &hd->_base);
}

static sds make_section_name(struct namuast_heading *hd) {
    struct namuast_heading *parent;
    if ((parent = hd->parent)) {
        if (parent->parent) {
            return sdscatprintf(make_section_name(parent), ".%d", hd->seq);
        } else
            return sdsfromlonglong(hd->seq);
    } else
        return sdsempty();
}

static struct namuast_heading* make_heading(struct namuast_heading* parent, struct namuast_inl_container *content, int h_num) {
    struct namuast_heading* ret = (struct namuast_heading *)NEW_NAMUAST(namuast_type_heading);
    ret->content = content;
    ret->h_num = h_num;

    list_init(&ret->children);
    if (parent) {
        ret->parent = parent;
        if (list_empty(&parent->children)) {
            ret->seq = 1;
        } else {
            struct namuast_heading* last = list_entry(list_back(&parent->children), struct namuast_heading, elem);
            ret->seq = last->seq + 1;
        }
        list_push_back(&parent->children, &ret->elem);
    } else
        ret->parent = NULL;
    ret->section_name = make_section_name(ret);
    return ret;
}

struct nm_block_emitters block_emitter_ops_inline;
struct nm_block_emitters block_emitter_ops_paragraphic;

void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inl_container* inl) {
    namuast_container_add_steal(ctx->result_container, &inl->_base._base);
}

void nm_emit_return(struct namugen_ctx* ctx) {
    OBTAIN_NAMUAST(ctx->shared_return);
    namuast_container_add_steal(ctx->result_container, &ctx->shared_return->_base);
}

void nm_emit_hr(struct namugen_ctx* ctx, int hr_num) {
    OBTAIN_NAMUAST(ctx->shared_hr);
    namuast_container_add_steal(ctx->result_container, &ctx->shared_hr->_base);
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


void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inl_container* inl) {
    struct namuast_quotation *quote = (struct namuast_quotation *)NEW_NAMUAST(namuast_type_quotation);
    quote->content = inl;
    namuast_container_add_steal(ctx->result_container, &quote->_base);
}

void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl) {
    namuast_container_add_steal(ctx->result_container, &tbl->_base);
}

void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li) {
    namuast_container_add_steal(ctx->result_container, &li->_base);
}

void nm_on_start(struct namugen_ctx* ctx) {
}

void nm_on_finish(struct namugen_ctx* ctx) {
    // emit footnote section at the end of article always
    namuast_inl_container *container = make_inl_container();
    emit_fnt_section(container, ctx);
    nm_emit_inline(ctx, container);

}

void nm_inl_emit_span(struct namuast_inl_container* container, struct namuast_inl_container* content, enum nm_span_type type) {
    struct namuast_inl_span *span = (struct namuast_inl_span *)NEW_INL_NAMUAST(namuast_inltype_span);
    span->span_type = type;
    span->content = content;
    inl_container_add_steal(container, &span->_base);
}

void nm_inl_emit_str(struct namuast_inl_container* container, bndstr s) {
    if (container->len > 0 && container->children[container->len - 1]->inl_type == namuast_inltype_str) {
        struct namuast_inl_str *prev_inlstr = (struct namuast_inl_str *)container->children[container->len - 1];
        prev_inlstr->str = sdscatlen(prev_inlstr->str, s.str, s.len);
    } else {
        struct namuast_inl_str *inlstr = (struct namuast_inl_str *)NEW_INL_NAMUAST(namuast_inltype_str);
        inlstr->str = sdsnewlen(s.str, s.len);
        inl_container_add_steal(container, &inlstr->_base);
    }
}

static void emit_toc(namuast_inl_container *container, struct namugen_ctx *ctx) {
    OBTAIN_NAMUAST(ctx->shared_toc);
    inl_container_add_steal(container, &ctx->shared_toc->_base);
}


void nm_inl_emit_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, bndstr link, bool compatible_mode, struct namuast_inl_container *alias, bndstr section) {
    struct namugen_hook_itfc* hook_itfc = ctx->namugen_hook_itfc;

    if (!compatible_mode && !alias && section.len == 0) {
        if (!strncasecmp(link.str, "br", link.len)) {
            inl_container_add_return(container, ctx);
            return;
        } else if (!strncmp(link.str, "\xeb\xaa\xa9\xec\xb0\xa8", link.len) || !strncasecmp(link.str, "tableofcontents", link.len)) {
            emit_toc(container, ctx);
            return;
        } else if (!strncmp(link.str, "\xea\xb0\x81\xec\xa3\xbc", link.len) || !strncasecmp(link.str, "footnote", link.len)) {
            emit_fnt_section(container, ctx);
            return;
        } else {
            char* left_par = strnstr(link.str, "(", link.len);
            char* border = link.str + link.len;
            if (left_par) {
                char *right_par = strnstr(left_par + 1, ")", border - (left_par + 1));
                if (right_par && right_par + 1 < border) {
                    bndstr fn = {link.str, left_par - link.str};
                    bndstr arg = {left_par + 1, right_par - left_par - 1};
                    if (hook_itfc->hook_fn_call(hook_itfc, ctx, container, fn, arg))
                        return;
                }
            }
            if (hook_itfc->hook_fn_link(hook_itfc, ctx, container, link))
                return;
        }
    }
    struct namuast_inl_link *linknode = (struct namuast_inl_link *)NEW_INL_NAMUAST(namuast_inltype_link);
    linknode->name = sdsnewlen(link.str, link.len);
    linknode->alias = alias;
    if (section.len > 0)
        linknode->section = sdsnewlen(section.str, section.len);
    else
        linknode->section = NULL;
    inl_container_add_steal(container, &linknode->_base);
}

void nm_inl_emit_upper_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, struct namuast_inl_container *alias, bndstr section) {
    sds cur_doc_name = ctx->cur_doc_name; // borrowed

    char *p;
    char *sep = NULL;
    for (p = cur_doc_name; *p; p++) {
        if (*p == '/')
            sep = p;
    }
    sds upper_doc_name;
    if (sep) {
        upper_doc_name = sdsnewlen(cur_doc_name, sep - cur_doc_name);
    } else {
        upper_doc_name = sdsnew("..");
    }
    sds sds_section = NULL;
    if (section.len > 0)
        sds_section = sdsnewlen(section.str, section.len);

    struct namuast_inl_link *linknode = (struct namuast_inl_link *)NEW_INL_NAMUAST(namuast_inltype_link);
    linknode->name = upper_doc_name;
    linknode->alias = alias;
    linknode->section = sds_section;
    inl_container_add_steal(container, &linknode->_base);
}

void nm_inl_emit_lower_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, bndstr link, struct namuast_inl_container *alias, bndstr section) {
    sds docname = sdscatlen(sdscat(sdsdup(ctx->cur_doc_name), "/"), link.str, link.len);
    struct namuast_inl_link *linknode = (struct namuast_inl_link *)NEW_INL_NAMUAST(namuast_inltype_link);
    linknode->name = docname;
    linknode->alias = alias;
    if (section.len > 0)
        linknode->section = sdsnewlen(section.str, section.len);
    else
        linknode->section = NULL;
    inl_container_add_steal(container, &linknode->_base);
}

void nm_inl_emit_external_link(struct namuast_inl_container* container, bndstr link, struct namuast_inl_container* alias) {
    struct namuast_inl_extlink *extlink = (struct namuast_inl_extlink *)NEW_INL_NAMUAST(namuast_inltype_extlink);
    extlink->href = sdsnewlen(link.str, link.len);
    extlink->alias = alias;
    inl_container_add_steal(container, &extlink->_base);
}


void nm_inl_emit_image(struct namuast_inl_container* container, bndstr url, bndstr width, bndstr height, int align) {
   struct namuast_inl_image *image = (struct namuast_inl_image *)NEW_INL_NAMUAST(namuast_inltype_image);
   image->src = sdsnewlen(url.str, url.len);
    if (width.len > 0)
        image->width = sdsnewlen(width.str, width.len);
    else
        image->width = NULL;
    if (height.len > 0)
        image->height = sdsnewlen(height.str, height.len);
    else
        image->height = NULL;
    image->align = align;
    inl_container_add_steal(container, &image->_base);
}

void nm_inl_emit_footnote_mark(struct namuast_inl_container* container, struct namugen_ctx* ctx, int id, bndstr fnt_literal) {
    struct namuast_inl_fnt* fnt = get_footnote_by_id(ctx->result_container, id);
    if (!fnt) return;
    if (fnt->raw)
        sdsfree(fnt->raw);
    fnt->raw = sdsnewlen(fnt_literal.str, fnt_literal.len);
    OBTAIN_NAMUAST(fnt);
    inl_container_add_steal(container, &fnt->_base);
}

static bool p_emit_raw(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, bndstr raw) {
    struct namuast_block *block = (struct namuast_block *)NEW_NAMUAST(namuast_type_block);
    block->block_type = block_type_raw;
    block->data.raw = sdsnewlen(raw.str, raw.len);

    namuast_container_add_steal(ctx->result_container, &block->_base);
    return true;
}

static bool p_emit_html(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, bndstr html) {
    struct namuast_block *block = (struct namuast_block *)NEW_NAMUAST(namuast_type_block);
    block->block_type = block_type_html;
    block->data.html = sdsnewlen(html.str, html.len);
    namuast_container_add_steal(ctx->result_container, &block->_base);
    return true;
}

static bool i_emit_raw(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, bndstr raw) {
    struct namuast_inl_block *inlblock = (struct namuast_inl_block *)NEW_INL_NAMUAST(namuast_inltype_block);
    inlblock->inl_block_type = inlblock_type_raw;
    inlblock->data.raw = sdsnewlen(raw.str, raw.len);
    inlblock->content = NULL;

    inl_container_add_steal(outer_inl, &inlblock->_base);
    return true;
}

static bool i_emit_highlighted_block(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, struct namuast_inl_container* content, int level) {
    struct namuast_inl_block *inlblock = (struct namuast_inl_block *)NEW_INL_NAMUAST(namuast_inltype_block);
    inlblock->inl_block_type = inlblock_type_highlight;
    inlblock->data.highlight_level = level;
    inlblock->content = content;
    inl_container_add_steal(outer_inl, &inlblock->_base);
    return true;
}

static bool i_emit_colored_block(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, struct namuast_inl_container* content, bndstr webcolor) {
    struct namuast_inl_block *inlblock = (struct namuast_inl_block *)NEW_INL_NAMUAST(namuast_inltype_block);
    inlblock->inl_block_type = inlblock_type_color;
    inlblock->data.webcolor = sdsnewlen(webcolor.str, webcolor.len);
    inlblock->content = content;

    inl_container_add_steal(outer_inl, &inlblock->_base);
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


void namugen_init(namugen_ctx* ctx, const char* cur_doc_name, struct namugen_hook_itfc* namugen_hook_itfc) {
    ctx->is_in_footnote = false;
    ctx->last_footnote_id = 0;
    ctx->last_anon_fnt_num = 0;

    struct namuast_container *container =  (struct namuast_container *)NEW_NAMUAST(namuast_type_container);
    container->len = 0;
    container->capacity = 128;
    container->children = calloc(container->capacity, sizeof(namuast_base *));
    container->root_heading = make_heading(NULL, NULL, 0);
    list_init(&container->fnt_list);
    ctx->result_container = container;

    ctx->namugen_hook_itfc = namugen_hook_itfc;
    ctx->cur_doc_name = sdsnew(cur_doc_name);

    ctx->shared_hr = (struct namuast_hr *)NEW_NAMUAST(namuast_type_hr);
    ctx->shared_return = (struct namuast_return *)NEW_NAMUAST(namuast_type_return);
    ctx->shared_inl_return = (struct namuast_inl_return *)NEW_INL_NAMUAST(namuast_inltype_return);
    ctx->shared_toc = (struct namuast_inl_toc*)NEW_INL_NAMUAST(namuast_inltype_toc);
}

namuast_container* namugen_obtain_ast(struct namugen_ctx *ctx) {
    namuast_container* result = ctx->result_container;
    OBTAIN_NAMUAST(result);
    return result;
}


void namugen_remove(struct namugen_ctx* ctx) {
    XRELEASE_NAMUAST(ctx->result_container);
    RELEASE_NAMUAST(ctx->shared_hr);
    RELEASE_NAMUAST(ctx->shared_return);
    RELEASE_NAMUAST(ctx->shared_inl_return);
    RELEASE_NAMUAST(ctx->shared_toc);
    sdsfree(ctx->cur_doc_name);
}


static void remove_fnt_list(struct list *fnt_list) {
    while (!list_empty(fnt_list)) {
        struct namuast_inl_fnt *fnt = list_entry(list_pop_back(fnt_list), struct namuast_inl_fnt, elem);
        RELEASE_NAMUAST(fnt);
    }
}

static void inl_dtor_container(namuast_inline *inl) {
    struct namuast_inl_container *container = (struct namuast_inl_container *)inl;
    size_t idx;
    for (idx = 0; idx < container->len; idx++) {
        RELEASE_NAMUAST(container->children[idx]);
    }
    free(container->children);
}

static void dtor_container(namuast_base *base) {
    struct namuast_container *container = (struct namuast_container *)base;
    size_t idx;
    for (idx = 0; idx < container->len; idx++) {
        RELEASE_NAMUAST(container->children[idx]);
    }
    free(container->children);

    remove_fnt_list(&container->fnt_list);
    RELEASE_NAMUAST(container->root_heading);
}

static void dtor_quotation(namuast_base *base) {
    struct namuast_quotation *quote = (struct namuast_quotation *)base;
    RELEASE_NAMUAST(quote->content);
}

static void dtor_inline(namuast_base *base) {
    struct namuast_inline *inl = (struct namuast_inline *)base;
    namuast_inl_dtor dtor = namuast_inl_optbl[inl->inl_type].dtor;
    if (dtor) {
        dtor(inl);
    }
}


static void dtor_heading(namuast_base *base) {
    struct namuast_heading* root = (struct namuast_heading *)base;
    if (root->content)
        RELEASE_NAMUAST(root->content);
    sdsfree(root->section_name);
    while (!list_empty(&root->children)) {
        struct namuast_heading *child = list_entry(list_pop_front(&root->children), struct namuast_heading, elem);
        RELEASE_NAMUAST(child);
    }
}

static void dtor_block(namuast_base *base) {
    struct namuast_block* block = (struct namuast_block *)base;
    switch (block->block_type) {
    case block_type_html:
        sdsfree(block->data.html);
        break;
    case block_type_raw:
        sdsfree(block->data.raw);
        break;
    }
}

static void inl_dtor_str(namuast_inline *inl) {
    struct namuast_inl_str *s = (struct namuast_inl_str *)inl;
    sdsfree(s->str);
}

static void inl_dtor_fnt(namuast_inline *inl) {
    struct namuast_inl_fnt *fnt = (struct namuast_inl_fnt *)inl;
    if (fnt->is_named)
        sdsfree(fnt->repr.name);
    if (fnt->raw)
        sdsfree(fnt->raw);
    RELEASE_NAMUAST(fnt->content);
}

static void inl_dtor_span(namuast_inline *inl) {
    struct namuast_inl_span *span = (struct namuast_inl_span *)inl;
    RELEASE_NAMUAST(span->content);
}

static void inl_dtor_link(namuast_inline *inl) {
    struct namuast_inl_link *link = (struct namuast_inl_link *)inl;
    sdsfree(link->name);
    if (link->section)
        sdsfree(link->section);
    if (link->alias) {
        RELEASE_NAMUAST(link->alias);
    }
}

static void inl_dtor_block(namuast_inline *inl) {
    struct namuast_inl_block *block = (struct namuast_inl_block *)inl;
    switch (block->inl_block_type) {
    case inlblock_type_color:
        sdsfree(block->data.webcolor);
        RELEASE_NAMUAST(block->content);
        break;
    case inlblock_type_raw:
        sdsfree(block->data.raw);
        break;
    case inlblock_type_highlight:
        RELEASE_NAMUAST(block->content);
        break;
    }
}

static void inl_dtor_extlink(namuast_inline *inl) {
    struct namuast_inl_extlink *extlink = (struct namuast_inl_extlink *)inl;
    sdsfree(extlink->href);
    if (extlink->alias) {
        RELEASE_NAMUAST(extlink->alias);
    }
}
static void inl_dtor_image(namuast_inline* inl) {
    struct namuast_inl_image *image = (struct namuast_inl_image *)inl;
    sdsfree(image->src);
    if (image->width) {
        sdsfree(image->width);
    }
    if (image->height) {
        sdsfree(image->height);
    }
}


/*
 * Traversers here
 */
#define VISIT_PREORDER(trav, base) do {\
    if (trav->preorder[base->ast_type]) { \
        trav->preorder[base->ast_type](trav, base); \
    } \
} while (0)

#define VISIT_POSTORDER(trav, base) do {\
    if (trav->postorder[base->ast_type]) { \
        trav->postorder[base->ast_type](trav, base); \
    } \
} while (0)

#define VISIT_INL_PREORDER(trav, inl) do {\
    if (trav->inl_preorder[inl->inl_type]) { \
        trav->inl_preorder[inl->inl_type](trav, inl); \
    } \
} while (0)

#define VISIT_INL_POSTORDER(trav, inl) do {\
    if (trav->inl_postorder[inl->inl_type]) { \
        trav->inl_postorder[inl->inl_type](tarv, inl); \
    } \
} while (0)



#define SET_SIZETBL(type_no, type) namuast_sizetbl[type_no] = sizeof(type)
#define SET_INL_SIZETBL(type_no, type) namuast_inl_sizetbl[type_no] = sizeof(type)
#define SET_DTOR(type_no, fn) namuast_optbl[type_no].dtor = fn
#define SET_INL_DTOR(type_no, fn) namuast_inl_optbl[type_no].dtor = fn
void initmod_namugen() {
    SET_SIZETBL(namuast_type_container, struct namuast_container);
    SET_SIZETBL(namuast_type_return, struct namuast_return);
    SET_SIZETBL(namuast_type_hr, struct namuast_hr);
    SET_SIZETBL(namuast_type_quotation, struct namuast_quotation);
    SET_SIZETBL(namuast_type_inline, struct namuast_inline);
    SET_SIZETBL(namuast_type_block, struct namuast_block);
    SET_SIZETBL(namuast_type_table, struct namuast_table);
    SET_SIZETBL(namuast_type_list, struct namuast_list);
    SET_SIZETBL(namuast_type_heading, struct namuast_heading);

    SET_DTOR(namuast_type_container, dtor_container);
    SET_DTOR(namuast_type_return, NULL);
    SET_DTOR(namuast_type_hr, NULL);
    SET_DTOR(namuast_type_quotation, dtor_quotation);
    SET_DTOR(namuast_type_inline, dtor_inline);
    SET_DTOR(namuast_type_block, dtor_block);
    SET_DTOR(namuast_type_table, _namuast_dtor_table);
    SET_DTOR(namuast_type_list, _namuast_dtor_list);
    SET_DTOR(namuast_type_heading, dtor_heading);

    SET_INL_SIZETBL(namuast_inltype_str, struct namuast_inl_str);
    SET_INL_SIZETBL(namuast_inltype_container, struct namuast_inl_container);
    SET_INL_SIZETBL(namuast_inltype_link, struct namuast_inl_link);
    SET_INL_SIZETBL(namuast_inltype_extlink, struct namuast_inl_extlink);
    SET_INL_SIZETBL(namuast_inltype_block, struct namuast_inl_block);
    SET_INL_SIZETBL(namuast_inltype_fnt, struct namuast_inl_fnt);
    SET_INL_SIZETBL(namuast_inltype_fnt_section, struct namuast_inl_fnt_section);
    SET_INL_SIZETBL(namuast_inltype_toc, struct namuast_inl_toc);
    SET_INL_SIZETBL(namuast_inltype_span, struct namuast_inl_span);
    SET_INL_SIZETBL(namuast_inltype_return, struct namuast_inl_return);
    SET_INL_SIZETBL(namuast_inltype_image, struct namuast_inl_image);

    SET_INL_DTOR(namuast_inltype_str, inl_dtor_str);
    SET_INL_DTOR(namuast_inltype_container, inl_dtor_container);
    SET_INL_DTOR(namuast_inltype_link, inl_dtor_link);
    SET_INL_DTOR(namuast_inltype_extlink, inl_dtor_extlink);
    SET_INL_DTOR(namuast_inltype_image, inl_dtor_image);
    SET_INL_DTOR(namuast_inltype_block, inl_dtor_block);
    SET_INL_DTOR(namuast_inltype_fnt, inl_dtor_fnt);
    SET_INL_DTOR(namuast_inltype_fnt_section, NULL);
    SET_INL_DTOR(namuast_inltype_toc, NULL);
    SET_INL_DTOR(namuast_inltype_span, inl_dtor_span);
    SET_INL_DTOR(namuast_inltype_return, NULL);

}
