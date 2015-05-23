#include <stdio.h>
#include <stdlib.h>

#include "pine.h"
#include "data.h"
#include "hiredis/hiredis.h"
// #include "data.h"
//
//
#define mysql_fatal(mysql) do {\
    uwsgi_log("(MYSQL)%s at [%s:%d]\n", mysql_error(mysql), __FILE__, __LINE__); \
    abort(); \
} while (0)

typedef struct {
    int id;
    ConnCtx conn;
} CoreData;

int not_found_404(PineRequest *req) {
    pr_prepare(req, 404, NULL);
    pr_add_content_type(req, "text/html; charset=utf-8;");
    GUARD(pr_writes(req, "<h1> 404 NOT FOUND </h1>"));
    return PINE_OK;
}

int bad_request_400(PineRequest *req) {
    pr_prepare(req, 400, NULL);
    pr_add_content_type(req, "text/html; charset=utf-8;");
    GUARD(pr_writes(req, "<h1> 400 BAD REQUEST </h1>"));
    return PINE_OK;
}

static bool prefix(char* s, char* pattern, char** p_out) {
    size_t len = strlen(pattern);
    if (!strncmp(s, pattern, len))  {
        *p_out = s + len;
        return true;
    } else {
        *p_out = NULL;
        return false;
    }
}

#define RAW_PAGE_PREFIX "/wiki/raw/"
int pine_main (PineRequest *req) {
    ((CoreData *)CORE_DATA(req))->conn.req = req;

    sds path = req->env.path;
    char *suffix;
    if (prefix(path, RAW_PAGE_PREFIX, &suffix)) {
        if (strstr(suffix, "/")) {
            goto bad_request;
        }
        RAII_SDS sds docname = sdsnew(suffix);
        RAII_Document Document doc;
        Document_init(&doc);
        if (!find_document(&((CoreData *)CORE_DATA(req))->conn, docname, &doc)) {
            goto not_found;
        }
        GUARD(pr_prepare(req, 200, NULL));
        GUARD(pr_add_content_type(req, "text/plain; charset=utf-8"));
        GUARD(pr_write(req, doc.source, sdslen(doc.source)));
    } else {
        goto not_found;
    }
    return PINE_OK;

bad_request:
    GUARD(bad_request_400(req));
    return PINE_OK;
not_found:
    GUARD(not_found_404(req));
    return PINE_OK;
}

static void uwsgi_coroutine_read_hook(redisContext *c) {
    uwsgi.wait_read_hook(c->fd, 0);
}

static void uwsgi_coroutine_write_hook(redisContext *c) {
    uwsgi.wait_write_hook(c->fd, 0);
}

void pine_init(int async) {
    redisCoroutineReadHook = uwsgi_coroutine_read_hook;
    redisCoroutineWriteHook = uwsgi_coroutine_write_hook;
    uwsgi_log("**Starting Pine using %d async worker(s)**\n", async);
}

void* pine_init_data_per_core(int core_id) {
    // Connection to DB's done here

    CoreData* core_data = malloc(sizeof(CoreData));
    core_data->id = core_id;

    ConnCtx *conn = &core_data->conn;

    conn->wait_read_hook = uwsgi.wait_read_hook;
    conn->wait_write_hook = uwsgi.wait_read_hook;

    conn->mysql = mysql_init(NULL);
    mysql_options(conn->mysql, MYSQL_OPT_NONBLOCK, 0);
    uwsgi_log("Connecting to MySQL/MariaDB...\n");
    if (!mysql_real_connect(conn->mysql, NULL, "root", NULL, "test", 0, "/tmp/mysql.sock", 0)) {
        mysql_fatal(conn->mysql);
    }

    struct timeval timeout = { 20, 0 }; // 20 seconds
    uwsgi_log("Connecting to Redis...\n");
    conn->redis = redisConnectWithTimeout("localhost",6379, timeout);
    if (conn->redis == NULL || conn->redis->err) {
        if (conn->redis) {
            uwsgi_log("(Redis)Connection error: %s\n", conn->redis->errstr);
            redisFree(conn->redis);
        } else {
            uwsgi_log("Connection error: can't allocate redis context\n");
        }
        abort();
    }
    uwsgi_log("Connected to Redis!\n");
    return core_data;
}
