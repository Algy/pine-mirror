#ifndef _HTMLGEN_H
#define _HTMLGEN_H

#include "sds/sds.h"
#include "namugen.h"
#include <stdbool.h>

void initmod_htmlgen();

struct namugen_doc_itfc {
    struct namuast_container* (*get_ast)(struct namugen_doc_itfc *, const char *doc_name); // it may return NULL
    void (*documents_exist)(struct namugen_doc_itfc *, int argc, char** docnames, bool *results);
    sds (*doc_href)(struct namugen_doc_itfc *, char *doc_name);
};

#define MAX_TOC_COUNT 100
#define INITIAL_MAIN_BUF (4096*4)
#define INITIAL_INTERNAL_LINKS 1024


struct htmlgen_ctx;
typedef struct htmlgen_macro_record {
    const char *name;
    sds (*converter)(struct htmlgen_ctx *ctx, struct htmlgen_macro_record*, struct namuast_inl_macro *, struct namuast_container *, sds buf);
} htmlgen_macro_record;

typedef struct htmlgen_simple_macro_record {
    const char *name;
    const char *temp;
} htmlgen_simple_macro_record;


struct htmlgen_ctx;
typedef struct htmlgen_includer_info {
    struct htmlgen_ctx *includer_ctx;
} htmlgen_includer_info;

typedef struct htmlgen_ctx {
    struct namugen_doc_itfc *doc_itfc;
    struct namuast_inl_fnt *last_emitted_fnt; // borrowed (a weak reference)

    sds cur_doc_name;

    /* temporary values */
    sds *ne_docs; // sorted array of names of not existing documents
    size_t ne_docs_count;
    namuast_container *ast_being_used;
    
    htmlgen_includer_info *includer_info;
} htmlgen_ctx;

void htmlgen_init(htmlgen_ctx *ctx, const char *cur_doc_name, struct namugen_doc_itfc *doc_itfc);
sds htmlgen_generate(htmlgen_ctx *html_ctx, namuast_container *ast_container, sds buf);
void htmlgen_remove(htmlgen_ctx *ctx);

sds htmlgen_generate_directly(const char *doc_name, struct namugen_doc_itfc *doc_itfc, sds buf, bool *success_out);

sds htmlgen_macro_fallback(htmlgen_ctx *ctx, struct namuast_inl_macro* macro, sds buf);
#endif // ifndef _HTMLGEN_H
