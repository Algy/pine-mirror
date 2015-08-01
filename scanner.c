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

bndstr to_bndstr(char *st, char *ed) {
    bndstr result = {st, ed - st};
    return result;
}


struct webcolor_name {
    char *name;
    char *webcolor; // don't free me!
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

char* find_webcolor_by_name(char *name, size_t len) {
    struct webcolor_name *wnm;
    for (wnm = webcolor_nametbl; wnm->name; wnm++) {
        if (!strncasecmp(wnm->name, name, len)) {
            return wnm->webcolor;
        }
    }
    return NULL;
}



static inline struct namuast_inl_container* parse_multiline(char *p, char* border, struct namugen_ctx* ctx);

struct namuast_list* namuast_make_list(int type, struct namuast_inl_container* content) {
    struct namuast_list* retval = (struct namuast_list*)NEW_NAMUAST(namuast_type_list);
    retval->type = type;
    retval->content = content;
    return retval;
}

void _namuast_dtor_list(struct namuast_base *base) {
    struct namuast_list* lt = (struct namuast_list *)base;
    struct namuast_list* sibling;
    for (sibling = lt; sibling; ) {
        if (sibling->content)
            RELEASE_NAMUAST(sibling->content);
        if (sibling->sublist)
            RELEASE_NAMUAST(sibling->sublist);
        struct namuast_list* next = sibling->next;
        if (sibling != lt) {
            free(sibling); // HACK
        }
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
    struct namuast_table* table = (struct namuast_table*)NEW_NAMUAST(namuast_type_table);
    table->row_size = 8;
    table->row_count = 0;
    table->align = nm_align_none;
    table->border_webcolor = NULL;
    table->width = NULL;
    table->height = NULL;
    table->bg_webcolor = NULL;
    table->caption = NULL;
    table->max_col_count = 0;
    table->rows = calloc(table->row_size, sizeof(struct namuast_table_row));
    return table;
}

void _namuast_dtor_table(struct namuast_base *base) {
    struct namuast_table* table = (struct namuast_table*)base;
    size_t row_idx;
    for (row_idx = 0; row_idx < table->row_count; row_idx++) {
        struct namuast_table_row *row = &table->rows[row_idx];
        size_t col_idx;
        for (col_idx = 0; col_idx < row->col_count; col_idx++) {
            struct namuast_table_cell *cell = &row->cols[col_idx];
            if (cell->content) {
                RELEASE_NAMUAST(cell->content);
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
    if (table->caption) RELEASE_NAMUAST(table->caption);
    free(table->rows);
}

char* dup_str(char *st, char *ed) {
    char *retval = calloc(ed - st + 1, sizeof(char));
    memcpy(retval, st, ed - st);
    return retval;
}


/*
typedef struct {
    char *tagname; // may be NULL
    size_t keyvalue_count;
    char **keys; // may be NULL only if keyvalue_count is 0
    char **values; // may be NULL only if keyvalue_count is 0
    char *after_pipe; // may be NULL
} celltag;
*/

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

/*
static void remove_celltag(celltag *tag) {
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
}
*/

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
    int colspan = dup_pipes / 2 + 1;

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
            testp++; // consume '>'
        }
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
     * ^|(.|\n)*|(||)* \s* <(.|\n)*> \s* CONTENT \s* ( ( (||)+ <.*> CONTENT)* || \n )+
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
        table->caption = parse_multiline(caption_start, caption_end, ctx);
    }

    while (1) {
        struct namuast_table_row* row = namuast_add_table_row(table);
        bool is_first = true;
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
            cell->colspan = colspan;
            cell->content = parse_multiline(cell_content_st, cell_content_ed, ctx);
            table_use_cell_ctrl(cell_ctrl_st, cell_ctrl_ed, is_first, table, row, cell);

            CONSUME_SPACETAB(testp, border);
        } while (!MET_EOF(testp, border) && *testp != '\n');
        SAFE_INC(testp, border); // consume '\n'

        if (EQ(testp + 1, border, '|') && *testp == '|') {
            testp += 2;
            continue;
        }
        break;
    }
    *p_out = testp;
    return table;
failure:
    verbose_log("Table creation failed..\n");
    RELEASE_NAMUAST(table);
    return NULL;
}


bool scn_parse_block(char *p, char *border, char **p_out, struct nm_block_emitters* ops, struct namugen_ctx* ctx, struct namuast_inl_container *container) {
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
            if (EQ(testp, border, '+') ) {
                if (ops->emit_highlighted_block) {
                    testp++;
                    verbose_log("got highlight mode");
                    if (!MET_EOF(testp, border) && *testp >= '1' && *testp <= '5') {
                        int highlight_level = *testp - '0';
                        verbose_log("got highlight level: %d\n", highlight_level);
                        testp++;
                        CONSUME_WHITESPACE(testp, border);
                        RCONSUME_SPACETAB(testp, border);
                        namuast_inl_container *content = scn_parse_inline(make_inl_container(), testp, content_end_p, &testp, ctx);
                        if (ops->emit_highlighted_block(ctx, container, content, highlight_level)) {
                            *p_out = lastp;
                            return true;
                        }
                    }
                }
            } else if (EQ(testp, border, '#')) {
                testp++;
                char *color_st = testp;
                UNTIL_REACHING3(testp, border, ' ', '\n', '\t') {
                    testp++;
                }
                char *color_ed = testp;
                if (EQSTR(color_st, color_ed, "!html")) {
                    if (ops->emit_html) {
                        CONSUME_SPACETAB(testp, content_end_p);
                        RCONSUME_SPACETAB(testp, content_end_p);
                        bndstr html = {testp, content_end_p - testp};
                        if (ops->emit_html(ctx, container, html)) {
                            *p_out = lastp;
                            return true;
                        }
                    }
                } else if (ops->emit_colored_block) {
                    char *colorname;
                    char colorbuf[64];
                    bndstr webcolor;
                    if ((colorname = find_webcolor_by_name(color_st, color_ed - color_st))) {
                        webcolor.str = colorname;
                        webcolor.len = strlen(colorname);
                    } else {
                        size_t color_len = color_ed - color_st < 62? color_ed - color_st : 62;
                        colorbuf[0] = '#';
                        memcpy(colorbuf + 1, color_st, sizeof(char) * color_len);
                        colorbuf[color_len + 1] = 0;
                        webcolor.str = colorbuf;
                        webcolor.len = color_len + 1;
                    }
                    CONSUME_SPACETAB(testp, content_end_p);
                    RCONSUME_SPACETAB(testp, content_end_p);
                    struct namuast_inl_container *content = scn_parse_inline(make_inl_container(), testp, content_end_p, &testp, ctx);
                    if (ops->emit_colored_block(ctx, container, content, webcolor)) {
                        *p_out = lastp;
                        return true;
                    }
                    return lastp;
                }
            } else if (ops->emit_raw) {
                // raw string
                CONSUME_SPACETAB(testp, content_end_p);
                RCONSUME_SPACETAB(testp, content_end_p);
                if (ops->emit_raw(ctx, container, (bndstr){testp, content_end_p - testp})) {
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
    ret = ret_p = calloc(strlen(s) + 1, sizeof(char));
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
static void emit_internal_link(char *link_st, char *link_ed, namuast_inl_container *alias, struct namuast_inl_container* container, struct namugen_ctx *ctx) {
    char *link, *section;
    if (!strip_section(link_st, link_ed, &link, &section)) {
        link = unescape_link(dup_str(link_st, link_ed));
        section = NULL;
    }

    bndstr bnd_link = {link, strlen(link)};
    bndstr bnd_section = {0, 0};
    if (section) {
        bnd_section.str = section;
        bnd_section.len = strlen(section);
    }

    if  (*link == '/') {
        memmove(link, link + 1, strlen(link));
        nm_inl_emit_lower_link(container, ctx, bnd_link, alias, bnd_section);
    } else if (!strcmp(link, "../")) {
        nm_inl_emit_upper_link(container, ctx, alias, bnd_section);
    } else {
        nm_inl_emit_link(container, ctx, bnd_link, alias, bnd_section);
    }
    free(link);
    free(section);
}

void scn_parse_link_content(char *p, char* border, char* pipe_pos, struct namugen_ctx* ctx, struct namuast_inl_container* container) {
    char *dummy;
    namuast_inl_container* alias_subinl = NULL;
    if (pipe_pos) {
        char *rpipe_pos = pipe_pos + 1;
        CONSUME_SPACETAB(rpipe_pos, border);
        alias_subinl = scn_parse_inline(make_inl_container(), rpipe_pos, border, &dummy, ctx);
    }
    char* lpipe_pos = pipe_pos? pipe_pos : border;
    RCONSUME_SPACETAB(p, lpipe_pos);
    
    bool use_wiki = false;
    bool use_dquote = false;
    if (PREFIXSTR(p, lpipe_pos, "http://") || 
        PREFIXSTR(p, lpipe_pos, "https://") || 
        (use_wiki = PREFIXSTR(p, lpipe_pos, "wiki:")) ||
        (use_dquote = (*p == '\"')))  {

        char* testp = p;
        char *url_st, *url_ed;
        char *link_st, *link_ed;
        if (use_wiki || use_dquote) {
            if (use_wiki) {
                testp += 5; // consume "wiki:"
                CONSUME_SPACETAB(testp, lpipe_pos);
            }
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
            if (alias_subinl) {
                RELEASE_NAMUAST(alias_subinl);
            }
            alias_subinl = scn_parse_inline(make_inl_container(), testp, lpipe_pos, &dummy, ctx);
        }
        if (use_wiki || use_dquote) {
            emit_internal_link(link_st, link_ed, alias_subinl, container, ctx);
        } else {
            bndstr url = {url_st, url_ed - url_st};
            nm_inl_emit_external_link(container, url, alias_subinl);
        }
        return;
    }
prefix_failure:
    emit_internal_link(p, pipe_pos? pipe_pos : border, alias_subinl, container, ctx);
}

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

static inline struct namuast_inl_container* parse_multiline(char *p, char* border, struct namugen_ctx* ctx) {
    struct namuast_inl_container *container = make_inl_container();
    scn_parse_inline(container, p, border, &p, ctx);

    UNTIL_NOT_REACHING1(p, border, '\n') {
        p++;
        inl_container_add_return(container, ctx);
    }

    while (!MET_EOF(p, border)) {
        scn_parse_inline(container, p, border, &p, ctx);
        UNTIL_NOT_REACHING1(p, border, '\n') {
            p++;
            inl_container_add_return(container, ctx);
        }
    }
    return container;
}


#define CONSUME_IF_ENDL(p, border) if (EQ(p, border, '\n')) { (p)++; }

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
            char *content_end_p = NULL;
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
                CONSUME_IF_ENDL(lastp, border);
                char *dummy;
                struct namuast_inl_container *content = scn_parse_inline(make_inl_container(), content_start_p, content_end_p, &dummy, ctx);
                nm_emit_heading(ctx, h_num, content);
                return lastp;
            } else 
                verbose_log("heading: %d != %d\n", h_num, cls_num);
        }
        break;

    case '{':
        {
            char *lastp;
            if (scn_parse_block(p, border, &lastp, &block_emitter_ops_paragraphic, ctx, NULL)) {
            CONSUME_IF_ENDL(lastp, border);
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
            struct namuast_inl_container *container = make_inl_container();
            char *testp = p;
            while (testp < border && *testp == '>') {
                UNTIL_NOT_REACHING1(testp, border, '>') {
                    testp++;
                }
                CONSUME_SPACETAB(testp, border);
                scn_parse_inline(container, testp, border, &testp, ctx);
                if (EQ(testp, border, '\n')) {
                    inl_container_add_return(container, ctx);
                    testp++;
                }
            }
            nm_emit_quotation(ctx, container);
            return testp;
        }
        break;
    case '|':
        {
            char *p_ret;
            struct namuast_table *table = parse_table(p, border, &p_ret, ctx);
            if (table) {
                nm_emit_table(ctx, table);
                CONSUME_IF_ENDL(p_ret, border);
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
            if (num >= 4 && num <= 10) {
                nm_emit_hr(ctx, num);
                CONSUME_IF_ENDL(testp, border);
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
#define MAKE_LIST(p, lt) namuast_make_list(lt, scn_parse_inline(make_inl_container(), p, border, &p, ctx))
#define FLUSH_STACK(ind, dangling_list) { \
    dangling_list = NULL; \
    while (stack_top > 0 && stack[stack_top - 1].indent_level > ind)  { \
        struct namuast_list *sublist = stack[stack_top - 1].head; \
        stack_top--; \
        if (stack_top > 0) { \
            struct namuast_list **p_sub_list = &stack[stack_top - 1].tail->sublist; \
            if (*p_sub_list) { \
                RELEASE_NAMUAST(*p_sub_list); \
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
            CONSUME_IF_ENDL(testp, border);
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
    nm_emit_inline(ctx, scn_parse_inline(make_inl_container(), p, border, &retval, ctx));
    // verbose_log("inline chunk: ");
    // verbose_str(p, retval);
    return retval;
}

void namugen_scan(struct namugen_ctx *ctx, char *buffer, size_t len) {
    char *border = buffer + len;
    char *p = buffer;
    nm_on_start(ctx);
    while (!MET_EOF(p, border)) {
        p = namu_scan_main(p, border, ctx);
    }
    nm_on_finish(ctx);
}
