#ifndef _NAMUGEN_H
#define _NAMUGEN_H
#include <stdbool.h>

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

    char* caption;
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

const char* find_webcolor_by_name(char *name);

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
 * User-defined emitters
 */


extern struct nm_block_emitters block_emitter_ops_inline;
extern struct nm_block_emitters block_emitter_ops_paragraphic;
// compile-time operations

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inline* inl_ast);
void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_return(struct namugen_ctx* ctx);
void nm_emit_hr(struct namugen_ctx* ctx);
void nm_begin_footnote(struct namugen_ctx* ctx);
void nm_end_footnote(struct namugen_ctx* ctx);
bool nm_in_footnote(struct namugen_ctx* ctx);
void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl);
void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li);
int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inline* fnt, char* extra);

struct namuast_inline* namuast_make_inline(struct namugen_ctx* ctx);
void namuast_remove_inline(struct namuast_inline *inl);

void nm_inl_emit_span(struct namuast_inline* inl, struct namuast_inline* span, enum nm_span_type type);
void nm_inl_emit_char(struct namuast_inline* inl, char c);
void nm_inl_emit_link(struct namuast_inline* inl, char *link, char *alias, char *section);
void nm_inl_emit_upper_link(struct namuast_inline* inl, char *alias, char *section);
void nm_inl_emit_lower_link(struct namuast_inline* inl, char *link, char *alias, char *section);
void nm_inl_emit_external_link(struct namuast_inline* inl, char *link, char *alias);
void nm_inl_emit_image(struct namuast_inline* inl, char *url, char *width, char *height, int align);
void nm_inl_emit_footnote_mark(struct namuast_inline* inl, int id, struct namugen_ctx *ctx);

#endif // !_NAMUGEN_H
