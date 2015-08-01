#ifndef _NAMUGEN_H
#define _NAMUGEN_H

#include <stdlib.h>
#include <stdbool.h>
#include "sds/sds.h"
#include "list.h"

void initmod_namugen();

#define UNTIL_REACHING1(p, border, c1) while ((p) < (border) && *(p) != (c1))
#define UNTIL_REACHING2(p, border, c1, c2) while ((p) < (border) && *(p) != (c1) && *(p) != (c2))
#define UNTIL_REACHING3(p, border, c1, c2, c3) while ((p) < (border) && *(p) != (c1) && *(p) != (c2) && *(p) != (c3))
#define UNTIL_REACHING4(p, border, c1, c2, c3, c4) while ((p) < (border) && *(p) != (c1) && *(p) != (c2) && *(p) != (c3) && *(p) != (c4))
#define MET_EOF(p, border) ((p) >= (border))
#define UNTIL_NOT_REACHING1(p, border, c1) while ((p) < (border) && *(p) == (c1))
#define UNTIL_NOT_REACHING2(p, border, c1, c2) while ((p) < (border) && (*(p) == (c1) || *(p) == (c2)))
#define UNTIL_NOT_REACHING3(p, border, c1, c2, c3) while ((p) < (border) && (*(p) == (c1) || *(p) == (c2) || *(p) == (c3)))
#define CONSUME_SPACETAB(p, border) UNTIL_NOT_REACHING2(p, border, '\t', ' ') { (p)++; }
#define CONSUME_WHITESPACE(p, border) UNTIL_NOT_REACHING3(p, border, '\t', ' ', '\n') { (p)++; }
#define RCONSUME_SPACETAB(st, ed) \
    while ((st) < (ed)) { \
        bool __end__ = false; \
        switch (*((ed) - 1)) { \
        case ' ': case '\t': \
            ed--;  \
            break; \
        default: \
            __end__ = true; \
            break; \
        } \
        if (__end__) break; \
    }
#define RCONSUME_WHITESPACE(st, ed) \
    while ((st) < (ed)) { \
        bool __end__ = false; \
        switch (*((ed) - 1)) { \
        case ' ': case '\t': case '\n': \
            ed--;  \
            break; \
        default: \
            __end__ = true; \
            break; \
        } \
        if (__end__) break; \
    }

#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\n' || (c) == '\t')
#define EQ(p, border, c1) (!MET_EOF(p, border) && (*(p) == (c1)))
#define SAFE_INC(p, border)  if (!MET_EOF(p, border)) { p++; }
#define EQSTR(p, border, str) (!strncmp((p), (str), (border) - (p)))
#define PREFIXSTR(p, border, prefix) (((border) - (p)) >= strlen(prefix) && !strncmp((p), (prefix), strlen(prefix)))
#define CASEPREFIXSTR(p, border, prefix) (((border) - (p)) >= strlen(prefix) && !strncasecmp((p), (prefix), strlen(prefix)))

typedef struct {
    char *str;
    size_t len;
} bndstr;


enum nm_span_type {
    nm_span_none,
    nm_span_bold,
    nm_span_italic,
    nm_span_strike,
    nm_span_underline,
    nm_span_superscript,
    nm_span_subscript
};

enum nm_align_type {
    nm_align_none,
    nm_align_left,
    nm_align_center,
    nm_align_right,
    nm_valign_none,
    nm_valign_top,
    nm_valign_middle,
    nm_valign_bottom
};

enum nm_list_type {
    nm_list_indent,
    nm_list_unordered,
    nm_list_ordered,
    nm_list_upper_alpha,
    nm_list_lower_alpha,
    nm_list_upper_roman,
    nm_list_lower_roman,
};

enum namuast_type {
    namuast_type_container = 0,
    namuast_type_return,
    namuast_type_hr,
    namuast_type_quotation,
    namuast_type_block,
    namuast_type_table,
    namuast_type_list,
    namuast_type_heading,
    namuast_type_inline,
    namuast_type_N
};

