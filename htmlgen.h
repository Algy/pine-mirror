#ifndef _HTMLGEN_H
#define _HTMLGEN_H

#include "sds/sds.h"
#include "namugen.h"
#include <stdbool.h>

void initmod_htmlgen();

struct namugen_doc_itfc {
    void (*documents_exist)(struct namugen_doc_itfc *, int argc, char** docnames, bool *results);
    sds (*doc_href)(struct namugen_doc_itfc *, char *doc_name);
};

#define MAX_TOC_COUNT 100
#define INITIAL_MAIN_BUF (4096*4)
#define INITIAL_INTERNAL_LINKS 1024

typedef struct htmlgen_ctx {
    struct namugen_doc_itfc *doc_itfc;
    struct namuast_inl_fnt *last_emitted_fnt; // borrowed (a weak reference)

    /* temporary values */
    sds *ne_docs; // sorted array of names of not existing documents
    size_t ne_docs_count;
    namuast_container *ast_being_used;

} htmlgen_ctx;

void htmlgen_init(htmlgen_ctx *ctx, struct namugen_doc_itfc *doc_itfc);
sds htmlgen_generate(htmlgen_ctx *html_ctx, namuast_container *ast_container, sds buf);
void htmlgen_remove(htmlgen_ctx *ctx);

sds htmlgen_generate_directly(const char *doc_name, char *buffer, size_t buf_len, struct namugen_hook_itfc* namugen_hook_itfc, struct namugen_doc_itfc *doc_itfc, sds buf);
#endif // ifndef _HTMLGEN_H
