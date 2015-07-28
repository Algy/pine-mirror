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

typedef struct my_itfc {
    struct namugen_doc_itfc base;
    char *buffer;
    size_t buffer_size;
} my_itfc;

struct namuast_container* get_ast (struct namugen_doc_itfc *_itfc, const char *doc_name) {
    my_itfc *itfc = (my_itfc *)_itfc;

    size_t buffer_size;
    char * buffer;
    if (!strcmp(doc_name, "MyDocument")) {
        buffer = itfc->buffer;
        buffer_size = itfc->buffer_size;
    } else if (!strcmp(doc_name, "inclusion")) {
        buffer = "{{Inclusion Success!}}";
        buffer_size = strlen("{{{Inclusion Success!}}}");
    } else
        return NULL;

    namugen_ctx namugen;
    namugen_init(&namugen, "MyDocument");
    namugen_scan(&namugen, buffer, buffer_size);
    struct namuast_container* result = namugen_obtain_ast(&namugen);
    namugen_remove(&namugen);
    return result;
}


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

    my_itfc itfc = {
        .base = {
            .get_ast = get_ast,
            .documents_exist = dummy_docs_exist,
            .doc_href = doc_href
        },
        .buffer = buffer,
        .buffer_size = filesize
    };

    sds result = sdsnewlen(NULL, filesize * 2);
    sdsupdatelen(result);
    clock_t clock_st = clock();
    result = htmlgen_generate_directly("MyDocument", &itfc.base, result, NULL);
    clock_t clock_ed = clock();
    double us = (((double) (clock_ed - clock_st)) / CLOCKS_PER_SEC) * 1000. * 1000.;

    printf("<div class='wiki-main'><article>%s</article></div>\n<span class='wiki-rendering-time'>generated in %.2lf us</span>\n", result, us);
    sdsfree(result);
    free(buffer);
    fclose(fp);
    return 0;
}
