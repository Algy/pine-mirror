#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <assert.h>

#include "namugen.h"

#define MAX_HEADING_NUMBER 6
#define MAX_LIST_STACK_SIZE 32

#ifdef VERBOSE 
#include <stdio.h>
#define verbose_log(...) { \
        fprintf(stderr, "[verbose] "); \
        fprintf(stderr, __VA_ARGS__); \
}

#define verbose_str(st, ed) { \
    char *__p__; \
    for (__p__ = st; __p__ < ed; __p__++) { \
        fputc(*__p__, stderr); \
    } \
    fputc('\n', stderr); \
}

#else
#define verbose_log(...) { }
#define verbose_str(st, ed) {}
#endif


struct webcolor_name {
    const char *name;
    const char *webcolor; // don't free me!
};

// These webcolor names conforms to HTML5
struct webcolor_name webcolor_nametbl[] = {
    {"AliceBlue", "#F0F8FF"}, 
    {"AntiqueWhite", "#FAEBD7"},  
    {"Aqua", "#00FFFF"},  
    {"Aquamarine", "#7FFFD4"},
    {"Azure", "#F0FFFF"},  
    {"Beige", "#F5F5DC"},  
    {"Bisque", "#FFE4C4"},  
    {"Black", "#000000"},  
    {"BlanchedAlmond", "#FFEBCD"},
    {"Blue", "#0000FF"},  
    {"BlueViolet", "#8A2BE2"},  
    {"Brown", "#A52A2A"},  
    {"BurlyWood", "#DEB887"},  
    {"CadetBlue", "#5F9EA0"},
    {"Chartreuse", "#7FFF00"},
    {"Chocolate", "#D2691E"},  
    {"Coral", "#FF7F50"}, 
    {"CornflowerBlue", "#6495ED"}, 
    {"Cornsilk", "#FFF8DC"},
    {"Crimson", "#DC143C"},  
    {"Cyan", "#00FFFF"},  
    {"DarkBlue", "#00008B"},  
    {"DarkCyan", "#008B8B"},  
    {"DarkGoldenRod", "#B8860B"},  
    {"DarkGray", "#A9A9A9"},
    {"DarkGreen", "#006400"},
    {"DarkKhaki", "#BDB76B"},
    {"DarkMagenta", "#8B008B"},
    {"DarkOliveGreen", "#556B2F"},
    {"DarkOrange", "#FF8C00"},
    {"DarkOrchid", "#9932CC"},
    {"DarkRed", "#8B0000"},
    {"DarkSalmon", "#E9967A"},
    {"DarkSeaGreen", "#8FBC8F"},
    {"DarkSlateBlue", "#483D8B"},
    {"DarkSlateGray", "#2F4F4F"},
    {"DarkTurquoise", "#00CED1"},
    {"DarkViolet", "#9400D3"},
    {"DeepPink", "#FF1493"},
    {"DeepSkyBlue", "#00BFFF"},
    {"DimGray", "#696969"},
    {"DodgerBlue", "#1E90FF"},
    {"FireBrick", "#B22222"},
    {"FloralWhite", "#FFFAF0"},
    {"ForestGreen", "#228B22"},
    {"Fuchsia", "#FF00FF"},
    {"Gainsboro", "#DCDCDC"},
    {"GhostWhite", "#F8F8FF"},
    {"Gold", "#FFD700"},
    {"GoldenRod", "#DAA520"},
    {"Gray", "#808080"},
    {"Green", "#008000"},
    {"GreenYellow", "#ADFF2F"},
    {"HoneyDew", "#F0FFF0"},
    {"HotPink", "#FF69B4"},
    {"IndianRed", "#CD5C5C"},
    {"Indigo", "#4B0082"},
    {"Ivory", "#FFFFF0"},
    {"Khaki", "#F0E68C"},
    {"Lavender", "#E6E6FA"},
    {"LavenderBlush", "#FFF0F5"},
    {"LawnGreen", "#7CFC00"},
    {"LemonChiffon", "#FFFACD"},
    {"LightBlue", "#ADD8E6"},
    {"LightCoral", "#F08080"},
    {"LightCyan", "#E0FFFF"},
    {"LightGoldenRodYellow", "#FAFAD2"},
    {"LightGray", "#D3D3D3"},
    {"LightGreen", "#90EE90"},
    {"LightPink", "#FFB6C1"},
    {"LightSalmon", "#FFA07A"},
    {"LightSeaGreen", "#20B2AA"},
    {"LightSkyBlue", "#87CEFA"},
    {"LightSlateGray", "#778899"},
    {"LightSteelBlue", "#B0C4DE"},
    {"LightYellow", "#FFFFE0"},
    {"Lime", "#00FF00"},
    {"LimeGreen", "#32CD32"},
    {"Linen", "#FAF0E6"},
    {"Magenta", "#FF00FF"},
    {"Maroon", "#800000"},
    {"MediumAquaMarine", "#66CDAA"},
    {"MediumBlue", "#0000CD"},
    {"MediumOrchid", "#BA55D3"},
    {"MediumPurple", "#9370DB"},
    {"MediumSeaGreen", "#3CB371"},
    {"MediumSlateBlue", "#7B68EE"},
    {"MediumSpringGreen", "#00FA9A"},
    {"MediumTurquoise", "#48D1CC"},
    {"MediumVioletRed", "#C71585"},
    {"MidnightBlue", "#191970"},
    {"MintCream", "#F5FFFA"},
    {"MistyRose", "#FFE4E1"},
    {"Moccasin", "#FFE4B5"},
    {"NavajoWhite", "#FFDEAD"},
    {"Navy", "#000080"},
    {"OldLace", "#FDF5E6"},
    {"Olive", "#808000"},
    {"OliveDrab", "#6B8E23"},
    {"Orange", "#FFA500"},
    {"OrangeRed", "#FF4500"},
    {"Orchid", "#DA70D6"},
    {"PaleGoldenRod", "#EEE8AA"},
    {"PaleGreen", "#98FB98"},
    {"PaleTurquoise", "#AFEEEE"},
    {"PaleVioletRed", "#DB7093"},
    {"PapayaWhip", "#FFEFD5"},
    {"PeachPuff", "#FFDAB9"},
    {"Peru", "#CD853F"},
    {"Pink", "#FFC0CB"},
    {"Plum", "#DDA0DD"},
    {"PowderBlue", "#B0E0E6"},
    {"Purple", "#800080"},
    {"RebeccaPurple", "#663399"},
    {"Red", "#FF0000"},
    {"RosyBrown", "#BC8F8F"},
    {"RoyalBlue", "#4169E1"},
    {"SaddleBrown", "#8B4513"},
    {"Salmon", "#FA8072"},
    {"SandyBrown", "#F4A460"},
    {"SeaGreen", "#2E8B57"},
    {"SeaShell", "#FFF5EE"},
    {"Sienna", "#A0522D"},
    {"Silver", "#C0C0C0"},
    {"SkyBlue", "#87CEEB"},
    {"SlateBlue", "#6A5ACD"},
    {"SlateGray", "#708090"},
    {"Snow", "#FFFAFA"},
    {"SpringGreen", "#00FF7F"},
    {"SteelBlue", "#4682B4"},
    {"Tan", "#D2B48C"},
    {"Teal", "#008080"},
    {"Thistle", "#D8BFD8"},
    {"Tomato", "#FF6347"},
    {"Turquoise", "#40E0D0"},
    {"Violet", "#EE82EE"},
    {"Wheat", "#F5DEB3"},
    {"White", "#FFFFFF"},
    {"WhiteSmoke", "#F5F5F5"},
    {"Yellow", "#FFFF00"},
    {"YellowGreen", "#9ACD32"},
    {NULL, NULL}
};

