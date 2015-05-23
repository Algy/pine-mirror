#ifndef _DATA_H
#define _DATA_H

#include <stdbool.h>

#include "sds/sds.h"
#include "mariadb-connector-c/include/mysql.h"
#include "hiredis/hiredis.h"

typedef struct {
    // TODO
    MYSQL* mysql;
    redisContext *redis;
    struct PineRequest* req;

    void (*wait_read_hook)(int fd, int timeout);
    void (*wait_write_hook)(int fd, int timeout);
} ConnCtx;

typedef struct {
    sds name;
    long long updated_time;
    long long collected_time;
    long long cached_time;
    sds rev;

    sds source;
} Document;

void Document_init(Document* doc);
void Document_remove(Document* doc);

bool find_document(ConnCtx* ctx, sds docname, Document* doc_out);

char* serialize_document(Document *slot, long long cached_time, size_t* buf_size_out);
bool deserialize_document(Document *doc_out, char *s, size_t len);

void documents_exist(ConnCtx *ctx, int argc, sds* docnames, bool *result);

#endif