enum namuast_inltype {
    namuast_inltype_str = 0,
    namuast_inltype_link,
    namuast_inltype_extlink,
    namuast_inltype_image,
    namuast_inltype_block,
    namuast_inltype_fnt,
    namuast_inltype_fnt_section,
    namuast_inltype_toc,
    namuast_inltype_span,
    namuast_inltype_return,
    namuast_inltype_container,
    namuast_inltype_macro,
    namuast_inltype_N
};

struct namuast_base;
struct namuast_inline;

typedef void (*namuast_dtor)(struct namuast_base *);
/*
typedef struct namuast_traverser {
    void (*preorder[namuast_type_N])(struct namuast_traverser*, struct namuast_base *);
    void (*postorder[namuast_type_N])(struct namuast_traverser*, struct namuast_base *);
    void (*inl_preorder[namuast_inltype_N])(struct namuast_traverser*, struct namuast_inline*);
    void (*inl_postorder[namuast_inltype_N])(struct namuast_traverser*, struct namuast_inline*);
} namuast_traverser;
*/

struct {
    namuast_dtor dtor;
    // void (*traverse)(struct namuast_base *b, namuast_traverser *trav);
} namuast_optbl[namuast_type_N];

size_t namuast_sizetbl[namuast_type_N];
size_t namuast_inl_sizetbl[namuast_inltype_N];
typedef void (*namuast_inl_dtor)(struct namuast_inline *);
struct {
    namuast_inl_dtor dtor;
     // void (*traverse)(struct namuast_inline *inl, namuast_traverser *trav);
} namuast_inl_optbl[namuast_inltype_N];

/*
 * Paragraphic Objects
 */
typedef struct namuast_base {
    enum namuast_type ast_type;
    long refcount;
} namuast_base;

typedef struct namuast_container {
    namuast_base _base;
    namuast_base** children;
    size_t len;
    size_t capacity;

    struct list fnt_list;
    struct namuast_heading* root_heading; // owns root of headings
} namuast_container;

typedef struct namuast_heading {
    namuast_base _base;
    struct namuast_inl_container *content;
    sds section_name;

    int seq;

    int h_num;

    struct list_elem elem;
    struct list children;
    struct namuast_heading* parent; // weak reference
} namuast_heading;

typedef struct namuast_return {
    namuast_base _base;
} namuast_return;

typedef struct namuast_hr {
    namuast_base _base;
} namuast_hr;


struct namuast_quotation {
    namuast_base _base;
    struct namuast_inl_container* content;
};

struct namuast_block {
    namuast_base _base;
    enum {
        block_type_html,
        block_type_raw
    } block_type;
    union {
        sds html;
        sds raw;
    } data;
};

struct namuast_table {
    namuast_base _base;
    size_t max_col_count;
    int align;
    struct namuast_inl_container* caption;
    sds bg_webcolor;
    sds width; 
    sds height; 
    sds border_webcolor;
    size_t row_count;
    size_t row_size;
    struct namuast_table_row *rows; // [row][col]
};

struct namuast_list {
    namuast_base _base;
    enum nm_list_type type;
    struct namuast_inl_container* content;
    struct namuast_list* sublist;
    struct namuast_list* next;
};

/*
 * Inline objects
 */

typedef struct namuast_inline {
    namuast_base _base;
    enum namuast_inltype inl_type;
} namuast_inline;

typedef struct namuast_inl_container {
    namuast_inline _base;
    namuast_inline** children;
    size_t len;
    size_t capacity;
} namuast_inl_container;

struct namuast_inl_str {
    namuast_inline _base;
    sds str;
};

struct namuast_inl_link {
    namuast_inline _base;
    sds name;
    sds section; // may be NULL
    namuast_inl_container* alias; // may be NULL
};

struct namuast_inl_extlink {
    namuast_inline _base;
    sds href;
    namuast_inl_container* alias; // may be NULL
};