const char* find_webcolor_by_name(char *name) {
    struct webcolor_name *wnm;
    for (wnm = webcolor_nametbl; wnm->name; wnm++) {
        if (!strcasecmp(wnm->name, name)) {
            return wnm->webcolor;
        }
    }
    return NULL;
}



static struct namuast_inline* parse_inline(char *p, char* border, char **p_out, struct namugen_ctx* ctx);

struct namuast_list* namuast_make_list(int type, struct namuast_inline* content) {
    struct namuast_list* retval = calloc(sizeof(struct namuast_list), 1);
    retval->type = type;
    retval->content = content;
    return retval;
}

void namuast_remove_list(struct namuast_list* inl) {
    struct namuast_list* sibling;
    for (sibling = inl; sibling; ) {
        if (sibling->content)
            namuast_remove_inline(sibling->content);
        if (sibling->sublist)
            namuast_remove_list(sibling->sublist);
        struct namuast_list* next = sibling->next;
        free(sibling);
        sibling = next;
    }
}


struct namuast_table_cell* namuast_add_table_cell(struct namuast_table* table, struct namuast_table_row *row) {
    if (row->col_count >= row->col_size) {
        row->col_size *= 2;
        row->cols = realloc(row->cols, sizeof(struct namuast_table_cell) * row->col_size);
    }
    struct namuast_table_cell* cell = &row->cols[row->col_count++];
    cell->content = NULL;
    cell->rowspan = 0;
    cell->align = nm_align_none;
    cell->valign = nm_valign_none;
    cell->bg_webcolor = NULL;
    cell->width = NULL;
    cell->height = NULL;

    if (table->max_col_count < row->col_count) {
        table->max_col_count = row->col_count;
    }
    return cell;
}

struct namuast_table_row* namuast_add_table_row(struct namuast_table* table) {
    if (table->row_count >= table->row_size) {
        table->row_size *= 2;
        table->rows = realloc(table->rows, sizeof(struct namuast_table_row) * table->row_size);
    }
    struct namuast_table_row* row = &table->rows[table->row_count++];
    row->col_size = 8;
    row->col_count = 0;
    row->bg_webcolor = NULL;
    row->cols = calloc(row->col_size, sizeof(struct namuast_table_cell));
    return row;
}

struct namuast_table* namuast_make_table() {
    struct namuast_table* table = malloc(sizeof(struct namuast_table));
    table->row_size = 8;
    table->row_count = 0;
    table->border_webcolor = NULL;
    table->width = NULL;
    table->height = NULL;
    table->bg_webcolor = NULL;
    table->caption = NULL;
    table->rows = calloc(table->row_size, sizeof(struct namuast_table_row));
    return table;
}

