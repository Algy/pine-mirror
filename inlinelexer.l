%{
#include <stdio.h>

#include "namugen.h"

typedef struct {
    size_t num_chars;
    struct namuast_inl_container* container;
    struct namugen_ctx *ctx;
} InlineScannerExtra;

static void as_span(InlineScannerExtra* extra, char* st, char* ed, enum nm_span_type type);
static void as_link(InlineScannerExtra* extra, char* st, char* ed);
static void as_macro(InlineScannerExtra *extra, char* st, char *ed);
#define TRACK_NUM_CHARS yyextra->num_chars += yyleng

%}

letter [a-zA-Z0-9_]
image_ext ("jpg"|"jpeg"|"png"|"gif")
image_option ("width"|"height"|"align")
normal_char [^\n[\]'{~\-_^,h]
escapable_char_in_link [|[\]]
not_a_rbrk [^\]\n]

%option noyywrap
%option reentrant
%option extra-type="InlineScannerExtra *"

%%

"[*"([^[\]\n]|("["[^\]\n]*"]")|("[["("]"?[^\]\n])*"]]"))*"]" {
    if (nm_in_footnote(yyextra->ctx)) {
        bndstr s = {yytext, yyleng};
        nm_inl_emit_str(yyextra->container, s);
    } else {
        char *testp;
        char *border = yytext + yyleng - 1;
        char *extra_st = yytext + 2;
        testp = yytext + 2;
        UNTIL_REACHING2(testp, border, ' ', '\t') {
            testp++;
        }
        char *extra_ed = testp;

        CONSUME_SPACETAB(testp, border);

        bndstr head = {extra_st, extra_ed - extra_st};

        char *fnt_st_p;
        char *dummy;

        fnt_st_p = testp;
        nm_begin_footnote(yyextra->ctx);

        struct namuast_inl_container *footnote_content = make_inl_container();  
        scn_parse_inline(footnote_content, testp, border, &dummy, yyextra->ctx);
        nm_end_footnote(yyextra->ctx);

        int id = nm_register_footnote(yyextra->ctx, footnote_content, head);
        bndstr s = {fnt_st_p, border - fnt_st_p};
        nm_inl_emit_footnote_mark(yyextra->container, yyextra->ctx, id, s);
    }
    TRACK_NUM_CHARS;
}

"[["(("\\"{escapable_char_in_link})|("]"|""){not_a_rbrk})*"]]" {
    as_link(yyextra, yytext + 2, yytext + yyleng - 2);
    TRACK_NUM_CHARS;
}

"["(("\\"{escapable_char_in_link})|{not_a_rbrk})*"]" {
    as_macro(yyextra, yytext + 1, yytext + yyleng - 1);
    TRACK_NUM_CHARS;
}

"http""s"?"://"[^ \n]*("."{image_ext}|"?.jpg")("?"({image_option}"="{letter}+)("&"{image_option}"="{letter}+)*)? {
    char *border = yytext + yyleng;
    bndstr width = {0, 0}, height = {0, 0};
    int align = nm_align_none;

    char* url_st, *url_ed;
    url_st = yytext;
    url_ed = border;
    // now parse the option
    bool qm_found = false;
    char *testp;
    if ((testp = strnstr(yytext, "?", yyleng))) {
        url_ed = testp;
        testp++;
        if (!strncmp(testp, ".jpg", 4)) {
            testp += 4;
            if ((testp = strnstr(testp, "?", border - testp))) {
                qm_found = true;
                testp++;
            }
        } else 
            qm_found = true;
    }
    if (qm_found) {
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
                width = (bndstr){value_st, value_ed - value_st};
            } else if (EQSTR(key_st, key_ed, "height")) {
                height = (bndstr){value_st, value_ed - value_st};
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

    bndstr url = {url_st, url_ed - url_st};
    nm_inl_emit_image(yyextra->container, url, width, height, align);
    TRACK_NUM_CHARS;
}

"{{{"(("}}"|"}"|"")[^}\n])*"}}}" {
    char *dummyp;
    scn_parse_block(yytext, yytext + yyleng, &dummyp, &block_emitter_ops_inline, yyextra->ctx, yyextra->container);
    TRACK_NUM_CHARS;
}

