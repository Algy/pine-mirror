#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "namugen.h"
#include "htmlgen.h"

static void dummy_docs_exist(struct namugen_doc_itfc* x, int argc, char** docnames, bool* results) {
    int idx;
    for (idx = 0; idx < argc; idx++) {
        results[idx] = false;
    }
}

#include "escaper.inc"
sds doc_href(struct namugen_doc_itfc* x, char* doc_name) {
    sds chunk = escape_url_chunk(doc_name, false);
    sds ret = sdsnew("/wiki/page/");
    ret = sdscatsds(ret, chunk);
    sdsfree(chunk);
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
    initmod_namugen();
    initmod_htmlgen();

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

    sds result = sdsnewlen(NULL, filesize * 2);
    sdsupdatelen(result);
    clock_t clock_st = clock();
    result = htmlgen_generate_directly("Dummy", buffer, filesize, &hook, &doc_itfc, result);
    clock_t clock_ed = clock();
    double us = (((double) (clock_ed - clock_st)) / CLOCKS_PER_SEC) * 1000. * 1000.;

    printf("<div class='wiki-main'><article>%s</article></div>\n", result);
    sdsfree(result);
    printf("generated in %.2lf us\n", us);
    free(buffer);
    fclose(fp);
    return 0;
}
