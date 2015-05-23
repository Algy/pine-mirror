#include <stdio.h>
#include <stdlib.h>

#include "pine.h"
#include "data.h"
#include "hiredis/hiredis.h"
#include "namugen.h"

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

static bool say_no() {
    return false;
}

typedef struct {
    struct namugen_doc_itfc vtbl;
    ConnCtx *conn;
    char* docname_prefix;
} NormalNamugenDocumentInterface;

struct namugen_hook_itfc todo_hook = {
    .hook_fn_link = say_no,
    .hook_fn_call = say_no
};


#include "escaper.inc"
static sds nmdi_doc_href(struct namugen_doc_itfc* x, char* doc_name) {
    NormalNamugenDocumentInterface *nmdi = (NormalNamugenDocumentInterface *)x;
    sds chunk = escape_url_chunk(doc_name, false);
    sds ret = sdsnew(nmdi->docname_prefix);
    ret = sdscatsds(ret, chunk);
    sdsfree(chunk);
    return ret;
}
static int ok_put_plain(PineRequest *req, sds html) {
    GUARD(pr_prepare(req, 200, NULL));
    GUARD(pr_add_content_type(req, "text/plain; charset=utf-8"));
    GUARD(pr_write(req, html, sdslen(html)));
    return PINE_OK;
}

static int ok_put_html(PineRequest *req, sds html) {
    GUARD(pr_prepare(req, 200, NULL));
    GUARD(pr_add_content_type(req, "text/html; charset=utf-8"));
    GUARD(pr_write(req, html, sdslen(html)));
    return PINE_OK;
}

static void nmdi_docs_exist(struct namugen_doc_itfc* x, int argc, char** docnames, bool* results) {
    NormalNamugenDocumentInterface *nmdi = (NormalNamugenDocumentInterface *)x;
    documents_exist(nmdi->conn, argc, docnames, results);
}

struct namugen_doc_itfc nmdi_vtbl = {
    .documents_exist = nmdi_docs_exist,
    .doc_href = nmdi_doc_href
};

static sds render_page(ConnCtx *ctx, Document *doc, char *docname_prefix) {
    if (!docname_prefix)
        docname_prefix = "/wiki/page/";
    NormalNamugenDocumentInterface my_itfc = {
       .vtbl = nmdi_vtbl,
       .conn = ctx,
       .docname_prefix = docname_prefix
    };

    struct namugen_ctx* namugen = namugen_make_ctx(doc->name, &my_itfc.vtbl, &todo_hook);
    clock_t clock_st = clock();
    namugen_scan(namugen, doc->source, sdslen(doc->source));
    clock_t clock_ed = clock();
    double ms = (((double) (clock_ed - clock_st)) / CLOCKS_PER_SEC) * 1000.;

    sds result = namugen_ctx_flush_main_buf(namugen);
    result = sdscatprintf(result, "<p class='gen-ms'>generated in %.2lfms</p>", ms);
    namugen_remove_ctx(namugen);
    return result;
}

#define RAW_PAGE_PREFIX "/wiki/raw/"
#define RENDERED_PAGE_PREFIX "/wiki/rendered/"
#define WIKI_PAGE_PREFIX "/wiki/page/"
int pine_main(PineRequest *req) {
    ConnCtx *conn = &((CoreData *)CORE_DATA(req))->conn;
    conn->req = req;

    sds path = req->env.path;
    char *suffix;
    if (prefix(path, RAW_PAGE_PREFIX, &suffix)) {
        if (strstr(suffix, "/")) {
            goto bad_request;
        }
        RAII_SDS sds docname = sdsnew(suffix);
        RAII_Document Document doc;
        Document_init(&doc);
        if (!find_document(conn, docname, &doc)) {
            goto not_found;
        }
        return ok_put_plain(req, doc.source);
    } else if (prefix(path, RENDERED_PAGE_PREFIX, &suffix)) {
        if (strstr(suffix, "/")) {
            goto bad_request;
        }
        RAII_SDS sds docname = sdsnew(suffix);
        RAII_Document Document doc;
        Document_init(&doc);
        if (!find_document(conn, docname, &doc)) {
            goto not_found;
        }
        RAII_SDS sds page = render_page(conn, &doc, RENDERED_PAGE_PREFIX);
        return ok_put_html(req, page);
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
