#include <string.h>
#include "pine.h"
#define SAFELY_SDSFREE(x) if (x) { sdsfree(x); }


// from http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
static struct status_code_pair {
    int code;
    char *status;
} status_code_list [] = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "Switch Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {419, "Authentication Timeout"},
    {420, "Enhance Your Calm"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {440, "Login Timeout"},
    {444, "No Response"},
    {449, "Retry With"},
    {450, "Blocked by Windows Parental Controls"},
    {451, "Unavailable For Legal Reasons"},
    {451, "Redirect"},
    {494, "Request Header Too Large"},
    {495, "Cert Error"},
    {496, "No Cert"},
    {497, "HTTP to HTTPS"},
    {498, "Token expired/invalid"},
    {499, "Client Closed Request"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {509, "Bandwidth Limit Exceeded"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"},
    {598, "Network read timeout error"},
    {599, "Network connect timeout error"},
};

char* get_status_from_code(int code) {
    int st = 0, ed = sizeof(status_code_list) / sizeof(struct status_code_pair) - 1;
    while (st < ed) {
        int mid = (st + ed) / 2;
        struct status_code_pair pair = status_code_list[mid];
        if (pair.code < code) {
            st = mid + 1;
        } else if (pair.code > code) {
            ed = mid - 1;
        } else
            return pair.status;
    }
    return NULL; // failed to retreive status
}

static void keyval_copy (void *lhs, const void *rhs) {
    PineKeyval *dst = (PineKeyval *)lhs;
    const PineKeyval *src = (const PineKeyval *) rhs;

    dst->key = sdsdup(src->key);
    dst->value = sdsdup(src->value);
}

const UT_icd ut_pinekeyval_icd = {sizeof(PineKeyval), NULL, keyval_copy, (void (*)(void *))PineKeyval_remove};

void PineKeyval_init(PineKeyval *kv, char *key, char* value) {
    kv->key = sdsnew(key);
    kv->value = sdsnew(value);
}


UT_array* PineKeyval_array() {
    UT_array* retval;
    utarray_new(retval, &ut_pinekeyval_icd);
    return retval;
}

void PineKeyval_remove(PineKeyval *kv) {
    sdsfree(kv->key);
    sdsfree(kv->value);
}


    

void pr_init(PineRequest* req) {
    memset(&req->env, 0, sizeof(PineRequestStrings));
    req->arg_kvs = NULL;
    req->env._sentinel = (sds)-1;
}

static void PineRequestStrings_remove(PineRequestStrings *strs) {
    sds* s;
    for (s = (sds *)strs; *s != (sds)-1; s++) {
        SAFELY_SDSFREE(*s);
    }
}


void pr_remove(PineRequest* req) {
    PineRequestStrings_remove(&req->env);
    if (req->arg_kvs)
        utarray_free(req->arg_kvs);
}

int pr_prepare(PineRequest *req, int code, char *status) {
    if (!status) {
        status = get_status_from_code(code);
        if (!status)
            status = "Unknown";
    }
    char s[128];
    int len = snprintf(s, 128, "%d %s", code, status);
    if (len > 128) {
        // unlikely to happen
        len = 128;
        memset(s, 0, sizeof(char) * 128);
    }
    return uwsgi_response_prepare_headers(req->wsgi_req, s, len);
}

int pr_header(PineRequest *req, char *key, char* value, size_t value_len) {
    return uwsgi_response_add_header(req->wsgi_req, key, strlen(key), value, value_len);
}

int pr_add_content_type(PineRequest *req, char* value) {
    return pr_header(req, "Content-Type", value, strlen(value));
}

int pr_add_content_length(PineRequest *req, size_t content_length) {
    char s[128];

    int len = snprintf(s, 128, "%zu", content_length);
    if (len > 128) {
        // unlikely to happen
        len = 128;
        memset(s, 0, sizeof(char) * 128);
    } else if (len < 0)
        len = 0;
    return pr_header(req, "Content-Length", s, (size_t)len);
}

int pr_write(PineRequest *req, char* buf, size_t len) {
    return uwsgi_response_write_body_do(req->wsgi_req, buf, len);
}

int pr_writes(PineRequest *req, char* str) {
    return pr_write(req, str, strlen(str));
}

char* PineRequest_readline(PineRequest *req, ssize_t hint, ssize_t *rlen) {
    return uwsgi_request_body_readline(req->wsgi_req, hint, rlen);
}

char* PineRequest_read(PineRequest *req, ssize_t hint, ssize_t *rlen) {
    return uwsgi_request_body_read(req->wsgi_req, hint, rlen);
}


static inline bool test_if_hexchar(char ch, unsigned char *dec_out) {
    if (ch >= 'A' && ch <= 'F') {
        *dec_out = ch - 'A' + 10;
        return true;
    } else if (ch >= 'a' && ch <= 'f') {
        *dec_out = ch - 'a' + 10;
        return true;
    } else if (ch >= '0' && ch <= '9') {
        *dec_out = ch - '0';
        return true;
    }
    *dec_out = -1;
    return false;
}

// unsafe operation, boundary check should be done before this called
static bool decode_percent_escaped_ch(char *str, char *ch_out) {
    unsigned char hi, lo;
    if (*str == '%' && test_if_hexchar(*(str + 1), &hi) && test_if_hexchar(*(str + 2), &lo)) {
        *ch_out = (char)((hi << 4) + lo);
        return true;
    } else { 
        *ch_out = 0;
        return false;
    }
}

sds decode_percent_encoded_str(char *str, size_t len, bool plus_used_as_space) {
    sds ret = sdsempty();
    size_t idx;
    idx = 0;
    while (idx < len) {
        char hex;
        if (idx + 2 < len && decode_percent_escaped_ch(str + idx, &hex)) {
            ret = sdscatlen(ret, &hex, 1);
            idx += 3;
        } else if (plus_used_as_space && str[idx] == '+') {
            ret = sdscatlen(ret, " ", 1);
            idx++;
        } else {
            ret = sdscatlen(ret, str + idx, 1);
            idx++;
        }
    }
    return ret;
}

UT_array* parse_query_string(char* str, bool plus_used_as_space) {
    UT_array* kvs = PineKeyval_array();
    int chunk_count;
    int idx;
    sds* chunks = sdssplitlen(str, strlen(str), "&", 1, &chunk_count);
    for (idx = 0; idx < chunk_count; idx++) {
        sds chunk = chunks[idx];
        char* eq_p = strstr(chunk, "=");
        RAII_SDS sds key;
        RAII_SDS sds value;
        if (eq_p) {
            key = decode_percent_encoded_str(chunk, eq_p - chunk, plus_used_as_space);
            value = decode_percent_encoded_str(eq_p + 1, chunk + sdslen(chunk) - eq_p, plus_used_as_space);
        } else { 
            key = sdsdup(chunk);
            value = sdsempty();
        }

        PineKeyval tmp_kv = {.key = key, .value = value};
        utarray_push_back(kvs, &tmp_kv);
    }
    for (idx = 0; idx < chunk_count; idx++)
        sdsfree(chunks[idx]);
    free(chunks);
    return kvs;
}