void namuast_remove_table(struct namuast_table* table) {
    int row_idx;
    for (row_idx = 0; row_idx < table->row_count; row_idx++) {
        struct namuast_table_row *row = &table->rows[row_idx];
        int col_idx;
        for (col_idx = 0; col_idx < row->col_count; col_idx++) {
            struct namuast_table_cell *cell = &row->cols[col_idx];
            if (cell->content) {
                namuast_remove_inline(cell->content);
            }
            if (cell->bg_webcolor) free(cell->bg_webcolor);
            if (cell->width) free(cell->width);
            if (cell->height) free(cell->height);
        }
        if (row->bg_webcolor) free(row->bg_webcolor);
        free(row->cols);
    }
    if (table->border_webcolor) free(table->border_webcolor);
    if (table->width) free(table->width);
    if (table->height) free(table->height);
    if (table->bg_webcolor) free(table->bg_webcolor);
    if (table->caption) free(table->caption);
    free(table->rows);
    free(table);
}

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

// TODO: are these right?
#define PREFIXSTR(p, border, prefix) (!strncmp((p), (prefix), strlen(prefix)))
#define CASEPREFIXSTR(p, border, prefix) (!strncasecmp((p), (prefix), strlen(prefix)))

static char* dup_str(char *st, char *ed) {
    char *retval = calloc(ed - st + 1, 1);
    memcpy(retval, st, ed - st);
    return retval;
}


typedef struct {
    char *tagname; // may be NULL
    size_t keyvalue_count;
    char **keys; // may be NULL only if keyvalue_count is 0
    char **values; // may be NULL only if keyvalue_count is 0
    char *after_pipe; // may be NULL
} celltag;

/*
static inline celltag* parse_xml_like_tag_content(char *p, char *border, char** p_out) {
#   define MAKE_KVSTRING(st_out, ed_out, value_follows_out) { \
        st_out = testp; \
        bool __exit_flag__ = false; \
        while (!MET_EOF(testp, border) && !__exit_flag__) { \
            switch (*testp++) { \
            case ' ': case '\n': case '\t': case '=': case '|': \
                __exit_flag__ = true; \
                break;  \
            } \
        } \
        ed_out = testp; \
        value_follows_out = EQ(testp, border, '='); \
    }
    celltag *retval = malloc(sizeof(celltag));
    char *testp = p;
    CONSUME_WHITESPACE(testp, border);

    char *tagname_or_first_key_st, *tagname_or_first_key_ed;
    bool first_chunk_is_key;
    MAKE_KVSTRING(tagname_or_first_key_st, tagname_or_first_key_ed, first_chunk_is_key);
    size_t first_chunk_memsize = tagname_or_first_key_ed - tagname_or_first_key_ed + 1;
    if (!first_chunk_is_key) {
        // tagname
        retval->tagname = calloc(1, first_chunk_memsize);
        memcpy(retval->tagname, tagname_or_first_key_ed, (first_chunk_memsize - 1));
    } else {
    }

    do {
        char *key_st, *key_ed;
        char *value_st, *value_ed;
        key_st = testp;
        key_ed = testp;
        if (EQ(key_ed, border, '=')) {
        }
#   undef MAKE_KVSTRING

        
    } while ( ) ;

}

TODO
*/

static void remove_celltag(celltag *tag) {
    /*
    int idx;
    if (tag->tagname) free(tag->tagname);
    if (celltag->keys) {
        for (idx = 0; idx < tag->keyvalue_count; idx++) {
            free(tag->keys[idx]);
            free(tag->values[idx]);
        }
    }
    if (tag->keys) free(tag->keys);
    if (tag->values) free(tag->values);
    if (tag->after_pipe) free(tag->after_pipe);
    TODO
    */
}

static inline void table_use_cell_ctrl(char *ctrl_st, char *ctrl_ed, bool is_first, struct namuast_table *table, struct namuast_table_row *row, struct namuast_table_cell *cell) {
    /*
    CONSUME_WHITESPACE(ctrl_st, ctrl_ed);
    char *sp = ctrl_st;
    char *testp = ctrl_st;
    char* label_st, *label_ed;

    bool first

    if (is_first && !strncmp(ctrl_st, "table", ctrl_ed - ctrl_st)) {
        CONSUME_WHITESPACE(ctrl_st, ctrl_ed);
    }
   

    if (is_first) {
        char saved_c = ctrl;
        sscanf(ctrl_st, );
    }
    */
    // TODO
}

// Hacky
static inline bool parse_table_cell(char *p, char *border, char **p_out, int *colspan_out, char** cell_ctrl_start_p_out, char** cell_ctrl_end_p_out, char** content_start_p_out, char** content_end_p_out) {
    // (||)* \s* <(.|\n)*> \s* CONTENT \s* ||
    char *testp = p;
    int dup_pipes = 0;
    UNTIL_NOT_REACHING1(testp, border, '|') {
        testp++;
        dup_pipes++;
    }
    if (dup_pipes % 2) {
        testp--;
        dup_pipes--;
    }
    int colspan = dup_pipes / 2;

    CONSUME_WHITESPACE(testp, border);

    /* <(.|\n)*> */
    char *cell_ctrl_start_p = NULL, *cell_ctrl_end_p = NULL;
    if (EQ(testp, border, '<')) {
        testp++; // consume '<'
        char* _tmp_st_p = testp;
        UNTIL_REACHING1(testp, border, '>') {
            testp++;
        }
        if (!MET_EOF(testp, border)) {
            cell_ctrl_start_p = _tmp_st_p;
            cell_ctrl_end_p = testp;
        }
        testp++; // consume '>'
    }
    CONSUME_WHITESPACE(testp, border);

    char *content_start_p, *content_end_p;

    content_start_p = content_end_p = testp;
    while (1) {
        if (MET_EOF(testp, border)) {
            return false;
        }
        if (EQ(testp + 1, border, '|') && *testp == '|') {
            content_end_p = testp;
            testp += 2;
            break;
        }
        testp++;
    }
    RCONSUME_SPACETAB(content_start_p, content_end_p);
    *colspan_out = colspan;
    *p_out = testp;
    *cell_ctrl_start_p_out = cell_ctrl_start_p;
    *cell_ctrl_end_p_out = cell_ctrl_end_p;
    *content_start_p_out = content_start_p;
    *content_end_p_out = content_end_p;
    return true;
}

