#ifndef _NAMUGEN_H
#define _NAMUGEN_H

#include <stdlib.h>
#include <stdbool.h>
#include "sds/sds.h"
#include "list.h"

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
#define PREFIXSTR(p, border, prefix) (!strncmp((p), (prefix), strlen(prefix)))
#define CASEPREFIXSTR(p, border, prefix) (!strncasecmp((p), (prefix), strlen(prefix)))


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
    nm_valign_center,
    nm_valign_bottom
};

enum {
    nm_list_indent,
    nm_list_unordered,
    nm_list_ordered,
    nm_list_upper_alpha,
    nm_list_lower_alpha,
    nm_list_upper_roman,
    nm_list_lower_roman,
};

typedef struct bndstr {
    char* str;
    size_t len;
} bndstr;
char* dup_str(char *st, char *ed);

struct namuast_inline;
struct namuast_table_cell {
    struct namuast_inline* content;
    int rowspan;
    int colspan;
    int align;
    int valign;

    char *bg_webcolor;
    char* width; 
    char* height; 
};

struct namuast_table_row {
    char *bg_webcolor;

    size_t col_size;
    size_t col_count;
    struct namuast_table_cell* cols;
};

struct namuast_table {
    size_t max_col_count;

    int align;
    struct namuast_inline* caption;
    char *bg_webcolor;
    char* width; 
    char* height; 
    char* border_webcolor;
    size_t row_count;
    size_t row_size;
    struct namuast_table_row *rows; // [row][col]
};

struct namuast_list {
    int type;
    struct namuast_inline* content;
    struct namuast_list* sublist;
    struct namuast_list* next;
};


struct namugen_ctx;


char* find_webcolor_by_name(char *name);

struct namuast_list* namuast_make_list(int type, struct namuast_inline* content);
void namuast_remove_list(struct namuast_list* inl);
struct namuast_table_cell* namuast_add_table_cell(struct namuast_table* table, struct namuast_table_row *row);
struct namuast_table_row* namuast_add_table_row(struct namuast_table* table);
void namuast_remove_table(struct namuast_table* table);

void namugen_scan(struct namugen_ctx *ctx, char *buffer, size_t len);

/*
 * Definitions of dynamic operations
 */

// IMPORTANT: 
// As calling all dynamic operations, 
// scanner give the ownership for char* and namuast_inline and relinquish its ownership.
// Thus, memories which are given as arguments should be freed when operations are called, even if an operation returns false.
// The only exceptions is when the field corresponding to an operation is NULL. In this case, scanner will internally free memory.
struct nm_block_emitters {
    // NOTE: don't free outer_inl
    bool (*emit_raw)(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw);
    bool (*emit_highlighted_block)(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, int level);
    bool (*emit_colored_block)(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, char* webcolor);
    bool (*emit_html)(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* html); 
};

/*
 * Functins visible to .lex files
 */
bool scn_parse_block(char *p, char *border, char **p_out, struct nm_block_emitters* ops, struct namugen_ctx* ctx, struct namuast_inline *outer_inl);
void scn_parse_link_content(char *p, char* border, char* pipe_pos, bool compatible_mode, struct namugen_ctx* ctx, struct namuast_inline* inl);
struct namuast_inline* scn_parse_inline(char *p, char* border, char **p_out, struct namugen_ctx* ctx);

/*
 * User-defined emitters
 */


extern struct nm_block_emitters block_emitter_ops_inline;
extern struct nm_block_emitters block_emitter_ops_paragraphic;
// compile-time operations

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inline* content);
void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_return(struct namugen_ctx* ctx);
void nm_emit_hr(struct namugen_ctx* ctx, int hr_num);
void nm_begin_footnote(struct namugen_ctx* ctx);
void nm_end_footnote(struct namugen_ctx* ctx);
bool nm_in_footnote(struct namugen_ctx* ctx);
void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl);
void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li);
void nm_on_start(struct namugen_ctx* ctx);
void nm_on_finish(struct namugen_ctx* ctx);
int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inline* fnt, char* head);

struct namuast_inline* namuast_make_inline(struct namugen_ctx* ctx);
void namuast_remove_inline(struct namuast_inline *inl);

void nm_inl_emit_span(struct namuast_inline* inl, struct namuast_inline* span, enum nm_span_type type);
void nm_inl_cat(struct namuast_inline* inl_dest, struct namuast_inline* inl_src, bool insert_br) ;
void nm_inl_emit_str(struct namuast_inline* inl, char* s, size_t len);
void nm_inl_emit_link(struct namuast_inline* inl, char *link, bool compatible_mode, char *alias, char *section);
void nm_inl_emit_upper_link(struct namuast_inline* inl, char *alias, char *section);
void nm_inl_emit_lower_link(struct namuast_inline* inl, char *link, char *alias, char *section);
void nm_inl_emit_external_link(struct namuast_inline* inl, char *link, char *alias);
void nm_inl_emit_image(struct namuast_inline* inl, char *url, char *width, char *height, int align);
void nm_inl_emit_footnote_mark(struct namuast_inline* inl, int id, char* fnt_literal, size_t len);

/*
 * Namugen 
 */

// We believe href generated by this interface is escaped
struct namugen_doc_itfc {
    void (*documents_exist)(struct namugen_doc_itfc *, int argc, char** docnames, bool *results);
    sds (*doc_href)(struct namugen_doc_itfc *, char *doc_name);
};

struct namugen_hook_itfc {
    bool (*hook_fn_link)(struct namugen_hook_itfc *, struct namugen_ctx *ctx, struct namuast_inline *inl, char *fn);
    bool (*hook_fn_call)(struct namugen_hook_itfc *, struct namugen_ctx *ctx, struct namuast_inline *inl, char *fn, char* arg);
};

#define MAX_TOC_COUNT 100
#define INITIAL_MAIN_BUF (4096*4)
#define INITIAL_INTERNAL_LINKS 1024

struct sdschunk {
    struct list_elem elem;
    sds buf;
    bool is_lazy;
};


struct namugen_ctx {
    bool is_in_footnote;
    int last_footnote_id;
    int last_anon_fnt_num;
    struct list fnt_list;

    struct heading* root_heading; // sentinel-heading

    struct sdschunk* toc_positions[MAX_TOC_COUNT];
    int toc_count;

    struct list internal_link_list;
    int internal_link_count;

    struct list main_chunk_list;
    sds main_fast_buf;

    struct namugen_doc_itfc *doc_itfc;
    struct namugen_hook_itfc *namugen_hook_itfc;

    sds cur_doc_name;
}; 


struct namugen_ctx* namugen_make_ctx(char* cur_doc_name, struct namugen_doc_itfc* doc_itfc, struct namugen_hook_itfc* namugen_hook_itfc);
sds namugen_ctx_flush_main_buf(struct namugen_ctx* ctx);
void namugen_remove_ctx(struct namugen_ctx* ctx);


#endif // !_NAMUGEN_H
