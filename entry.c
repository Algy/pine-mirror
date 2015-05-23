#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pine.h"

PineDataPerCore pine_data_per_core[MAX_ASYNC_CORE];

int pine_main(PineRequest *req);
void pine_init(int async);
void* pine_init_data_per_core(int core_id);
#define PINE_CORE_DATA()  

static sds get_var_value(struct wsgi_request *wsgi_req, char *key) {
    // get REMOTE_ADDR
    uint16_t vlen = 0;
    char *v = uwsgi_get_var(wsgi_req, key, strlen(key), &vlen);
    assert(v != NULL);
    return sdsnewlen(v, vlen);
}

void _pine_after_fork() {
    if (uwsgi.async > MAX_ASYNC_CORE) {
        uwsgi_log("[Pine] You can't use more than %d (async) cores.", MAX_ASYNC_CORE);
        abort();
    }
    pine_init(uwsgi.async);
    int idx;
    for (idx = 0; idx < uwsgi.async; idx++) {
        pine_data_per_core[idx].userdata = pine_init_data_per_core(idx);
    }
}

int _pine_entry_point(struct wsgi_request *wsgi_req) {
     // read request variables
    if (uwsgi_parse_vars(wsgi_req)) {
        return -1;
    }
    uwsgi_log("ASYNC ID: %d\n", wsgi_req->async_id);

    RAII(pr_remove) PineRequest req;
    pr_init(&req);
    req.wsgi_req = wsgi_req;

    sds query_string;
    req.env.method = get_var_value(wsgi_req, "REQUEST_METHOD");
    req.env.host = get_var_value(wsgi_req, "HTTP_HOST");
    req.env.remote_addr = get_var_value(wsgi_req, "REMOTE_ADDR");
    req.env.path = get_var_value(wsgi_req, "PATH_INFO");
    req.env.request_uri = get_var_value(wsgi_req, "REQUEST_URI");
    req.env.query_string = query_string = get_var_value(wsgi_req, "QUERY_STRING");
    req.arg_kvs = parse_query_string(query_string, false);

    if (!setjmp(req.urgent_jmp_buf)) {
        GUARD(pine_main(&req));
    } else {
        pr_prepare(&req, 500, NULL);
        pr_add_content_type(&req, "text/plain; charset=utf-8;");
        GUARD(pr_writes(&req, "500 Internal Server Error"));
        uwsgi_log("[Fatal Error] %s at [%s:%s]\n", req.urgent_jmp_msg, req.urgent_jmp_file, req.urgent_jmp_line);
        sdsfree(req.urgent_jmp_file);
        sdsfree(req.urgent_jmp_line);
        return UWSGI_OK;
    }

    if (!wsgi_req->headers_sent) {
        if (uwsgi_response_write_headers_do(wsgi_req))
            return -1;
    }
    return UWSGI_OK;
}