static struct namuast_table* parse_table(char* p, char* border, char **p_out, struct namugen_ctx* ctx) {
    // assert(*p == '|');
    /*
     * ^|.*|(||)* \s* <(.|\n)*> \s* CONTENT \s* ( ( (||)+ <.*> CONTENT)* || \n )+
     */
    
    char *testp = p;
    char *caption_start, *caption_end;
    /*
     * ^|.*|
     */
    testp++; // consume '|'
    caption_start = testp;
    UNTIL_REACHING2(testp, border, '|', '\n') {
        testp++;
    }
    if (MET_EOF(testp, border) || *testp == '\n') {
        return NULL; 
    }
    caption_end = testp;
    testp++; // consume '|'

    struct namuast_table* table = namuast_make_table();
    if (caption_start < caption_end) {
        table->caption = dup_str(caption_start, caption_end);
    }
    bool is_first = true;
    do {
        struct namuast_table_row* row = namuast_add_table_row(table);
        do {
            struct namuast_table_cell* cell = namuast_add_table_cell(table, row);
            int colspan;
            char* cell_ctrl_st, *cell_ctrl_ed;
            char* cell_content_st, *cell_content_ed;
            if (!parse_table_cell(testp, border, &testp, &colspan, &cell_ctrl_st, &cell_ctrl_ed, &cell_content_st, &cell_content_ed)) {
                goto failure;
            }
            if (is_first)
                is_first = false;

            // configure cell
            char *dummy;
            cell->colspan = colspan;
            cell->content = parse_inline(cell_content_st, cell_content_ed, &dummy, ctx);
            table_use_cell_ctrl(cell_ctrl_st, cell_ctrl_ed, is_first, table, row, cell);

            CONSUME_SPACETAB(testp, border);
        } while (!MET_EOF(testp, border) && *testp != '\n');
        SAFE_INC(testp, border); // consume '\n'
    } while (EQ(testp, border, '|'));
    *p_out = testp;
    return table;
failure:
    verbose_log("Table creation failed..\n");
    namuast_remove_table(table);
    return NULL;
}


static bool parse_block(char *p, char *border, char **p_out, struct nm_block_emitters* ops, struct namugen_ctx* ctx, struct namuast_inline *outer_inl) {
    if (EQ(p + 2, border, '{') && *(p + 1) == '{' && *p == '{') {
        char *lastp = p + 3;
        char *content_end_p;
        bool closing_braces_found = true;
        while (1) {
            UNTIL_REACHING1(lastp, border, '}') {
                lastp++;
            }
            if (MET_EOF(lastp, border)) {
                content_end_p = lastp;
                closing_braces_found  = false;
                break;
            } else if (EQ(lastp + 2, border, '}') && *(lastp + 1) == '}')  {
                content_end_p = lastp;
                lastp += 3;
                break;
            } else
                lastp += 2;
        }
        if (closing_braces_found) {
            char *testp = p + 3;
            if (EQ(testp, border, '+') && ops->emit_highlighted_block) {
                testp++;
                verbose_log("got highlight mode");
                if (!MET_EOF(testp, border) && *testp >= '0' && *testp <= '5') {
                    int highlight_level = *testp - '0';
                    verbose_log("got highlight level: %d\n", highlight_level);
                    testp++;
                    CONSUME_WHITESPACE(testp, border);
                    RCONSUME_SPACETAB(testp, border);
                    struct namuast_inline *content = parse_inline(testp, content_end_p, &testp, ctx);
                    if (ops->emit_highlighted_block(ctx, outer_inl, content, highlight_level)) {
                        *p_out = lastp;
                        return true;
                    }
                }
            } else if (EQ(testp, border, '#') && ops->emit_colored_block) {
                testp++;
                char *color_st = testp;
                UNTIL_REACHING3(testp, border, ' ', '\n', '\t') {
                    testp++;
                }
                char *color_ed = testp;
                CONSUME_SPACETAB(testp, content_end_p);
                RCONSUME_SPACETAB(testp, content_end_p);
                struct namuast_inline *content = parse_inline(testp, content_end_p, &testp, ctx);
                if (ops->emit_colored_block(ctx, outer_inl, content, dup_str(color_st, color_ed))) {
                    *p_out = lastp;
                    return true;
                }
                return lastp;
            } else if (PREFIXSTR(testp, border, "!html") && ops->emit_html) {
                testp += 5;
                CONSUME_SPACETAB(testp, content_end_p);
                RCONSUME_SPACETAB(testp, content_end_p);
                if (ops->emit_html(ctx, outer_inl, dup_str(testp, content_end_p))) {
                    *p_out = lastp;
                    return true;
                }
            } else if (ops->emit_raw) {
                // raw string
                CONSUME_SPACETAB(testp, content_end_p);
                RCONSUME_SPACETAB(testp, content_end_p);
                if (ops->emit_raw(ctx, outer_inl, dup_str(testp, content_end_p))) {
                    *p_out = lastp;
                    return true;
                }
            }
        } 
    }
    return false;
}

