#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "data.h"
#include "uthash/src/uthash.h"
#include "lz4/lib/lz4.h"
#include "lz4/lib/lz4hc.h"

#include "raii.h"

// 1 hour
#define CACHE_VALID_MILLIS (60L * 60L * 1000L) 


static long long get_epoch() {
    struct timeval tv; 
    if (gettimeofday(&tv, NULL)) {
        return -1; 
    }   
    /* seconds, multiplied with 1 million */
    long long millis = tv.tv_sec * 1000;
    /* Add full microseconds */
    millis += tv.tv_usec/1000;
    /* round up if necessary */
    if (tv.tv_usec % 1000 >= 500) {
        millis++;
    }   
    return millis;
}


#define REDIS_NOT_ERROR(reply) if (reply->type == REDIS_REPLY_STATUS) {  \
        printf("Error received from redis: %s[%s:%d]\n", reply->str, __FILE__, __LINE__); \
        abort(); \
    } else 
#define MARIADB_NOT_ERROR(mysql, ret) if (ret) {  \
    printf("Error received from MariaDB: %s[%s:%d]\n", mysql_error(mysql), __FILE__, __LINE__); \
} else


#define MYSQL_ASYNC(ctx, start, cont) do { \
    int __status__ = start; \
    while (__status__) { \
        int timeout = 0; \
        if (__status__ & MYSQL_WAIT_TIMEOUT) { \
            timeout = mysql_get_timeout_value(ctx->mysql); \
        } \
        __status__ = 0; \
        if (__status__ & MYSQL_WAIT_READ) { \
            int hook_ret = ctx->wait_read_hook(mysql_get_socket(ctx->mysql), timeout); \
            if (hook_ret > 0) { \
                __status__ |= MYSQL_WAIT_READ; \
            } else if (hook_ret == 0) { \
                __status__ |= MYSQL_WAIT_TIMEOUT; \
            } \
        } \
        if (__status__ & MYSQL_WAIT_WRITE) { \
            int hook_ret = ctx->wait_write_hook(mysql_get_socket(ctx->mysql), timeout); \
            if (hook_ret > 0) { \
                __status__ |= MYSQL_WAIT_WRITE; \
            } else if (hook_ret == 0) { \
                __status__ |= MYSQL_WAIT_TIMEOUT; \
            } \
        } \
        __status__ = cont; \
    } \
} while (0)

static void _query_inner(ConnCtx* ctx, sds query) {
    int query_ret;
    MYSQL_ASYNC(ctx,
        mysql_real_query_start(&query_ret, ctx->mysql, query, sdslen(query)),
        mysql_real_query_cont(&query_ret, ctx->mysql, __status__)
    );
    MARIADB_NOT_ERROR(ctx->mysql, query_ret) { }
}

static MYSQL_RES* query_seq(ConnCtx* ctx, sds query) {
    _query_inner(ctx, query);
    MYSQL_RES *mysql_ret = mysql_use_result(ctx->mysql);
    return mysql_ret;
}

static bool async_fetch_row(ConnCtx* ctx,  MYSQL_RES* res, MYSQL_ROW *row) {
    MYSQL_ASYNC(ctx,
        mysql_fetch_row_start(row, res),
        mysql_fetch_row_cont(row, res, __status__)
    );
    return *row != NULL;
}


static sds escape_sql_str(MYSQL *mysql, char* s) {
    size_t len = strlen(s);
    sds escaped_s = sdsnewlen(NULL, len * 2);
    mysql_real_escape_string(mysql, escaped_s, s, len);
    sdsupdatelen(escaped_s);
    return escaped_s;
}

/*
 * Mysql
 * ===
 *
 *  RecentDocument
 * --------------------
 *  name: primary key
 *  -----
 *  source: string
 *  -------------------
 *  rev:  varchar(25)
 *  -------------------
 *  collected_time | datetime
 *  -------------------
 *  updated_time | datetime
 *
 *
 *  Archeive
 *  ------
 *  TODO
 *
 *
 * Redis
 * (always used as LRU cache)
 * 
 * existing headers + original_size
 * ===
 * (Header : Value\n)*
 * \n
 * lz4_compressed_data
 */

