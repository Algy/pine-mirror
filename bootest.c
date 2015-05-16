#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "namugen.h"

#define SAFELY_FREE(x) if (x) free(x)
struct namugen_ctx {
    char *s;
    size_t len;
    size_t size;

    bool is_in_footnote;
    int last_footnote_id;
};

static struct namugen_ctx* make_ctx() {
    struct namugen_ctx* ctx = malloc(sizeof(struct namugen_ctx));
    ctx->len = 0;
    ctx->size = 64;
    ctx->is_in_footnote = false;
    ctx->last_footnote_id = -1;
    ctx->s = calloc(1, ctx->size);
    return ctx;
}


static void free_ctx(struct namugen_ctx *ctx) {
    free(ctx->s);
    free(ctx);
}

static void ctx_append(struct namugen_ctx *ctx, char *s) {
    size_t len = strlen(s);
    if (len + ctx->len + 1 >= ctx->size) {
        ctx->size *= 2;
        ctx->s = realloc(ctx->s, ctx->size);
    }
    memcpy(ctx->s + ctx->len, s, len);
    ctx->len += len;
    ctx->s[ctx->len] = 0;
}

struct namuast_inline {
    char s[1025];
    size_t len;
};

static char* span_type_to_str(enum nm_span_type type) { 
    switch (type) {
    case nm_span_none:
        return "normal";
    case nm_span_bold:
        return "bold";
    case nm_span_italic:
        return "italic";
    case nm_span_strike:
        return "strike";
    case nm_span_underline:
        return "underline";
    case nm_span_superscript:
        return "superscript";
    case nm_span_subscript:
        return "subscript";
    default:
        return "unknown";
    }
}

static void append(struct namuast_inline *inl, char *s) {
    size_t len = strlen(s);
    if (inl->len + len + 1 < 1024) {
        memcpy(inl->s + inl->len, s, len);
        inl->len += len;
        inl->s[inl->len] = 0;
    }
}

static inline void push(struct namuast_inline* inl, char c) {
    if (inl->len < 1024) {
        inl->s[inl->len++] = c;
        inl->s[inl->len] = 0;
    }
}

static bool p_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    ctx_append(ctx, "==raw==\n");
    ctx_append(ctx, raw);
    ctx_append(ctx, "\n==end raw==\n");
    free(raw);
    return true;
}
static bool p_emit_html(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* html) {
    ctx_append(ctx, "==html==\n");
    ctx_append(ctx, html);
    ctx_append(ctx, "\n==end html==\n");
    free(html);
    return true;
}

static bool i_emit_raw(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, char* raw) {
    append(outer_inl, "(raw){");
    // we should truncate string at the beginning of second line
    char *first_return = strstr(raw, "\n");
    if (first_return)
        *first_return = 0; 
    append(outer_inl, raw);
    append(outer_inl, "}");
    free(raw);
    return true;
}

static bool i_emit_highlighted_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, int level) {
    char str[100];
    sprintf(str, "(highlight%d){", level);
    append(outer_inl, str);
    append(outer_inl, content->s);
    append(outer_inl, "}");

    namuast_remove_inline(content);
    return true;
}