static inline char* unescape_link(char *s) {
    char *p;
    char *ret;
    char *ret_p;
    ret = ret_p = calloc(strlen(s) + 1, 1);
    for (p = s; *p; ) {
        if (*p == '\\' && *(p + 1)) {
            switch (*(p + 1)) {
            case '\\':
            case '|':
            case '[':
            case ']':
                *ret_p++ = *(p + 1);
                p += 2;
                break;
            default:
                *ret_p++ = *p++;
                break;
            }
        } else {
            *ret_p++ = *p++;
        }
    }
    free(s);
    return ret;
}

static bool strip_section(char *st, char *ed, char **link_out, char **section_out) {
    char *p;
    for (p = ed - 4; st < p; p--) {
        if (*p == '#' && *(p+1) == 's' && *(p+2) == '-') {
            *link_out = unescape_link(dup_str(st, p));
            *section_out = unescape_link(dup_str(p + 3, ed));
            return true;
        }
    }
    return false;
}
static void emit_internal_link(char *link_st, char *link_ed, char *alias, struct namuast_inline* inl) {
    char *link, *section;
    if (!strip_section(link_st, link_ed, &link, &section)) {
        link = unescape_link(dup_str(link_st, link_ed));
        section = NULL;
    }

    if  (*link == '/') {
        memmove(link, link + 1, strlen(link));
        nm_inl_emit_lower_link(inl, link, alias, section);
    } else if (!strcmp(link, "../")) {
        free(link);
        nm_inl_emit_upper_link(inl, alias, section);
    } else {
        nm_inl_emit_link(inl, link, alias, section);
    }
}

void namu_scan_link_content(char *p, char* border, char* pipe_pos, struct namugen_ctx* ctx, struct namuast_inline* inl) {
    char* alias = NULL;
    if (pipe_pos) {
        char *rpipe_pos = pipe_pos + 1;
        CONSUME_SPACETAB(rpipe_pos, border);
        alias = unescape_link(dup_str(rpipe_pos, border));
    }
    char* lpipe_pos = pipe_pos? pipe_pos : border;
    RCONSUME_SPACETAB(p, lpipe_pos);
    
    bool use_wiki = false;
    if (PREFIXSTR(p, lpipe_pos, "http://") || 
        PREFIXSTR(p, lpipe_pos, "https://") || 
        (use_wiki = PREFIXSTR(p, lpipe_pos, "wiki:")))  {

        char* testp = p;
        char *url_st, *url_ed;
        char *link_st, *link_ed;
        if (use_wiki) {
            testp += 5; // consume "wiki:"
            CONSUME_SPACETAB(testp, lpipe_pos);

            if (!EQ(testp, lpipe_pos, '\"')) {
                goto prefix_failure;
            }
            testp++;
            link_st = testp;
            UNTIL_REACHING1(testp, lpipe_pos, '\"') {
                testp++;
            }
            link_ed = testp;
            testp++; // consume "
        } else {
            url_st = testp;
            UNTIL_REACHING2(testp, lpipe_pos, ' ', '\t') {
                testp++;
            } 
            url_ed = testp;
        }

        CONSUME_SPACETAB(testp, lpipe_pos);
        // old-style alias
        if (!MET_EOF(testp, lpipe_pos)) {
            if (alias) {
                free(alias);
            }
            alias = dup_str(testp, lpipe_pos);
        }

        if (use_wiki) {
            emit_internal_link(link_st, link_ed, alias, inl);
        } else {
            nm_inl_emit_external_link(inl, dup_str(url_st, url_ed), alias);
        }
        return;
    }
prefix_failure:
    emit_internal_link(p, pipe_pos? pipe_pos : border, alias, inl);
}

static inline int get_span_type_from_double_mark(char mark) {
    switch (mark) {
        case '\'':
            return nm_span_italic;
        case '~':
        case '-':
            return nm_span_strike;
        case '_':
            return nm_span_underline;
        case '^':
            return nm_span_superscript;
        case ',':
            return nm_span_subscript;
        default:
            return nm_span_none;
    }
}

char *supported_photo_exts[] = { 
    "jpg",
    "jpeg",
    "png",
    "gif",
    NULL
};

