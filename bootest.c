#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "namugen.h"

void dummy_docs_exist(struct namugen_doc_itfc* x, int argc, char** docnames, bool* results) {
    int idx;
    for (idx = 0; idx < argc; idx++) {
        results[idx] = false;
    }
}

#include "escaper.inc"
sds doc_href(struct namugen_doc_itfc* x, char* doc_name) {
    sds val = escape_html_attr(doc_name);

    sds ret = sdsnew("/wiki/page/");
    // TODO: url escaping here
    ret = sdscatsds(ret, val);
    sdsfree(val);
    return ret;
}

bool say_no() {
    return false;
}

struct namugen_doc_itfc doc_itfc = {
    .documents_exist = dummy_docs_exist,
    .doc_href = doc_href
};


struct namugen_hook_itfc hook = {
    .hook_fn_link = say_no,
    .hook_fn_call = say_no
};

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
    char *buffer = calloc(filesize + 1, 1);
    fread(buffer, 1, filesize + 1, fp);
    buffer[filesize] = 0;

    struct namugen_ctx* ctx = namugen_make_ctx("MyDocument", &doc_itfc, &hook);

    clock_t clock_st = clock();
    namugen_scan(ctx, buffer, filesize);
    clock_t clock_ed = clock();
    double us = (((double) (clock_ed - clock_st)) / CLOCKS_PER_SEC) * 1000. * 1000.;

    sds result = namugen_ctx_flush_main_buf(ctx);
    printf("<!doctype html><meta charset='utf-8'><link ref='stylesheet' href='https://namu.wiki/css/wiki.css?1430916909'> <div class='wiki-main'><article>%s</article></div>\n", result);
    sdsfree(result);
    printf("generated in %.2lf us\n", us);
    namugen_remove_ctx(ctx);
    free(buffer);
    fclose(fp);
    return 0;
}