"\'\'\'"(("\'\'"|"\'"|"")[^'\n])+"\'\'\'" {
    as_span(yyextra, yytext + 3, yytext + yyleng - 3, nm_span_bold);
    TRACK_NUM_CHARS;
}

"\'\'"(("\'"|"")[^'\n])+"\'\'" {
    as_span(yyextra, yytext + 2, yytext + yyleng - 2, nm_span_italic);
    TRACK_NUM_CHARS;
}

"~~"(("~"|"")[^~\n])+"~~" |
"--"(("-"|"")[^-\n])+"--" {
    as_span(yyextra, yytext + 2, yytext + yyleng - 2, nm_span_strike);
    TRACK_NUM_CHARS;
}

"__"(("_"|"")[^_\n])+"__" {
    as_span(yyextra, yytext + 2, yytext + yyleng - 2, nm_span_underline);
    TRACK_NUM_CHARS;
}

"^^"(("^"|"")[^^\n])+"^^" {
    as_span(yyextra, yytext + 2, yytext + yyleng - 2, nm_span_superscript);
    TRACK_NUM_CHARS;
}

",,"((","|"")[^,\n])+",," {
    as_span(yyextra, yytext + 2, yytext + yyleng - 2, nm_span_subscript);
    TRACK_NUM_CHARS;
}


{normal_char}+ {
    bndstr s = {yytext, yyleng};
    nm_inl_emit_str(yyextra->container, s);
    TRACK_NUM_CHARS;
}


\n {
    return 0;
}
<<EOF>> {
    return 0;
}

. {
    bndstr s = {yytext, 1};
    nm_inl_emit_str(yyextra->container, s);
    TRACK_NUM_CHARS;
}

%%
static void as_span(InlineScannerExtra* extra, char* st, char* ed, enum nm_span_type type) {
    char *dummy;
    CONSUME_SPACETAB(st, ed);
    RCONSUME_SPACETAB(st, ed);
    struct namuast_inl_container *span_container = make_inl_container();
    scn_parse_inline(span_container, st, ed, &dummy, extra->ctx);
    nm_inl_emit_span(extra->container, span_container, type);
}

static void as_link(InlineScannerExtra* extra, char* st, char* ed) {
    CONSUME_SPACETAB(st, ed);
    RCONSUME_SPACETAB(st, ed);

    char *p;
    for (p = ed - 1; p >= st; ) {
        if (p - 1 >= st && *(p - 1) == '\\' && *p == '|')
            p -= 2;
        else if (*p == '|')
            break;
        else
            p--;
    }
    if (p < st)
        p = NULL;
    char* pipe_pos = p;
    scn_parse_link_content(st, ed, pipe_pos, extra->ctx, extra->container);
}

static void as_macro(InlineScannerExtra *extra, char* st, char *ed) {
    bndstr raw = {st, ed - st};
    bndstr name;
    size_t pos_args_len;
    size_t kw_args_len;

    char *testp = st;
    UNTIL_REACHING1(testp, ed, '(') {
        testp++;
    }
    name.str = st;
    name.len = testp - st;

    pos_args_len = 0;
    kw_args_len = 0;

    if (EQ(testp, ed, '(')) {
        testp++;
        char *lpar_st = testp;
        UNTIL_REACHING1(testp, ed, ')') {
            UNTIL_REACHING2(testp, ed, ',', ')') {
                UNTIL_REACHING3(testp, ed, ',', '=', ')') {
                    testp++;
                }
                if (EQ(testp, ed, '=')) {
                    testp++;
                    UNTIL_REACHING2(testp, ed, ',', ')') {
                        testp++;
                    }
                    kw_args_len++;
                } else {
                    pos_args_len++;
                }
            }
            if (testp >= ed) {
                goto not_a_fn_macro;
            } else if (EQ(testp, ed, ',')) {
                testp++;
            }
        }

        // now it is assured that this syntax is a function-style macro
        bndstr pos_args[pos_args_len];
        bndstr kw_args[kw_args_len * 2];
        size_t pos_idx = 0, kw_idx = 0;

        testp = lpar_st;
        UNTIL_REACHING1(testp, ed, ')') {
            UNTIL_REACHING2(testp, ed, ',', ')') {
                char *k_st = testp;
                UNTIL_REACHING3(testp, ed, ',', '=', ')') {
                    testp++;
                }
                char *k_ed = testp;
                CONSUME_SPACETAB(k_st, k_ed);
                RCONSUME_SPACETAB(k_st, k_ed);

                if (EQ(testp, ed, '=')) {
                    testp++;
                    char *v_st = testp;
                    UNTIL_REACHING2(testp, ed, ',', ')') {
                        testp++;
                    }
                    char *v_ed = testp;
                    CONSUME_SPACETAB(v_st, v_ed);
                    RCONSUME_SPACETAB(v_st, v_ed);

                    kw_args[kw_idx++] = (bndstr){k_st, k_ed - k_st};
                    kw_args[kw_idx++] = (bndstr){v_st, v_ed - v_st};
                } else {
                    pos_args[pos_idx++] = (bndstr){k_st, k_ed - k_st};
                }
            }
            if (EQ(testp, ed, ',')) {
                testp++;
            }
        }
        nm_inl_emit_macro(extra->container, extra->ctx, name, true, pos_args_len, pos_args, kw_args_len, kw_args, raw);
        return;
    }
not_a_fn_macro:
    nm_inl_emit_macro(extra->container, extra->ctx, name, false, 0, NULL, 0, NULL, raw);
}


namuast_inl_container *scn_parse_inline(namuast_inl_container *container, char *p, char* border, char **p_out, struct namugen_ctx* ctx) { 
    yyscan_t inline_scanner;
    InlineScannerExtra extra = {
        .ctx = ctx,
        .container = container,
        .num_chars = 0
    };
    yylex_init_extra(&extra, &inline_scanner);

    yy_scan_bytes(p, border - p, inline_scanner);
    yylex(inline_scanner);
    yylex_destroy(inline_scanner);

    *p_out = p + extra.num_chars;
    return container;
}