static inline bool parse_list_header (char *p, char *border, char **content_st_out, int *list_type_out, int* indent_level_out) {
    int list_type;

    char *testp = p;
    char *ind_st = testp;
    CONSUME_SPACETAB(testp, border);
    char *ind_ed = testp;

    bool is_list = true;
    if (EQ(testp, border, '*') || EQ(testp + 1, border, '.')) {
        switch (*testp) {
        case '*':
            testp += 1;
            list_type = nm_list_unordered;
            break;
        case 'a':
            testp += 2;
            list_type = nm_list_lower_alpha;
            break;
        case 'A':
            testp += 2;
            list_type = nm_list_upper_alpha;
            break;
        case 'i':
            testp += 2;
            list_type = nm_list_lower_roman;
            break;
        case 'I':
            testp += 2;
            list_type = nm_list_upper_roman;
            break;
        case '1':
            testp += 2;
            list_type = nm_list_ordered;
        default:
            is_list = false;
        }
    } else
        is_list = false;

    CONSUME_SPACETAB(testp, border);
    if (!is_list) {
        list_type = nm_list_indent;
    }

    *content_st_out = testp;
    *list_type_out = list_type;
    *indent_level_out = ind_ed - ind_st;
    return is_list;
}


static inline struct namuast_inline* parse_inline(char *p, char* border, char **p_out, struct namugen_ctx* ctx) {
    int cur_span_type = nm_span_none;

    struct namuast_inline *inl = namuast_make_inline(ctx);
    UNTIL_REACHING1(p, border, '\n') {
        switch (*p) {
        case '{':
            {
                char *lastp;
                if (parse_block(p, border, &lastp, &block_emitter_ops_inline, ctx, inl)) {
                    p = lastp;
                } else
                    goto scan_literally;
            }
            break;
        case 'h':
            // https?://.*(.jpg|.jpeg|.png|.gif|?.jpg)
            {
                char *url_st, *url_ed;

                url_st = p;
                if (!PREFIXSTR(p + 1, border, "ttp")) goto scan_literally;
                char *testp = p + 4;
                if (EQ(testp, border, 's'))
                    testp++;
                if (!PREFIXSTR(testp, border, "://")) goto scan_literally;
                testp += 3;

                bool is_genuine_url = false;

                // detecting file extension
                UNTIL_REACHING3(testp, border, ' ', '\t', '\n') {
                    if (*testp == '.') {
                        if (*(testp - 1) == '?' && CASEPREFIXSTR(testp + 1, border, "jpg")) {
                            url_ed = testp - 1;
                            testp += 4;
                            is_genuine_url = true;
                            break;
                        }
                        char **p_ext;
                        bool stop_loop = false;
                        for (p_ext = supported_photo_exts; *p_ext; p_ext++) {
                            if (CASEPREFIXSTR(testp + 1, border, *p_ext)) {
                                testp += 1 + strlen(*p_ext);
                                url_ed = testp;
                                is_genuine_url = true;
                                stop_loop = true;
                                break;
                            }
                        }
                        if (stop_loop)
                            break;
                    }
                    testp++;
                }
                if (!is_genuine_url) goto scan_literally;

                char *width = NULL, *height = NULL;
                int align = nm_align_none;

                // now parse the option
                if (EQ(testp, border, '?')) {
                    testp++;

                    while (1) {
                        char *key_st, *key_ed;
                        char *value_st, *value_ed;
                        key_st = testp;
                        CONSUME_SPACETAB(testp, border);
                        UNTIL_REACHING2(testp, border, '=', '\n') {
                            testp++;
                        }
                        if (!EQ(testp, border, '=')) break;
                        key_ed = testp;
                        if (key_st == key_ed) 
                            goto scan_literally;
                        else
                            RCONSUME_SPACETAB(key_st, key_ed);
                        testp++; // consume '='

                        CONSUME_SPACETAB(testp, border);
                        value_st = testp;
                        UNTIL_REACHING4(testp, border, '&', '\n', ' ', '\t') {
                            testp++;
                        }
                        value_ed = testp;
                        RCONSUME_SPACETAB(value_st, value_ed);

                        CONSUME_SPACETAB(testp, border);

                        // now set up options, though it's a hard wiring hack...
                        if (EQSTR(key_st, key_ed, "width")) {
                            if (width) free(width);
                            width = dup_str(value_st, value_ed);
                        } else if (EQSTR(key_st, key_ed, "height")) {
                            if (height) free(height);
                            height = dup_str(value_st, value_ed);
                        } else if (EQSTR(key_st, key_ed, "align")) {
                            if (EQSTR(value_st, value_ed, "left")) {
                                align = nm_align_left;
                            } else if (EQSTR(value_st, value_ed, "right")) {
                                align = nm_align_right;
                            } else if (EQSTR(value_st, value_ed, "center")) {
                                align = nm_align_center;
                            }
                        }

                        if (EQ(testp, border, '&')) {
                            testp++; // consume '&'
                            continue;
                        }
                        break;
                    }
                }
                char *url = dup_str(url_st, url_ed);
                nm_inl_emit_image(inl, url, width, height, align);
                p = testp;
            }
            break;
        case '\'':
            if (EQ(p + 2, border, '\'') && *(p + 1) == '\'') {
                const char span_mark = '\'';
                const int mark_type = nm_span_bold;

                char* testp = p;
                testp += 3;

                char *content_st = testp;
                while (!MET_EOF(testp + 3, border) && *testp != '\n' && !(*(testp + 2) == span_mark && *(testp + 1) == span_mark && *testp == span_mark)) {
                    testp++;
                }
                if (MET_EOF(testp + 3, border) || *testp == '\n')
                    goto scan_literally;
                char *content_ed = testp;
                testp += 3;
                char *dummy;

                CONSUME_SPACETAB(content_st, content_ed);
                RCONSUME_SPACETAB(content_st, content_ed);
                struct namuast_inline *span = parse_inline(content_st, content_ed, &dummy, ctx);
                nm_inl_emit_span(inl, span, mark_type);
                p = testp;
                break;
            }
            // falling through intended
        case '~':
        case '-':
        case '_':
        case '^':
        case ',':
            {
                char span_mark = *p;
                char* testp = p;
                int mark_type = get_span_type_from_double_mark(span_mark);
                if (!EQ(testp + 1, border, span_mark))
                    goto scan_literally;
                testp += 2;
                char *content_st = testp;
                while (!MET_EOF(testp + 1, border) && *testp != '\n' && !(*(testp + 1) == span_mark && *testp ==  span_mark)) {
                    testp++;
                }
                if (MET_EOF(testp + 1, border) || *testp == '\n')
                    goto scan_literally;
                char *content_ed = testp;
                testp += 2;
                char *dummy;

                CONSUME_SPACETAB(content_st, content_ed);
                RCONSUME_SPACETAB(content_st, content_ed);
                struct namuast_inline *span = parse_inline(content_st, content_ed, &dummy, ctx);
                nm_inl_emit_span(inl, span, mark_type);
                p = testp;
            }
            break;
        case ']':
            if (!nm_in_footnote(ctx)) {
                goto scan_literally;
            } else {
                p++;
                nm_end_footnote(ctx);
                goto exit;
            }
            break;
        case '[':
            {
                // parse as footnote
                if (EQ(p + 1, border, '*')) {
                    if (nm_in_footnote(ctx)) {
                        goto scan_literally;
                    }
                    p += 2;
                    char *extra_st = p;
                    UNTIL_REACHING4(p, border, ' ', '\n', '\t', ']') {
                        p++;
                    }
                    char *extra_ed = p;

                    CONSUME_WHITESPACE(p, border);
                    char *extra;
                    if (extra_st == extra_ed) {
                        extra = NULL;
                    } else
                        extra = dup_str(extra_st, extra_ed);
                    char *fnt_end_p;

                    nm_begin_footnote(ctx);
                    struct namuast_inline *footnote = parse_inline(p, border, &fnt_end_p, ctx);

                    if (!EQ(fnt_end_p - 1, border, ']')) {
                    // '[*' prefix pretented to be a footnote
                        namuast_remove_inline(footnote); 
                        if (extra) free(extra);
                        goto scan_literally;
                    }
                    int id = nm_register_footnote(ctx, footnote, extra);
                    nm_inl_emit_footnote_mark(inl, id, ctx);
                    p = fnt_end_p;
                    goto exit;
                }

                // parse as link
                char *testp = p;
                bool compatible_mode;
                testp++;
                if (EQ(testp, border, '[')) {
                    compatible_mode = false;
                    testp++;
                } else {
                    compatible_mode = true;
                }
                char *link_st, *link_ed;
                char *pipe_pos = NULL;

                CONSUME_SPACETAB(testp, border);
                link_st = testp;
                while (1) {
                    UNTIL_REACHING2(testp, border, ']', '\n') {
                        bool escape_next_char = false; 
                        switch (*testp) {
                        case '|':
                            pipe_pos = testp;
                            break;
                        case '\\':
                            if (!MET_EOF(testp + 1, border)) {
                                switch (*(testp + 1)) {
                                case '\\':
                                case '[':
                                case '|':
                                case ']':
                                    escape_next_char = true;
                                    break;
                                }
                            }
                        }
                        if (escape_next_char)
                            testp += 2;
                        else
                            testp++;
                    }
                    if (MET_EOF(testp, border) || *testp == '\n') {
                        goto scan_literally;
                    } else if (compatible_mode) {
                        link_ed = testp;
                        testp++;
                        break;
                    } else if (EQ(testp + 1, border, ']')) {
                        link_ed = testp;
                        testp += 2;
                        break;
                    }
                }
                // get rid of spaces on the right side of link
                RCONSUME_SPACETAB(link_st, link_ed);
                namu_scan_link_content(link_st, link_ed, pipe_pos, ctx, inl);
                p = testp;
            }
            break;
        default:
            goto scan_literally;
        }
        continue;
scan_literally:
        nm_inl_emit_char(inl, *p++);
    }
exit:
    *p_out = p;
    return inl;
}



