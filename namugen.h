#ifndef _NAMUGEN_H
#define _NAMUGEN_H

enum namu_span_type {
    nm_span_none,
    nm_span_bold,
    nm_span_italic,
    nm_span_strike,
    nm_span_underline,
    nm_span_superscript,
    nm_span_subscript
};

enum {
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
 * User-defined emitters
 */

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inline* inl_ast);
void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_return(struct namugen_ctx* ctx);
void nm_emit_hr(struct namugen_ctx* ctx);
void nm_emit_html(struct namugen_ctx* ctx, char* html);
void nm_emit_raw(struct namugen_ctx* ctx, char* raw);
void nm_begin_comment(struct namugen_ctx* ctx, char* extra);
void nm_end_comment(struct namugen_ctx* ctx);
bool nm_in_comment(struct namugen_ctx* ctx);
void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inline* inl);
void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* tbl);
void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li);
void nm_emit_indent(struct namugen_ctx* ctx, struct namuast_list* ind);

struct namuast_inline* namuast_make_inline(struct namugen_ctx* ctx);
void namuast_remove_inline(struct namuast_inline *inl);

void nm_inl_set_span_type(struct namuast_inline* inl, enum namu_span_type type);
void nm_inl_set_color(struct namuast_inline* inl, char *webcolor);
void nm_inl_set_highlight(struct namuast_inline* inl, int highlight_level);
void nm_inl_emit_char(struct namuast_inline* inl, char c);
void nm_inl_emit_link(struct namuast_inline* inl, char *link, char *alias, char *section);
void nm_inl_emit_upper_link(struct namuast_inline* inl, char *alias, char *section);
void nm_inl_emit_lower_link(struct namuast_inline* inl, char *link, char *alias, char *section);
void nm_inl_emit_external_link(struct namuast_inline* inl, char *link, char *alias);
void nm_inl_emit_image(struct namuast_inline* inl, char *url, char *width, char *height, int align);

#endif // !_NAMUGEN_H