bool deserialize_document(Document *doc_out, char *s, size_t len) {
    if (len == 0)
        return false;
    char *s_ed = s + len;
    char *p = s;

    int original_size = -1;
    bool updated_time_supplied = false;
    bool collected_time_supplied = false;
    bool rev_supplied = false;
    bool name_supplied = false;
    bool cached_time_supplied = false;
    while (p < s_ed && *p != '\n') {
        char *hd_st = p;
        while (p < s_ed && *p != ':' && *p != '\n') 
            p++;
        if (p >= s_ed || *p == '\n')
            return false;
        // assert *p == ':'
        RAII_SDS sds key = sdsnewlen(hd_st, p - hd_st);
        p++;
        char *val_st = p;
        while (p < s_ed && *p != '\n') 
            p++;
        RAII_SDS sds value = sdsnewlen(val_st, p - val_st);
        if (p < s_ed)
            p++;

        // TODO: refactoring this hard-wired logic
        if (!strcmp(key, "name")) {
            name_supplied = true;
            doc_out->name = sdsdup(value);
        } else if (!strcmp(key, "updated_time")) {
            updated_time_supplied = true;
            doc_out->updated_time = strtoll(value, NULL, 10);
        } else if (!strcmp(key, "rev")) {
            rev_supplied = true;
            doc_out->rev = sdsdup(value);
        } else if (!strcmp(key, "collected_time")) {
            collected_time_supplied = true;
            doc_out->collected_time = strtoll(value, NULL, 10);
        } else if (!strcmp(key, "cached_time")) {
            cached_time_supplied = true;
            doc_out->cached_time = strtoll(value, NULL, 10);
        } else if (!strcmp(key, "original_size")) {
            original_size = (int)strtoll(value, NULL, 10);
        }
    }
    bool success = (original_size > 0) && updated_time_supplied && collected_time_supplied && rev_supplied && name_supplied && cached_time_supplied; 
    if (!success) {
        if (doc_out->rev) {
            sdsfree(doc_out->rev);
            doc_out->rev = NULL;
        }
        if (doc_out->name) {
            sdsfree(doc_out->name);
            doc_out->name = NULL;
        }
        doc_out->updated_time = -1;
        doc_out->collected_time = -1;
        doc_out->cached_time = -1;
        return false;
    }

    if (p >= s_ed) {
        return false;
    } else {
        p++; // consume '\n'
        int compressed_size = s + len - p; // TODO
        int max_decompressed_size = original_size;
        sds original_source = sdsnewlen(NULL, max_decompressed_size);
        int dec_size = LZ4_decompress_safe(p, original_source, compressed_size, max_decompressed_size);
        if (dec_size < 0) {
            sdsfree(original_source);
            return false;
        }
        doc_out->source = original_source;
    }
    return true;
}

char* serialize_document(Document *slot, long long cached_time, size_t *buf_size_out) {
    RAII_SDS sds header = sdsempty();
#define MAKE_CACHE_HEADER(expr, name, percent) do {  \
    header = sdscatprintf(header, name":"percent"\n", expr); \
    } while (0)

    MAKE_CACHE_HEADER(slot->name, "name", "%s");
    MAKE_CACHE_HEADER((long long)slot->updated_time, "updated_time", "%lld");
    MAKE_CACHE_HEADER((long long)slot->collected_time, "collected_time", "%lld");
    MAKE_CACHE_HEADER(slot->rev, "rev", "%s");
    MAKE_CACHE_HEADER(sdslen(slot->source), "original_size", "%zu");
    MAKE_CACHE_HEADER(cached_time, "cached_time", "%lld");
    header = sdscat(header, "\n");
    size_t header_len = sdslen(header);

    int compress_bound = LZ4_compressBound(sdslen(slot->source)); 
    char* buf = calloc(header_len + compress_bound, 1);
    memcpy(buf, header, header_len * sizeof(char));
    int compr_size = LZ4_compress_HC(slot->source, buf + header_len, sdslen(slot->source), compress_bound, 4);
    *buf_size_out = header_len + compr_size;
    return buf;
}

typedef struct {
    char* docname;
    bool* result_box;
} _RemainingSlot;

static int _p_sdscmp(const void *lhs, const void *rhs) {
    return sdscmp(*(sds *)lhs, *(sds *)rhs);
}

static int _p_strcmp(const void *lhs, const void *rhs) {
    return strcmp(*(char **)lhs, *(char **)rhs);
}