static inline char* namu_scan_main(char *p, char* border, struct namugen_ctx* ctx) {
    if (MET_EOF(p, border))
        return border;
    // supose we are at the begining of line
    switch (*p) {
    case '=':
        {
            int h_num = 1;
            char *testp = p;
            testp++;
            UNTIL_NOT_REACHING1(testp, border, '=') {
                h_num++;
                testp++;
            }
            if (MET_EOF(testp, border))
                break;


            CONSUME_SPACETAB(testp, border);

            char *content_start_p = testp;
            char *content_end_p;
            int cls_num = 0;
            UNTIL_REACHING1(testp, border, '\n') {
                cls_num = 0;
                content_end_p = NULL;
                UNTIL_REACHING2(testp, border, '=', '\n') {
                    testp++;
                }
                if (MET_EOF(testp, border) || *testp == '\n') {
                    break;
                }
                content_end_p = testp;
                UNTIL_NOT_REACHING1(testp, border, '=') {
                    testp++;
                    cls_num++;
                }
                CONSUME_SPACETAB(testp, border);
            }
            char *lastp = testp; // exclusive
            if (content_end_p && cls_num == h_num && h_num <= MAX_HEADING_NUMBER) {
                RCONSUME_SPACETAB(content_start_p, content_end_p);
                // verbose_log("emit heading%d, %s:%s \n", h_num, content_start_p, content_end_p);
                char *dummy;
                struct namuast_inline *inl = parse_inline(content_start_p, content_end_p, &dummy, ctx);
                nm_emit_heading(ctx, h_num, inl);
                return lastp;
            } else 
                verbose_log("heading: %d != %d\n", h_num, cls_num);
        }
        break;

    case '{':
        {
            char *lastp;
            if (parse_block(p, border, &lastp, &block_emitter_ops_paragraphic, ctx, NULL)) {
                return lastp;
            }
        }
        break;
    case '#':
        if (EQ(p + 1, border, '#')) {
            char *testp = p + 2;
            UNTIL_REACHING1(testp, border, '\n') {
                testp++;
            }
            SAFE_INC(testp, border); // consume \n
            return testp;
        }
        break;
    case '>':
        // quotation
        {
            char *testp = p;
            testp++;
            UNTIL_NOT_REACHING1(testp, border, '>') {
                testp++;
            }
            CONSUME_SPACETAB(testp, border);
            nm_emit_quotation(ctx, parse_inline(testp, border, &testp, ctx));
            return testp;
        }
        break;
    case '|':
        {
            char *p_ret;
            struct namuast_table *table = parse_table(p, border, &p_ret, ctx);
            if (table) {
                nm_emit_table(ctx, table);
                return p_ret;
            }
        }
        break;
    case '-':
        {
            char *testp = p + 1;
            int num = 1;
            UNTIL_NOT_REACHING1(testp, border, '-') {
                testp++;
                num++;
            }
            if (num >= 3 && num <= 10) {
                nm_emit_hr(ctx);
                return testp;
            }
        }
        break;
    case ' ':
        // test if it is a list and/or an indent
        {
            char *testp = p;
            int list_type;
            int indent_level;

            struct {
                struct namuast_list* head;
                struct namuast_list* tail;
                int indent_level;
            } stack[MAX_LIST_STACK_SIZE];
            int stack_top = 0;

            struct namuast_list* result_list;
#define MAKE_LIST(p, lt) namuast_make_list(lt, parse_inline(p, border, &p, ctx))
#define FLUSH_STACK(ind, dangling_list) { \
    dangling_list = NULL; \
    while (stack_top > 0 && stack[stack_top - 1].indent_level > ind)  { \
        struct namuast_list *sublist = stack[stack_top - 1].head; \
        stack_top--; \
        if (stack_top > 0) { \
            struct namuast_list **p_sub_list = &stack[stack_top - 1].tail->sublist; \
            if (*p_sub_list) { \
                namuast_remove_list(*p_sub_list); \
            } \
            *p_sub_list = sublist; \
        } else { \
            dangling_list = sublist; \
            break; \
        } \
    } \
}
            parse_list_header(testp, border, &testp, &list_type, &indent_level);
            stack[stack_top].head = stack[stack_top].tail = MAKE_LIST(testp, list_type);
            stack[stack_top].indent_level = indent_level;
            stack_top++;

            while (EQ(testp, border, '\n')) {
                testp++; // consume '\n'

                char *next_testp = testp;
                parse_list_header(next_testp, border, &next_testp, &list_type, &indent_level);
                if (indent_level == 0)
                    break;

                struct namuast_list *dangling_list = NULL;
                FLUSH_STACK(indent_level, dangling_list);

                if (stack_top < MAX_LIST_STACK_SIZE) {
                    if (stack_top > 0 && stack[stack_top - 1].indent_level == indent_level) {
                        if (stack[stack_top - 1].tail->type != list_type) {
                            list_type = stack[stack_top - 1].tail->type;
                        }
                        struct namuast_list* tail = stack[stack_top - 1].tail;
                        tail->next = MAKE_LIST(next_testp, list_type);
                        stack[stack_top - 1].tail = tail->next;
                    } else {
                        stack[stack_top].head = stack[stack_top].tail = MAKE_LIST(next_testp, list_type);
                        stack[stack_top].indent_level = indent_level;
                        stack_top++;
                    }
                }
                if (dangling_list) {
                    if (dangling_list->type != list_type)
                        dangling_list->type = list_type;
                    dangling_list->next = stack[stack_top - 1].head;
                    stack[stack_top - 1].head = dangling_list;
                }
                testp = next_testp;
            }
            FLUSH_STACK(-1, result_list);
            nm_emit_list(ctx, result_list);
            return testp;
#undef MAKE_INLINE
#undef STACK_INSERT
#undef FLUSH_STACK
        }
        break;
    case '\n':
        nm_emit_return(ctx);
        return p + 1;
    }

    char *retval;
    nm_emit_inline(ctx, parse_inline(p, border, &retval, ctx));
    // verbose_log("inline chunk: ");
    // verbose_str(p, retval);
    return retval;
}

void namugen_scan(struct namugen_ctx *ctx, char *buffer, size_t len) {
    char *border = buffer + len;
    char *p = buffer;
    while (!MET_EOF(p, border)) {
        p = namu_scan_main(p, border, ctx);
    }
}