struct namuast_inl_image {
    namuast_inline _base;
    sds src;
    sds width; // may be NULL
    sds height; // may be NULL
    enum nm_align_type align;
};

struct namuast_inl_block {
    namuast_inline _base;
    enum {
        inlblock_type_color,
        inlblock_type_highlight,
        inlblock_type_raw
    } inl_block_type;
    union {
        sds webcolor;
        int highlight_level;
        sds raw;
    } data;
    namuast_inl_container *content; // may be NULL
};

struct namuast_inl_fnt {
    namuast_inline _base;

    int id;
    bool is_named;
    union {
        sds name;
        int anon_num;
    } repr;
    namuast_inl_container* content;
    sds raw;
    struct list_elem elem;
};

struct namuast_inl_fnt_section {
    namuast_inline _base;
    int cur_footnote_id;
};

struct namuast_inl_toc {
    namuast_inline _base;
};

struct namuast_inl_return {
    namuast_inline _base;
};

struct namuast_inl_span {
    namuast_inline _base;
    enum nm_span_type span_type;
    namuast_inl_container* content;
};

struct namuast_inl_macro {
    namuast_inline _base;
    sds name;

    bool is_fn;
    size_t pos_args_len;
    size_t kw_args_len;
    sds *pos_args;

    // kw_args is an array of length of 2 * kw_args_len, where items of odd index represent keyword, while items of even index reprensent value.
    // key-value pairs are sorted in the ascending order.
    sds *kw_args; 

    sds raw;
};

namuast_inl_container* make_inl_container();


namuast_base* _init_namuast_base(namuast_base *base, int type);
namuast_inline* _init_namuast_inl_base(namuast_inline *inl, int type);

#define OBTAIN_NAMUAST(ast) ((struct namuast_base*)(ast))->refcount++
#define _NEW_NAMUAST(type, typesize) _init_namuast_base((namuast_base*)calloc(typesize, 1), type)
#define NEW_NAMUAST(type) _NEW_NAMUAST(type, namuast_sizetbl[type])
#define NEW_INL_NAMUAST(inltype) _init_namuast_inl_base((namuast_inline *)_NEW_NAMUAST(namuast_type_inline, namuast_inl_sizetbl[inltype]), inltype);
#define RELEASE_NAMUAST(ast) do { \
    struct namuast_base* __base__ = (struct namuast_base*)(ast); \
    if (--__base__->refcount <= 0) { \
        namuast_dtor dtor = namuast_optbl[__base__->ast_type].dtor; \
        if (dtor) dtor(__base__); \
        free(__base__); \
    } \
} while (0)

#define XRELEASE_NAMUAST(ast) do { if (ast) { RELEASE_NAMUAST(ast); } } while (0)

bndstr to_bndstr(char *st, char *ed);

struct namuast_inline;
struct namuast_table_cell {
    struct namuast_inl_container* content;
    int rowspan;
    int colspan;
    enum nm_align_type align;
    enum nm_align_type valign;

    sds bg_webcolor;
    sds width; 
    sds height; 
};

struct namuast_table_row {
    sds bg_webcolor;

    size_t col_size;
    size_t col_count;
    struct namuast_table_cell* cols;
};



struct namugen_ctx;

struct namuast_list* namuast_make_list(int type, struct namuast_inl_container* content);
void _namuast_dtor_list(struct namuast_base *);
void _namuast_dtor_table(struct namuast_base *);


/*
 * Definitions of dynamic operations
 */

// IMPORTANT: 
// As calling all dynamic operations, 
// scanner give the ownership for namuast_inl_container and relinquish its ownership.
// Thus, memories which are given as arguments should be freed when operations are called, even if an operation returns false.
// The only exceptions is when the field corresponding to an operation is NULL. In this case, scanner will internally free memory.
struct nm_block_emitters {
    // NOTE: don't free outer_inl
    bool (*emit_raw)(struct namugen_ctx* ctx, struct namuast_inl_container* container, bndstr raw);
    bool (*emit_highlighted_block)(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, struct namuast_inl_container* content, int level);
    bool (*emit_colored_block)(struct namugen_ctx* ctx, struct namuast_inl_container* outer_inl, struct namuast_inl_container* content, bndstr webcolor);
    bool (*emit_html)(struct namugen_ctx* ctx, struct namuast_inl_container* container, bndstr html); 
};