static bool i_emit_colored_block(struct namugen_ctx* ctx, struct namuast_inline* outer_inl, struct namuast_inline* content, char* webcolor) {
    append(outer_inl, "(webcolor ");
    append(outer_inl, webcolor);
    append(outer_inl, ") {");
    append(outer_inl, content->s);
    append(outer_inl, "}");
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

void nm_emit_heading(struct namugen_ctx* ctx, int h_num, struct namuast_inline* inl) {
    char fmt[100];
    sprintf(fmt, "Heading%d@", h_num);
    ctx_append(ctx, fmt);
    ctx_append(ctx, inl->s);
    namuast_remove_inline(inl);
}
void nm_emit_inline(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx_append(ctx, inl->s);
    namuast_remove_inline(inl);
}

void nm_emit_return(struct namugen_ctx* ctx) {
    ctx_append(ctx, "\n");
}

void nm_emit_hr(struct namugen_ctx* ctx, int hr_num) {
    char str[100];
    sprintf(str, "--HR%d--", hr_num);
    ctx_append(ctx, str);
}


void nm_begin_footnote(struct namugen_ctx* ctx) {
    ctx->is_in_footnote = true;
}

void nm_end_footnote(struct namugen_ctx* ctx) {
    ctx->is_in_footnote = false;
}

bool nm_in_footnote(struct namugen_ctx* ctx) {
    return ctx->is_in_footnote;
}
void nm_emit_quotation(struct namugen_ctx* ctx, struct namuast_inline* inl) {
    ctx_append(ctx, "Quotation: ");
    ctx_append(ctx, inl->s);
    namuast_remove_inline(inl);
}
void nm_emit_table(struct namugen_ctx* ctx, struct namuast_table* table) {
    ctx_append(ctx, "[table] ");
    if (table->caption) {
        ctx_append(ctx, table->caption);
    }
    ctx_append(ctx, "(");
#define APPEND_ATTR(strlit, expr)  \
    if (table->expr) { \
        if (!is_first) { \
            ctx_append(ctx, ", "); \
        } else \
            is_first = false; \
        ctx_append(ctx, strlit); \
        ctx_append(ctx, "="); \
        ctx_append(ctx, table->expr); \
    }

    bool is_first = true;
    APPEND_ATTR("bg_webcolor", bg_webcolor);
    APPEND_ATTR("width", width);
    APPEND_ATTR("height", height);
    APPEND_ATTR("border_webcolor", border_webcolor);
    ctx_append(ctx, ")\n");

    int idx, kdx;
    for (idx = 0; idx < table->row_count; idx++) {
        struct namuast_table_row* row = &table->rows[idx];

        ctx_append(ctx, " [row](");
        if (row->bg_webcolor) {
            ctx_append(ctx, "bg_webcolor=");
            ctx_append(ctx, row->bg_webcolor);
        }
        ctx_append(ctx, ")\n");
        for (kdx = 0; kdx < row->col_count; kdx++) {
            struct namuast_table_cell *cell = &row->cols[kdx];
            ctx_append(ctx, "  [cell](");
            // TODO: print cell options
            ctx_append(ctx, ") ");
            ctx_append(ctx, cell->content->s);
            ctx_append(ctx, "\n");
        }
    }
    namuast_remove_table(table);
}

static char list_type_to_ch(int list_type) {
    switch (list_type) {
    case nm_list_indent:
        return 'i';
    case nm_list_ordered:
        return '1';
    case nm_list_unordered:
        return '*';
    case nm_list_lower_alpha:
        return 'a';
    case nm_list_upper_alpha:
        return 'A';
    case nm_list_lower_roman:
        return 'r';
    case nm_list_upper_roman:
        return 'R';
    }
    return '?';
}

static void traverse_list(struct namugen_ctx* ctx, struct namuast_list* li, int depth) {
    if (!li) return;

    int idx;
    for (idx = 0; idx < depth; idx++) {
        ctx_append(ctx, " ");
    }
    char sym[] = {list_type_to_ch(li->type), 0};
    ctx_append(ctx, sym);
    ctx_append(ctx, " ");
    if (li->content)
        ctx_append(ctx, li->content->s);
    ctx_append(ctx, "\n");
    traverse_list(ctx, li->sublist, depth + 2);
    traverse_list(ctx, li->next, depth);
}

void nm_emit_list(struct namugen_ctx* ctx, struct namuast_list* li) {
    traverse_list(ctx, li, 0);
    namuast_remove_list(li);
}

int nm_register_footnote(struct namugen_ctx* ctx, struct namuast_inline* fnt, char* extra) {
    ctx_append(ctx, "\n>>");
    if (extra) {
        ctx_append(ctx, extra);
    }
    ctx_append(ctx, "\n");
    ctx_append(ctx, fnt->s);
    ctx_append(ctx, "\n");
    ctx_append(ctx, "<<\n");
    namuast_remove_inline(fnt);
    if (extra) free(extra);
    return ++ctx->last_footnote_id;
}

struct namuast_inline* namuast_make_inline(struct namugen_ctx* ctx) {
    return calloc(1, sizeof(struct namuast_inline));
}

void namuast_remove_inline(struct namuast_inline *inl) {
    free(inl);
}

void nm_inl_set_span_type(struct namuast_inline* inl, enum nm_span_type type) {
    append(inl, "<span ");
    append(inl, span_type_to_str(type));
    append(inl, ">");
}
void nm_inl_set_color(struct namuast_inline* inl, char *webcolor) {
    append(inl, "<webcolor ");
    append(inl, webcolor);
    append(inl, ">");
}
void nm_inl_set_highlight(struct namuast_inline* inl, int highlight_level) {
    char s[100];
    sprintf(s, "<highlight+%d>", highlight_level);
    append(inl, s);
}
void nm_inl_emit_char(struct namuast_inline* inl, char c) {
    push(inl, c);
}

void nm_inl_emit_link(struct namuast_inline* inl, char *link, char *alias, char *section) {
    append(inl, "[wiki:");
    append(inl, link);
    if (section) {
        append(inl, " #");
        append(inl, section);
    }
    if (alias) {
        append(inl, " as ");
        append(inl, alias);
    }
    append(inl, "]");

    SAFELY_FREE(link);
    SAFELY_FREE(alias);
    SAFELY_FREE(section);
}

void nm_inl_emit_upper_link(struct namuast_inline* inl, char *alias, char *section) {
    append(inl, "[wiki:../");
    if (section) {
        append(inl, " #");
        append(inl, section);
    }
    if (alias) {
        append(inl, " as ");
        append(inl, alias);
    }
    append(inl, "]");

    SAFELY_FREE(alias);
    SAFELY_FREE(section);

}
void nm_inl_emit_lower_link(struct namuast_inline* inl, char *link, char *alias, char *section) {
    append(inl, "[wiki:/");
    append(inl, link);
    if (section) {
        append(inl, " #");
        append(inl, section);
    }
    if (alias) {
        append(inl, " as ");
        append(inl, alias);
    }
    append(inl, "]");

    SAFELY_FREE(link);
    SAFELY_FREE(alias);
    SAFELY_FREE(section);
}

void nm_inl_emit_span(struct namuast_inline* inl, struct namuast_inline* span, enum nm_span_type type) {
    append(inl, "<");
    append(inl, span_type_to_str(type));
    append(inl, ">");
    append(inl, span->s);
    append(inl, "</");
    append(inl, span_type_to_str(type));
    append(inl, ">");
    namuast_remove_inline(span);
}

void nm_inl_emit_external_link(struct namuast_inline* inl, char *link, char *alias) {
    append(inl, "[link:");
    append(inl, link);
    if (alias) {
        append(inl, " as ");
        append(inl, alias);
    }
    append(inl, "]");

    SAFELY_FREE(link);
    SAFELY_FREE(alias);
}
void nm_inl_emit_image(struct namuast_inline* inl, char *url, char *width, char *height, int align) {
    append(inl, "<image src=");
    append(inl, url);
    if (width) {
        append(inl, " width=");
        append(inl, width);
    }
    if (height) {
        append(inl, " height=");
        append(inl, height);
    }
    if (align != nm_align_none) {
        append(inl, " align=");
        switch (align) {
        case nm_align_left:
            append(inl, "left");
            break;
        case nm_align_center:
            append(inl, "center");
            break;
        case nm_align_right:
            append(inl, "right");
            break;
        }
    }
    append(inl, ">");
    SAFELY_FREE(url);
    SAFELY_FREE(width);
    SAFELY_FREE(height);
}

void nm_inl_emit_footnote_mark(struct namuast_inline* inl, int id, struct namugen_ctx *ctx) {
    char s[100];
    sprintf(s, "[fnt: %d]", id);
    append(inl, s);
}


int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Cannot open the file\n");
        return 1;
    }
    fseek(fp, 0L, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    char *buffer = calloc(filesize, 1);
    fread(buffer, 1, filesize + 1, fp);
    buffer[filesize] = 0;

    struct namugen_ctx* ctx = make_ctx();

    clock_t clock_st = clock();
    namugen_scan(ctx, buffer, filesize);
    clock_t clock_ed = clock();
    double us = (((double) (clock_ed - clock_st)) / CLOCKS_PER_SEC) * 1000. * 1000.;

    printf("%s\n", ctx->s);
    printf("generated in %.2lf us\n", us);
    free_ctx(ctx);
    free(buffer);
    fclose(fp);
    return 0;
}

