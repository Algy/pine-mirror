#ifndef _PINE_H
# define _PINE_H
#include <stdbool.h>
#include <setjmp.h>

#include "uwsgi.h"
#include "sds/sds.h"
#include "uthash/src/utarray.h"

#include "raii.h"

#define PINE_OK UWSGI_OK
#define PINE_AGAIN UWSGI_AGAIN

extern struct uwsgi_server uwsgi;

#define MAX_ASYNC_CORE 4096
typedef struct {
    void *userdata;
} PineDataPerCore;
extern PineDataPerCore pine_data_per_core[MAX_ASYNC_CORE];

#define CORE_DATA(req) (pine_data_per_core[(req)->wsgi_req->async_id].userdata)

#define URGENT_ESCAPE(pinereq, msg) do { \
    (pinereq)->urgent_jmp_msg = sdsnew(msg); \
    (pinereq)->urgent_jmp_file = sdsnew(__FILE__); \
    (pinereq)->urgent_jmp_line = sdsfromlonglong(__LINE__); \
    longjmp((pinereq)->urgent_jmp_msg, 1); \
} while (0) 

#define GUARD(expr) { \
    int __r__ = (expr); \
    if (__r__) \
        return __r__; \
}



typedef struct {
    sds key;
    sds value;
} PineKeyval;

void PineKeyval_init(PineKeyval *kv, char *key, char* value);
extern const UT_icd ut_pinekeyval_icd;
void PineKeyval_remove(PineKeyval *kv);
UT_array* PineKeyval_array();


typedef struct {
    sds method;
    sds host;
    sds path;
    sds remote_addr;
    sds query_string;

    sds request_uri;

    sds _sentinel; // should be intialized to (sds)-1
} PineRequestStrings;

typedef struct PineRequest {
    struct wsgi_request *wsgi_req; // always borrowed and not disposed when enclosing structure die

    PineRequestStrings env;

    jmp_buf urgent_jmp_buf;
    sds urgent_jmp_msg;
    sds urgent_jmp_file;
    sds urgent_jmp_line;

    UT_array* arg_kvs;
} PineRequest;


void pr_init(PineRequest* req);
void pr_remove(PineRequest* req);
int pr_prepare(PineRequest *req , int code, char *status);
int pr_header(PineRequest *req, char *key, char* value, size_t value_len);

int pr_add_content_type(PineRequest *req, char* type);
int pr_add_content_length(PineRequest *req, size_t content_length);
int pr_write(PineRequest *req, char* buf, size_t len);
int pr_writes(PineRequest *req, char* str);


/*
 * util functions
 */

sds decode_percent_encoded_str(char *str, size_t len, bool plus_used_as_space);
UT_array* parse_query_string(char* str, bool plus_used_as_space);
#endif // !defined(_PINE_H)

char* get_status_from_code(int code);