void documents_exist(ConnCtx *ctx, int argc, char** docnames, bool *result) {
    _RemainingSlot slots[argc];
    int slot_cnt = 0;

    int idx;
    for (idx = 0; idx < argc; idx++) {
        char* docname = docnames[idx];
        redisAppendCommand(ctx->redis, "EXISTS wiki-recent-document-%s", docname);
    }

    for (idx = 0; idx < argc; idx++) {
        redisReply *reply;
        redisGetReply(ctx->redis, (void **)&reply); // reply for SET
        REDIS_NOT_ERROR(reply) {
        }
        bool exists = reply->type == REDIS_REPLY_INTEGER && (bool)reply->integer;
        freeReplyObject(reply);

        result[idx] = exists;
        if (!exists) {
            // cache miss occurred
            slots[slot_cnt++] = (_RemainingSlot){docnames[idx], &result[idx]};
        }
    }

    if (slot_cnt > 0) {
        // Now deal with documents for which cache miss occurred

        RAII_SDS sds query = sdsnew("SELECT name FROM RecentDocument WHERE name IN ("); 
        bool is_first = true;
        for (idx = 0; idx < slot_cnt; idx++) {
            if (!is_first) {
                query = sdscat(query, ", ");
            } else
                is_first = false;
            RAII_SDS sds escaped_docname = escape_sql_str(ctx->mysql, slots[idx].docname);
            query = sdscatprintf(query, "'%s'", escaped_docname);
        }
        query = sdscat(query, ")");

        MYSQL_RES *res = query_seq(ctx, query);
        MYSQL_ROW row;

        sds existing_rows[slot_cnt];
        int existing_row_cnt = 0;
        while (async_fetch_row(ctx, res, &row)) {
            existing_rows[existing_row_cnt++] = sdsnew(row[0]);
        }

        mysql_free_result(res);
        qsort(existing_rows, existing_row_cnt, sizeof(sds), _p_sdscmp);

        for (idx = 0; idx < slot_cnt; idx++) {
            *slots[idx].result_box = bsearch(&slots[idx].docname, existing_rows, existing_row_cnt, sizeof(sds), _p_strcmp) != NULL;
        }

        for (idx = 0; idx < existing_row_cnt; idx++) {
            sdsfree(existing_rows[idx]);
        }
    }
}

static bool find_document_from_cache(ConnCtx* ctx, char* docname, Document* doc_out) {
    bool found = false;
    redisReply* reply = redisCommand(ctx->redis, "GET wiki-recent-document-%s", docname);
    REDIS_NOT_ERROR(reply) {
        if (reply->type == REDIS_REPLY_STRING) {
            if (deserialize_document(doc_out, reply->str, reply->len)) {
                if (!strcmp(doc_out->name, docname))
                    found = true;
                else {
#ifdef uwsgi_log
                    uwsgi_log("Cache docname mismatch\n");
#endif
                    found = false;
                }
            } else {
                found = false;
            }
        } else {
            found = false;
        }
    }
    freeReplyObject(reply);
    return found;
}

static void evict_cache(ConnCtx* ctx, char* docname) {
    freeReplyObject(redisCommand(ctx->redis, "DEL wiki-recent-document-%s", docname));
}

static void cache_document(ConnCtx* ctx, Document* doc) {
    size_t cache_len;
    char* cache = serialize_document(doc, get_epoch(), &cache_len);
    freeReplyObject(redisCommand(ctx->redis, "SET wiki-recent-document-%s %b", doc->name, cache, cache_len));
    free(cache);
}

static bool is_cache_up_to_date(Document* doc) {
    long long cur_epoch = get_epoch();
    if (cur_epoch >= doc->cached_time + CACHE_VALID_MILLIS) {
        return false;
    }
    return true;
}

static bool find_document_from_main_storage(ConnCtx* ctx, char* docname, Document* doc_out) {
    RAII_SDS sds escaped_docname = escape_sql_str(ctx->mysql, docname);
    RAII_SDS sds query = sdscatprintf(sdsempty(), "SELECT name, rev, UNIX_TIMESTAMP(collected_time), UNIX_TIMESTAMP(updated_time), source FROM RecentDocument WHERE name=\"%s\"", escaped_docname);
    MYSQL_RES *res = query_seq(ctx, query);
    MYSQL_ROW row;
    bool exists = async_fetch_row(ctx, res, &row);
    if (!exists) {
        mysql_free_result(res);
        return false;
    }
    doc_out->name = sdsnew(row[0]);
    doc_out->rev = sdsnew(row[1]);
    // TODO: use more accurate way
    doc_out->collected_time = strtoll(row[2], NULL, 10) * 1000L;
    doc_out->updated_time = strtoll(row[3], NULL, 10) * 1000L;
    doc_out->cached_time = -1;
    doc_out->source = sdsnew(row[4]);

    mysql_free_result(res);
    return true;
}


bool find_document(ConnCtx* ctx, char* docname, Document* doc_out) {
    if (find_document_from_cache(ctx, docname, doc_out)) {
        if (is_cache_up_to_date(doc_out)) {
            // HAPPY HAPPY
            return true;
        }
        evict_cache(ctx, doc_out->name);
    }

    if (find_document_from_main_storage(ctx, docname, doc_out)) {
        // hoping that LRU caching be done automatically
        cache_document(ctx, doc_out); 
        return true;
    }
    return false;
}

void Document_init(Document* doc) {
    doc->name = NULL;
    doc->rev = NULL;
    doc->source = NULL;
    doc->cached_time = -1;
    doc->collected_time = -1;
    doc->updated_time = -1;
}

void Document_remove(Document* doc) {
#define SAFELY_SDS_FREE(expr) if (expr) { sdsfree(expr); expr = NULL; }
    SAFELY_SDS_FREE(doc->name);
    SAFELY_SDS_FREE(doc->rev);
    SAFELY_SDS_FREE(doc->source);
}