/*
 * Functins visible to .lex files
 */
bool scn_parse_block(char *p, char *border, char **p_out, struct nm_block_emitters* ops, struct namugen_ctx* ctx, struct namuast_inl_container* container);
void scn_parse_link_content(char *p, char* border, char* pipe_pos, struct namugen_ctx* ctx, struct namuast_inl_container* container);
namuast_inl_container* scn_parse_inline(namuast_inl_container *container, char *p, char* border, char **p_out, struct namugen_ctx* ctx);

/*
 * User-defined emitters
 */


extern struct nm_block_emitters block_emitter_ops_inline;
extern struct nm_block_emitters block_emitter_ops_paragraphic;
// compile-time operations

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inl_container* content);
void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inl_container* container);
void nm_emit_return(struct namugen_ctx* ctx);
void nm_emit_hr(struct namugen_ctx* ctx, int hr_num);
void nm_begin_footnote(struct namugen_ctx* ctx);
void nm_end_footnote(struct namugen_ctx* ctx);
bool nm_in_footnote(struct namugen_ctx* ctx);
void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inl_container* container);
void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl);
void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li);
void nm_on_start(struct namugen_ctx* ctx);
void nm_on_finish(struct namugen_ctx* ctx);
int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inl_container* fnt_content, bndstr head);

struct namuast_inl_container* namuast_make_inline(struct namugen_ctx* ctx);
void inl_container_add_steal(struct namuast_inl_container* container, namuast_inline *src);
void inl_container_add_return(struct namuast_inl_container* container, struct namugen_ctx *ctx);

void nm_inl_emit_span(struct namuast_inl_container* container, struct namuast_inl_container* span, enum nm_span_type type);
void nm_inl_emit_str(struct namuast_inl_container* container, bndstr s);
void nm_inl_emit_macro(struct namuast_inl_container* container, struct namugen_ctx *ctx, bndstr name, bool is_fn, size_t pos_args_len, bndstr* pos_args, size_t kw_args_len, bndstr* kw_args, bndstr raw);
void nm_inl_emit_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, bndstr link, struct namuast_inl_container *alias, bndstr section);
void nm_inl_emit_upper_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, struct namuast_inl_container *alias, bndstr section);
void nm_inl_emit_lower_link(struct namuast_inl_container* container, struct namugen_ctx *ctx, bndstr link, struct namuast_inl_container *alias, bndstr section);
void nm_inl_emit_external_link(struct namuast_inl_container* container, bndstr link, struct namuast_inl_container *alias);
void nm_inl_emit_image(struct namuast_inl_container* container, bndstr url, bndstr width, bndstr height, int align);
void nm_inl_emit_footnote_mark(struct namuast_inl_container* container, struct namugen_ctx* ctx, int id, bndstr fnt_literal);

/*
 * Namugen 
 */


typedef struct namugen_ctx {
    bool is_in_footnote;
    int last_footnote_id;
    int last_anon_fnt_num;

    struct namuast_container *result_container;
    sds cur_doc_name;

    /*
     * Constants
     */
    struct namuast_hr *shared_hr;
    struct namuast_return *shared_return;
    struct namuast_inl_return *shared_inl_return;
    struct namuast_inl_toc *shared_toc;
} namugen_ctx; 


void namugen_init(namugen_ctx* ctx, const char* cur_doc_name);
void namugen_scan(struct namugen_ctx *ctx, char *buffer, size_t len);
namuast_container* namugen_obtain_ast(struct namugen_ctx *ctx);
void namugen_remove(namugen_ctx* ctx);


#endif // !_NAMUGEN_H
