#include <stdlib.h>
#include <stdint.h> 
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "namudiff.h"

#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))

typedef struct {
    const char *p;
    size_t index;
} UTF8IndexHint;

static inline int32_t iter_utf8(const char* utf8, const char* end, const char **next_p_ret) {
    int32_t c;
    const char* p = utf8;
    char v = *p;
    if (v >= 0) {
        c = v;
        p++;
    } else {
        int shiftCount = 0;
        if ((v & 0xE0) == 0xC0) {
            shiftCount = 1;
            c = v & 0x1F;
        } else if ((v & 0xF0) == 0xE0) {
            shiftCount = 2;
            c = v & 0xF;
        } else if ((v & 0xF8) == 0xF0) {
            shiftCount = 3;
            c = v & 0x7;
        } else {
            goto error;
        }
        if (p + shiftCount >= end)
            goto error;
        p++;
        while (shiftCount > 0) {
            v = *p++;
            if ((v & 0xC0) != 0x80) {
                goto error;
            }
            c <<= 6;
            c |= (v & 0x3F);
            --shiftCount;
        }
    }
    *next_p_ret = p;
    return c;
error:
    *next_p_ret = utf8;
    return -1;
}

static inline int32_t riter_utf8(const char* utf8, const char* base, const char **prev_p_ret) {
    int32_t c;
    const char* p = utf8;
    const char* C0chunk_ed = p;
    p--;
    while (base <= p && ((*p & 0xC0) == 0x80)) {
        p--;
    }
    if (base > p)
        goto error;
    const char* C0chunk_st = p + 1;
    size_t shiftCount = C0chunk_ed - C0chunk_st;

    char v = *p;
    switch (shiftCount) {
    case 0:
        if (v < 0)
            goto error;
        c = v;
        break;
    case 1:
        if ((v & 0xE0) != 0xC0)
            goto error;
        c = v & 0x1F;
        break;
    case 2:
        if ((v & 0xF0) != 0xE0)
            goto error;
        c = v & 0xF;
        break;
    case 3:
        if ((v & 0xF8) != 0xF0)
            goto error;
        c = v & 0x7;
        break;
    default:
        goto error;
    }
    const char* C0chunk_iter;
    for (C0chunk_iter = C0chunk_st; C0chunk_iter < C0chunk_ed; C0chunk_iter++) {
        c <<= 6;
        c |= ((*C0chunk_iter) & 0x3F);
    }
    *prev_p_ret = p;
    return c;
error:
    *prev_p_ret = utf8;
    return -1;
}

static inline size_t utf8_count(const char *utf8, size_t bufsize, bool *valid_utf8_ret) {
    const char *p = utf8, *end = utf8 + bufsize;
    bool success = true;
    size_t result = 0;
    while (p < utf8 + bufsize) { 
        if (iter_utf8(p, end, &p) == -1) {
            success = false;
            p++;
        }
        result++;
    }
    if (valid_utf8_ret)
        *valid_utf8_ret = success;
    return result;
}


static inline int32_t utf8_get(const char *utf8, size_t bufsize, size_t index, UTF8IndexHint *hint) {
    int32_t codepoint;

    const char *p;
    const char *end = utf8 + bufsize;
    const char* dummy;
    size_t iter_index;

    if (hint) {
        p = hint->p;
        iter_index = hint->index;
    } else {
        p = utf8;
        iter_index = 0;
    }

    if (hint && hint->index > index) {
        while (utf8 < p && iter_index > index) {
            if (riter_utf8(p, utf8, &p) == -1)
                p--;
            iter_index--;
        }
        if (iter_index > index)
            goto error;
        codepoint = iter_utf8(p, end, &dummy);
    } else {
        while (p < end && iter_index < index) {
            if (iter_utf8(p, end, &p) == -1)
                p++;
            iter_index++;
        }
        if (iter_index < index)
            goto error;
        codepoint = iter_utf8(p, end, &dummy);
    }
    if (hint) {
        hint->p = p;
        hint->index = index;
    }
    return codepoint;
error:
    return -1;
}

static inline void utf8_get_range(const char *utf8, size_t bufsize, size_t index, size_t length, UTF8IndexHint *hint, const char** st_ret, const char** ed_ret) {
    if (st_ret) {
        utf8_get(utf8, bufsize, index, hint);
        *st_ret = hint->p;
    }
    if (ed_ret) {
        utf8_get(utf8, bufsize, index + length, hint);
        *ed_ret = hint->p;
    }
}


RevisionInfo* RevisionInfo_new(const char *author, const char *revision_str, const char* date, const char* comment) {
    RevisionInfo* result = malloc(sizeof(RevisionInfo));
    result->author = strdup(author);
    result->revision_str = strdup(revision_str);
    result->date = strdup(date);
    result->comment = strdup(comment);
    return result;
}

void RevisionInfo_free(RevisionInfo *ptr) {
    free(ptr->author);
    free(ptr->revision_str);
    free(ptr->date);
    free(ptr->comment);
    free(ptr);
}

Revision* Revision_new(RevisionInfo *revinfo, char *buffer, size_t buffer_size) {
    Revision* rev = malloc(sizeof(Revision));

    rev->info = revinfo;
    rev->buffer = buffer;
    rev->buffer_size = buffer_size;
    return rev;
}

RevisionInfo *RevisionInfo_duplicate(RevisionInfo *info) {
    return RevisionInfo_new(info->author, info->revision_str, info->date, info->comment);
}

void Revision_free(Revision *rev, bool remove_revision_info) {
    if (rev->buffer)
        free(rev->buffer);
    if (remove_revision_info) {
        RevisionInfo_free(rev->info);
    }
    free(rev);
}

DiffNodeConnection *DiffNodeConnection_new(enum diff_node_type node_type, int diff_distance, DiffNode *del_node, DiffNode *ins_node) {
    assert (node_type != diff_node_type_word);

    DiffNode_obtain(del_node);
    DiffNode_obtain(ins_node);

    DiffNodeConnection* conn = malloc(sizeof(DiffNodeConnection));
    conn->node_type = node_type;
    conn->diff_distance = diff_distance;
    conn->diff_matches = varray_init();
    conn->full_match = diff_distance == 0;
    conn->del_node = del_node;
    conn->ins_node = ins_node;
    return conn;
}

DiffMatch* DiffNodeConnection_add(DiffNodeConnection *conn, size_t del_off, size_t ins_off, size_t len, DiffNodeConnection *subconn) {
    DiffMatch *match = malloc(sizeof(DiffMatch));
    match->del_off = del_off;
    match->ins_off = ins_off;
    match->len = len;
    match->subconn = subconn;

    varray_push(conn->diff_matches, match);
    return match;
}

static void free_diff_match(DiffMatch *match) {
    if (match->subconn)
        DiffNodeConnection_free(match->subconn);
    free(match);
}


void DiffNodeConnection_free(DiffNodeConnection *conn) {
    DiffNode_release(conn->del_node);
    DiffNode_release(conn->ins_node);
    varray_free(conn->diff_matches, (void (*)(void *))free_diff_match);
    free(conn);
}

DiffNode* DiffNode_new(enum diff_node_type type, const RevisionInfo *owner, const Revision *source_revision, size_t source_offset, size_t source_len) {
    DiffNode *node = malloc(sizeof(DiffNode));
    node->refcount = 1;
    node->type = type;

    node->owner = owner;

    node->source_revision = source_revision;
    node->source_offset = source_offset;
    node->source_len = source_len;

    node->children = varray_init();
    return node;
}

DiffNode* DiffNode_shallow_clone(const DiffNode* src, bool copy_children) {
    size_t idx, len;
    DiffNode *node = malloc(sizeof(DiffNode));
    memcpy(node, src, sizeof(DiffNode));
    node->refcount = 1;
    if (copy_children) {
        node->children = varray_initc(varray_length(src->children));
        varray_copy(src->children, node->children);
        for (idx = 0, len = varray_length(node->children); idx < len; idx++) {
            DiffNode_obtain((DiffNode *)varray_get(node->children, idx));
        }
    } else {
        node->children = varray_init();
    }
    return node;
}

void DiffNode_add_child(DiffNode *parent, DiffNode *node) {
    DiffNode_obtain(node);
    varray_length(parent->children);
    varray_push(parent->children, node);
}

/*
 * Paragraph seperator: '\n'
 * Sentence seperator: both '.' and '||' (only when setence starts with '|')
 */
static void parse_sentence(DiffNode *paragraph, const Revision *rev, bool sep_dpipe, size_t paragraph_offset, const char *buffer, size_t buffer_size) {
    if (buffer_size == 0)
        return;

    const char *border = buffer + buffer_size;
    const char *p = buffer;

    size_t acc_offset = 0;
    while (p < border) {
        size_t str_length = 0;
        const char *sstart = p;

        size_t cur_offset = acc_offset;
        while (p < border) {
            int32_t c;
            if ((c = iter_utf8(p, border, &p)) == -1) {
                p++;
            }
            acc_offset++;
            str_length++;
            if (c == '.') {
                while (p < border && *p == '.') {
                    acc_offset++;
                    str_length++;
                    p++;
                }
                break;
            }
        }
        const char *send = p;
        if (sstart < send) {
            DiffNode *sentence = DiffNode_new(diff_node_type_sentence, rev->info, rev, cur_offset + paragraph_offset, str_length);
            DiffNode *word = DiffNode_new(diff_node_type_word, rev->info, rev, cur_offset + paragraph_offset, str_length);
            DiffNode_add_child(sentence, word);
            DiffNode_release(word);

            DiffNode_add_child(paragraph, sentence);
            DiffNode_release(sentence);
        }
    }
}

DiffNode* DiffNode_parse(const Revision *rev) {
    DiffNode *article = DiffNode_new(diff_node_type_article, rev->info, rev, 0, 0);
    const char *buffer = rev->buffer;
    size_t buffer_size = rev->buffer_size;

    const char *border = buffer + buffer_size;

    const char *p = buffer;
    size_t acc_offset = 0;
    while (p < border) {
        size_t str_length = 0;
        const char *pstart = p;
        size_t cur_offset = acc_offset;
        while (p < border) {
            int32_t c;
            if ((c = iter_utf8(p, border, &p)) == -1) {
                p++;
            } 
            acc_offset++;
            str_length++;
            if (c == '\n') {
                while (p < border && *p == '\n') {
                    acc_offset++;
                    str_length++;
                    p++;
                }
                break;
            }
        }
        const char *pend = p;
        if (pstart < pend) {
            DiffNode *paragraph = DiffNode_new(diff_node_type_paragraph, rev->info, rev, cur_offset, str_length);

            parse_sentence(paragraph, rev, true, cur_offset, pstart, pend - pstart);
            DiffNode_add_child(article, paragraph);
            DiffNode_release(paragraph);
        }
    }
    article->source_len = acc_offset;
    return article;
}


void DiffNode_obtain(DiffNode *node) {
    if (node)
        node->refcount++;
}


void DiffNode_release(DiffNode *node) {
    if (node && --node->refcount <= 0) {
        varray_free(node->children, (void (*)(void *))DiffNode_release);
        free(node);
    }
}

static int idt_idx_fn(const void* data, int offset, void* context) {
    return offset;
}

struct txt_diff_ctx {
    const char *old_buf;
    size_t old_buf_size;
    const char *new_buf;
    size_t new_buf_size;
    size_t old_node_offset, new_node_offset;
    UTF8IndexHint *old_hint, *new_hint;
};

static int txt_cmp_fn(int idxA, int idxB, void *context) {
    struct txt_diff_ctx *ctx = context;
    int32_t a = utf8_get(ctx->old_buf, ctx->old_buf_size, ctx->old_node_offset + (size_t)idxA, ctx->old_hint);
    int32_t b = utf8_get(ctx->new_buf, ctx->new_buf_size, ctx->new_node_offset + (size_t)idxB, ctx->new_hint);
    return (a > b)? 1 : ((a < b)? -1 : 0);
}

typedef struct {
    DiffOption public_option;
    UTF8IndexHint old_hint, new_hint;
} DiffInternalOption;

struct node_diff_ctx {
    const DiffNode *old_node, *new_node;
    DiffInternalOption *option;
};


static int node_idx_fn(const void* data, int offset, void* context) {
    return offset;
}

static bool diff_node(DiffNode *old_node, DiffNode *new_node, DiffInternalOption *option, DiffNodeConnection **conn_ret);
static int node_cmp_fn(int idxA, int idxB, void *context) {
    struct node_diff_ctx *ctx = (struct node_diff_ctx *)context;
    assert (idxA < varray_length(ctx->old_node->children));
    assert (idxB < varray_length(ctx->new_node->children));

    DiffNode* old = varray_get(ctx->old_node->children, idxA);
    DiffNode* new_ = varray_get(ctx->new_node->children, idxB);
    if (diff_node(old, new_, ctx->option, NULL)) {
        return 0;
    } else {
        return 1;
    }
}


static bool coherency_test_counter(const struct diff_edit *ed, int ed_len, int max_counter) {
    // TODO
    return true;
}

static bool coherency_test_percentage(const struct diff_edit *ed, int ed_len, double percentage) {
    // TODO
    return true;
}

static void add_matched_nodes(DiffNodeConnection *conn, DiffNode *old_node, DiffNode *new_node, DiffInternalOption *option, size_t del_off, size_t ins_off, size_t len) {
    size_t idx;
    for (idx = 0; idx < len; idx++) {
        DiffNode *del_child = varray_get(old_node->children, idx + del_off);
        DiffNode *ins_child = varray_get(new_node->children, idx + ins_off);
        DiffNodeConnection *subconn;
        bool dret = diff_node(del_child, ins_child, option, &subconn);
        assert (dret == true);

        DiffNodeConnection_add(conn, idx + del_off, idx + ins_off, 1, subconn);
    }
}

static int nodewise_diff(DiffNode *old_node, DiffNode *new_node, DiffInternalOption *option, struct diff_edit **node_ed_ret, int *node_ed_len_ret) {
    struct node_diff_ctx ctx = {
        .old_node = old_node,
        .new_node = new_node,
        .option = option
    };

    enum diff_node_type node_type = new_node->type;

    int old_children_len = (int)varray_length(old_node->children);
    int new_children_len = (int)varray_length(new_node->children);

    int dmax;
    switch (option->public_option.dmax_algorithm) {
    case diff_dmax_none:
        dmax = INT_MAX;
        break;
    case diff_dmax_counter:
        dmax = option->public_option.dmax_arg[node_type].i;
        break;
    case diff_dmax_percentage:
        dmax = (int)(option->public_option.dmax_arg[node_type].d * MIN(old_children_len, new_children_len));
        break;
    default:
        assert (0); // NON REACHABLE
    }

    int diff_distance = 0;
    if (dmax > 0) {
        diff_distance = diff(old_node, 0, old_children_len, new_node, 0, new_children_len, node_idx_fn, node_cmp_fn, &ctx, dmax, node_ed_ret, node_ed_len_ret);
    } else {
        diff_distance = -1;
    }
    return diff_distance;
}

static bool diff_node(DiffNode *old_node, DiffNode *new_node, DiffInternalOption *option, DiffNodeConnection **conn_ret) {
    assert (new_node->type == old_node->type);
    assert (new_node->type != diff_node_type_word);

    struct diff_edit *ed = NULL;
    int ed_len;
    int diff_distance;
    int idx;
    enum diff_node_type node_type = new_node->type;

    DiffNodeConnection *result = NULL;


    UTF8IndexHint *old_hint = &option->old_hint;
    UTF8IndexHint *new_hint = &option->new_hint;
    size_t old_buf_utf8_len = old_node->source_len;
    size_t new_buf_utf8_len = new_node->source_len;

    if (node_type != diff_node_type_article) {
        struct txt_diff_ctx ctx = {
            .old_buf = old_node->source_revision->buffer,
            .old_buf_size = old_node->source_revision->buffer_size,
            .old_node_offset = old_node->source_offset,
            .new_buf = new_node->source_revision->buffer,
            .new_buf_size = new_node->source_revision->buffer_size,
            .new_node_offset = new_node->source_offset,
            .old_hint = old_hint,
            .new_hint = new_hint,
        };

        int dmax;
        switch (option->public_option.dmax_algorithm) {
        case diff_dmax_none:
            dmax = (old_buf_utf8_len + new_buf_utf8_len) / 2;
            break;
        case diff_dmax_counter:
            dmax = option->public_option.dmax_arg[node_type].i;
            break;
        case diff_dmax_percentage:
            dmax = (int)(option->public_option.dmax_arg[node_type].d * MIN(old_buf_utf8_len, new_buf_utf8_len));
            break;
        default:
            assert (0); // NON REACHABLE
        }
        if (dmax > 0) {
            diff_distance = diff(NULL, 0, old_buf_utf8_len, NULL, 0, new_buf_utf8_len, idt_idx_fn, txt_cmp_fn, &ctx, dmax, &ed, &ed_len);
        } else
            diff_distance = -1;
        if (diff_distance < 0)
            goto error;
    }

    switch (option->public_option.coherency_algorithm) {
    case diff_coherency_none:
        break;
    case diff_coherency_counter:
        if (!coherency_test_counter(ed, ed_len, option->public_option.coherency_arg[node_type].i)) goto error;
        break;
    case diff_coherency_percentage:
        if (!coherency_test_percentage(ed, ed_len, option->public_option.coherency_arg[node_type].d)) goto error;
        break;
    default:
        assert(0); // NON REACHABLE
    }

    if (conn_ret) {
        result = DiffNodeConnection_new(node_type, diff_distance, old_node, new_node);
        if (node_type == diff_node_type_sentence) {
            size_t ins_off = 0;
            for (idx = 0; idx < ed_len; idx++) {
                struct diff_edit *e = &ed[idx];
                switch (e->op) {
                case DIFF_MATCH:
                    DiffNodeConnection_add(result, (size_t)e->off, ins_off, (size_t)e->len, NULL);
                    ins_off += e->len;
                    break;
                case DIFF_DELETE:
                    break;
                case DIFF_INSERT:
                    ins_off += e->len;
                    break;
                }
            }
        } else {
            struct diff_edit *node_ed;
            int node_ed_len;
            int node_diff_distance;
            if ((node_diff_distance = nodewise_diff(old_node, new_node, option, &node_ed, &node_ed_len)) == -1)
                goto error;

            result->diff_distance = node_diff_distance;
            size_t ins_off = 0;
            for (idx = 0; idx < node_ed_len; idx++) {
                struct diff_edit *e = &node_ed[idx];
                switch (e->op) {
                case DIFF_MATCH:
                    add_matched_nodes(result, old_node, new_node, option, e->off, ins_off, e->len);
                    ins_off += e->len;
                    break;
                case DIFF_DELETE:
                    break;
                case DIFF_INSERT:
                    ins_off += e->len;
                    break;
                }
            }
            free(node_ed);
        }
        *conn_ret = result;
    }
    if (ed)
        free(ed);
    return true;
error:
    if (result)
        DiffNodeConnection_free(result);
    if (ed)
        free(ed);
    if (conn_ret)
        *conn_ret = NULL;
    return false;
}


/*
 * JSON format
 * -----
 *  {
 *      document: string
 *      revision_info: list of
 *      {
 *          author: string,
 *          revision_str: string,
 *          datetime: string (in format of %Y-%m-%d %H:%M:%S)
 *      }
 *      source: string
 *      source_revision_str: string
 *      previous_revision_str: string
 *
 *      nodes: list of {
 *          type: string in "word", "sentence", "paragraph", "article"
 *          owner_revision_str: string
 *          source_revision_str: string
 *          source_offset: int
 *          source_len: int
 *          children: list of document of the same format of itself
 *      }
 *  }
 */

DiffNodeConnection* DiffNode_diff(DiffNode *old_node, DiffNode *new_node, const DiffOption *option) {
    DiffInternalOption internal_option = {
        .public_option = *option,
        .old_hint = {old_node->source_revision->buffer, 0},
        .new_hint = {new_node->source_revision->buffer, 0}
    };
    DiffNodeConnection* conn;
    diff_node(old_node, new_node, &internal_option, &conn);
    return conn;
}


DiffNodeConnection* Revision_diff(const Revision *old_rev, const Revision *new_rev, const DiffOption *option) {
    DiffNode *old_node = DiffNode_parse(old_rev);
    DiffNode *new_node = DiffNode_parse(new_rev);

    DiffNodeConnection *conn = DiffNode_diff(old_node, new_node, option);

    DiffNode_release(old_node);
    DiffNode_release(new_node);
    return conn;
}


void namublame_init(NamuBlameContext *ctx, const char *document, Revision *initial_revision) {
    ctx->document = strdup(document);
    ctx->revision_info_array = varray_init();
    ctx->source_revision = initial_revision;
    ctx->previous_revision_str = NULL;
    ctx->article = DiffNode_parse(initial_revision);

    varray_push(ctx->revision_info_array, RevisionInfo_duplicate(initial_revision->info));
}


void namublame_remove(NamuBlameContext* ctx) {
    free(ctx->document);
    Revision_free(ctx->source_revision, false);
    varray_free(ctx->revision_info_array, (void (*)(void *))RevisionInfo_free);
    DiffNode_release(ctx->article);

    if (ctx->previous_revision_str)
        free(ctx->previous_revision_str);
}

static int bsearch_revision_info(const DiffNode *old_node, size_t off) {
    int left, right;

    left = 0;
    right = varray_length(old_node->children)  - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        DiffNode* mid_word = varray_get(old_node->children, mid);
        size_t mid_off = mid_word->source_offset - old_node->source_offset;
        if (mid_off <= off) {
            if (off < mid_off + mid_word->source_len) {
                return mid;
            } else {
                left = mid + 1;
            }
        } else {
            right = mid - 1;
        }
    }
    return -1;
}


static void propagate_words_to_ins_node(DiffNodeConnection *conn) {
#define _DIFF_NODE_INSERT_NEW_CHUNK(off) \
    if (fresh_ins_off < (off)) { \
        DiffNode* chunk = DiffNode_new(diff_node_type_word, new_node->owner, source_revision, new_node->source_offset + fresh_ins_off, (off) - fresh_ins_off); \
        DiffNode_add_child(new_node, chunk); \
        DiffNode_release(chunk); \
    }
    const DiffNode *old_node = conn->del_node;
    DiffNode *new_node = conn->ins_node;

    assert (old_node->type == diff_node_type_sentence);
    assert (new_node->type == diff_node_type_sentence);

    size_t fresh_ins_off = 0;
    size_t idx, len;
    size_t jdx; 

    varray_free(new_node->children, (void (*)(void *))DiffNode_release);
    new_node->children = varray_init();

    const Revision* source_revision = new_node->source_revision;
    for (idx = 0, len = varray_length(conn->diff_matches); idx < len; idx++) {
        DiffMatch *match = varray_get(conn->diff_matches, idx);

        if (match->len == 0)
            continue;

        int beginning_idx = bsearch_revision_info(old_node, match->del_off);
        int end_idx = bsearch_revision_info(old_node, match->del_off + match->len - 1);
        assert (beginning_idx != -1);
        assert (end_idx != -1);

        _DIFF_NODE_INSERT_NEW_CHUNK(match->ins_off)

        for (jdx = beginning_idx; jdx <= end_idx; jdx++) {
            size_t source_offset;
            size_t source_end;
            DiffNode* old_word = varray_get(old_node->children, jdx);
            if (jdx == beginning_idx) {
                source_offset = match->ins_off + new_node->source_offset;
            } else {
                source_offset = old_word->source_offset - old_node->source_offset + new_node->source_offset + match->ins_off - match->del_off;
            }
            if (jdx == end_idx) {
                source_end = match->ins_off + match->len + new_node->source_offset;
            } else {
                source_end = (old_word->source_offset + old_word->source_len) - old_node->source_offset + new_node->source_offset + match->ins_off - match->del_off;
            }

            DiffNode* chunk = DiffNode_new(diff_node_type_word, old_word->owner, source_revision, source_offset, source_end - source_offset);
            DiffNode_add_child(new_node, chunk);
            DiffNode_release(chunk);
        }
        fresh_ins_off = match->ins_off + match->len;
    }
    if (!varray_is_empty(conn->diff_matches)) {
        DiffMatch *last_match = varray_get_last_item(conn->diff_matches);
        _DIFF_NODE_INSERT_NEW_CHUNK(last_match->ins_off + last_match->len);
    }
}

static void propagate_owner_to_ins_node(DiffNodeConnection *conn) {
    size_t idx, len;
    if (conn->node_type == diff_node_type_sentence) {
        propagate_words_to_ins_node(conn);
    } else {
        for (idx = 0, len = varray_length(conn->diff_matches); idx < len; idx++) {
            DiffMatch *match = varray_get(conn->diff_matches, idx);
            match->subconn->ins_node->owner = match->subconn->del_node->owner;
            propagate_owner_to_ins_node(match->subconn);
        }
    }
}


int namublame_add(NamuBlameContext *ctx, Revision *revision, const DiffOption *option) {
    int diff_distance;
    RevisionInfo *revinfo = revision->info;
    varray_push(ctx->revision_info_array, revinfo);

    DiffNode *rev_node = DiffNode_parse(revision);
    // Do diff
    DiffNodeConnection* conn = DiffNode_diff(ctx->article, rev_node, option);
    if (conn) {
        diff_distance = conn->diff_distance;
        propagate_owner_to_ins_node(conn);
        DiffNodeConnection_free(conn);
    } else {
        diff_distance = -1;

        if (ctx->previous_revision_str)
            free(ctx->previous_revision_str);
        ctx->previous_revision_str = strdup(ctx->source_revision->info->revision_str);
    }

    Revision_free(ctx->source_revision, false);
    ctx->source_revision = revision;
    ctx->article = rev_node;

    return diff_distance;
}

const Revision* namublame_recent_revision(const NamuBlameContext *ctx) {
    return ctx->source_revision;
}

DiffNode* namublame_obtain_article(const NamuBlameContext *ctx) {
    DiffNode *result = ctx->article;
    DiffNode_obtain(result);
    return result;
}

enum namublame_error namublame_serialize(const NamuBlameContext *ctx, const char **buf_ret, size_t *buf_size_ret) {
    // TODO
    return namublame_error_ok;
}

enum namublame_error namublame_deserialize(NamuBlameContext *ctx, const char *buf, size_t buf_size) {
    // TODO
    return namublame_error_ok;
}


static RevisionInfo* namublame_get_revision_info(const NamuBlameContext *ctx, const char *revision_str) {
    int idx, len;
    for (len = varray_length(ctx->revision_info_array), idx = len - 1; idx >= 0; idx--) {
        RevisionInfo *info = varray_get(ctx->revision_info_array, idx);
        if (!strcmp(info->revision_str, 
                    revision_str)) {
            return info;
        }
    }
    return NULL;
}

static const char* diff_node_type_to_str(enum diff_node_type type) {
    switch (type) {
    case diff_node_type_article:
        return "article";
    case diff_node_type_paragraph:
        return "paragraph";
    case diff_node_type_sentence:
        return "sentence";
    case diff_node_type_word:
        return "word";
    default:
        return NULL;
    }
}

static bool diff_node_type_from_str(const char *str, enum diff_node_type *result) {
    if (!strcmp(str, "article")) {
        *result = diff_node_type_article;
    } else if (!strcmp(str, "paragraph")) {
        *result = diff_node_type_paragraph;
    } else if (!strcmp(str, "sentence")) {
        *result = diff_node_type_sentence;
    } else if (!strcmp(str, "word")) {
        *result = diff_node_type_word;
    } else
        return false;
    return true;
}

JSON_Value *DiffNode_jsonify(DiffNode *node, enum namublame_error *error_ret) {
    JSON_Value *result = json_value_init_object();
    JSON_Value *children = NULL;
    const char *type = diff_node_type_to_str(node->type);
    if (type == NULL) {
        *error_ret = namublame_error_invalid_node_type;
        goto error;
    }
    json_object_set_string(json_object(result), "type", diff_node_type_to_str(node->type));
    if (node->owner == NULL) {
        *error_ret = namublame_error_invalid_owner_revision_str;
        goto error;
    }
    json_object_set_string(json_object(result), "owner_revision_str", node->owner->revision_str);
    if (node->source_revision->info == NULL) {
        *error_ret = namublame_error_invalid_source_revision;
        goto error;
    }
    json_object_set_string(json_object(result), "source_revision_str", node->source_revision->info->revision_str);
    json_object_set_number(json_object(result), "source_offset", (double)node->source_offset);
    json_object_set_number(json_object(result), "source_len", (double)node->source_len);


    int idx, len;

    children = json_value_init_array();
    for (idx = 0, len = varray_length(node->children); idx < len; idx++) {
        enum namublame_error sub_error;
        JSON_Value *child_json = DiffNode_jsonify((DiffNode *)varray_get(node->children, idx), &sub_error);
        if (!child_json) {
            *error_ret = sub_error;
            goto error;
        }
        json_array_append_value(json_array(children), child_json);
    }
    json_object_set_value(json_object(result), "children", children);
    children = NULL;

    *error_ret = namublame_error_ok;
    return result;
error:
    json_value_free(result);
    if (children)
        json_value_free(children);
    return NULL;
}

DiffNode* DiffNode_parse_json(NamuBlameContext *ctx, JSON_Value *json, enum namublame_error *error_ret) {
    DiffNode *result = NULL;
    if (json_value_get_type(json) != JSONObject) {
        *error_ret = namublame_error_invalid_json_type;
        goto error;
    }
    const char* type = json_object_get_string(json_object(json), "type");
    enum diff_node_type node_type;
    if (!type || !diff_node_type_from_str(type, &node_type)) {
        *error_ret = namublame_error_invalid_node_type;
        goto error;
    }

    const char* owner_revision_str = json_object_get_string(json_object(json), "owner_revision_str");
    const char* source_revision_str = json_object_get_string(json_object(json), "source_revision_str");

    if (!owner_revision_str) {
        *error_ret = namublame_error_invalid_owner_revision_str;
        goto error;
    }
    if (!source_revision_str) {
        *error_ret = namublame_error_invalid_source_revision;
        goto error;
    }

    RevisionInfo *owner = namublame_get_revision_info(ctx, owner_revision_str);
    if (!owner) {
        *error_ret = namublame_error_invalid_owner_revision_str;
        goto error;
    }

    JSON_Value *source_offset_val =  json_object_get_value(json_object(json), "source_offset");
    JSON_Value *source_len_val = json_object_get_value(json_object(json), "source_len");

    if (!source_offset_val || json_value_get_type(source_offset_val) != JSONNumber) {
        *error_ret = namublame_error_invalid_source_offset;
        goto error;
    }
    if (!source_len_val || json_value_get_type(source_len_val) != JSONNumber) {
        *error_ret = namublame_error_invalid_source_len;
        goto error;
    }
    size_t source_offset = (size_t)json_number(source_offset_val);
    size_t source_len = (size_t)json_number(source_len_val);

    const Revision *source_revision = ctx->source_revision;
    if (strcmp(source_revision->info->revision_str, source_revision_str) != 0) {
        *error_ret = namublame_error_revision_str_mismatch;
        goto error;
    }

    result = DiffNode_new(node_type, owner, source_revision, source_offset, source_len);

    size_t idx, len;
    JSON_Value* children_val = json_object_get_value(json_object(json), "children");
    if (!children_val || json_value_get_type(children_val) != JSONArray) {
        *error_ret = namublame_error_invalid_children;
        goto error;
    }

    JSON_Array* children = json_array(children_val);

    for (idx = 0, len = json_array_get_count(children); idx < len; idx++) {
        JSON_Value *val = json_array_get_value(children, idx);
        DiffNode *child;
        if ((child = DiffNode_parse_json(ctx, val, error_ret)) == NULL) {
            goto error;
        }
        varray_push(result->children, child);
    }
    return result;
error:
    if (result)
        DiffNode_release(result);
    return NULL;
}

#ifdef SIMPLE_NAMUDIFF_PROGRAM
#include <stdio.h>
#include "sds/sds.h"

static void print_old(DiffNodeConnection *conn, size_t old_idx, UTF8IndexHint *old_hint) {
    bool str_mode = conn->node_type == diff_node_type_sentence;
    const char *buffer = conn->del_node->source_revision->buffer;
    size_t buffer_size = conn->del_node->source_revision->buffer_size;
    printf("\x1b[31;4m");
    const char *st, *ed;
    if (str_mode) {
        utf8_get_range(buffer, buffer_size, conn->del_node->source_offset + old_idx, 1, old_hint, &st, &ed);
    } else {
        DiffNode *oc = varray_get(conn->del_node->children, old_idx);
        utf8_get_range(buffer, buffer_size, oc->source_offset, oc->source_len, old_hint, &st, &ed);
    }
    const char *p;
    for (p = st; p < ed; p++) {
        if (*p == '\n')
            printf("\\n");
        else
            putchar(*p);
    }
    printf("\x1b[0m");
}

static void print_new(DiffNodeConnection *conn, size_t new_idx, UTF8IndexHint *new_hint) {
    bool str_mode = conn->node_type == diff_node_type_sentence;
    const char *buffer = conn->ins_node->source_revision->buffer;
    size_t buffer_size = conn->ins_node->source_revision->buffer_size;
    printf("\x1b[32;4m");
    const char *st, *ed;
    if (str_mode) {
        utf8_get_range(buffer, buffer_size, conn->ins_node->source_offset + new_idx, 1, new_hint, &st, &ed);
    } else {
        DiffNode *nc = varray_get(conn->ins_node->children, new_idx);
        utf8_get_range(buffer, buffer_size, nc->source_offset, nc->source_len, new_hint, &st, &ed);
    }
    fwrite(st, 1, ed - st, stdout);
    printf("\x1b[0m");
}

static void print_conn(DiffNodeConnection *conn, UTF8IndexHint *old_hint, UTF8IndexHint *new_hint) {
    size_t old_idx = 0;
    size_t new_idx = 0;
    int match_idx = 0;
    int match_len = varray_length(conn->diff_matches);

    bool str_mode = conn->node_type == diff_node_type_sentence;
    while (1) {
        if (match_idx >= match_len) {
            if (str_mode) {
                if (old_idx < conn->del_node->source_len) {
                    print_old(conn, old_idx, old_hint);
                    old_idx++;
                } else if (new_idx < conn->ins_node->source_len) {
                    print_new(conn, new_idx, new_hint);
                    new_idx++;
                } else
                    break;
            } else {
                if (old_idx < varray_length(conn->del_node->children)) {
                    print_old(conn, old_idx, old_hint);
                    old_idx++;
                } else if (new_idx < varray_length(conn->ins_node->children)) {
                    print_new(conn, new_idx, new_hint);
                    new_idx++;
                } else
                    break;
            }
        } else {
            DiffMatch *match = varray_get(conn->diff_matches, match_idx);
            if (old_idx < match->del_off) {
                print_old(conn, old_idx, old_hint);
                old_idx++;
            } else if (new_idx < match->ins_off) {
                print_new(conn, new_idx, new_hint);
                new_idx++;
            } else {
                if (str_mode) {
                    const char *st, *ed;
                    utf8_get_range(conn->del_node->source_revision->buffer, conn->del_node->source_revision->buffer_size, conn->del_node->source_offset + match->del_off, match->len, old_hint, &st, &ed);
                    fwrite(st, 1, ed - st, stdout);
                } else {
                    print_conn(match->subconn, old_hint, new_hint);
                }
                old_idx += match->len;
                new_idx += match->len;
                match_idx++;
            }
        }
    }
}

#define PRINT_INDENT do { \
    int idx = 0; \
    for (idx = 0; idx < indent; idx++) \
        printf(" ");\
    } while (0)
static void print_node(DiffNode *node, int indent) {
    int idx;

    PRINT_INDENT;
    printf("type: %s\n", diff_node_type_to_str(node->type));
    PRINT_INDENT;
    printf("owner: %s\n", node->owner->author);
    PRINT_INDENT;
    printf("source_offset: %zu\n", node->source_offset);
    PRINT_INDENT;
    printf("source_len: %zu\n", node->source_len);
    PRINT_INDENT;
    printf("children =>\n");
    for (idx = 0; idx < varray_length(node->children); idx++) { 
        DiffNode *subnode = varray_get(node->children, idx);
        print_node(subnode, indent + 4);
    }
}

static void print_conn_ast(DiffNodeConnection *conn, int indent) {
    int idx;

    PRINT_INDENT;
    printf("node_type: %s\n", diff_node_type_to_str(conn->node_type));
    PRINT_INDENT;
    printf("diff_distance: %d\n", (conn->diff_distance));
    PRINT_INDENT;
    printf("== Children == \n");
    for (idx = 0; idx < varray_length(conn->diff_matches); idx++) {
        DiffMatch *match = varray_get(conn->diff_matches, idx);
        PRINT_INDENT;
        printf("del_off: %zu\n", match->del_off);
        PRINT_INDENT;
        printf("ins_off: %zu\n", match->ins_off);
        PRINT_INDENT;
        printf("len: %zu\n", match->len);
        PRINT_INDENT;
        printf("subconn =>\n");
        if (match->subconn)
            print_conn_ast(match->subconn, indent + 4);
        printf("\n");
    }
    PRINT_INDENT;
    printf("== End of children. ==\n");
}


int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage -- [program] old_file new_file\n");
        return 1;
    }
    char *oldfile_path = argv[1];
    char *newfile_path = argv[2];
    FILE *oldfile = fopen(oldfile_path, "r");
    FILE *newfile = fopen(newfile_path, "r");
    if (!oldfile) {
        printf("Can't open the file at %s\n", oldfile_path);
        return 1;
    }
    if (!newfile) {
        printf("Can't open the file at %s\n", newfile_path);
        return 1;
    }
    long oldfile_len;
    fseek(oldfile, 0, SEEK_END);
    oldfile_len = ftell(oldfile);
    fseek(oldfile, 0, SEEK_SET);

    long newfile_len;
    fseek(newfile, 0, SEEK_END);
    newfile_len = ftell(newfile);
    fseek(newfile, 0, SEEK_SET);

    char* oldbuf = calloc(oldfile_len + 1, 1);
    char* newbuf = calloc(newfile_len + 1, 1);

    
    fread(oldbuf, 1, oldfile_len, oldfile);
    fread(newbuf, 1, newfile_len, newfile);

    RevisionInfo *old_revinfo = RevisionInfo_new("Olivia", "r1", "2012-11-05 12:02:52", "Initial document.");
    RevisionInfo *new_revinfo = RevisionInfo_new("Ned", "r2", "2012-12-01 21:30:00", "Added some description.");

    Revision* old_rev = Revision_new(old_revinfo, oldbuf, oldfile_len);
    Revision* new_rev = Revision_new(new_revinfo, newbuf, newfile_len);

    DiffOption opt = {.dmax_algorithm = diff_dmax_none, .coherency_algorithm = diff_coherency_none};
    DiffNodeConnection *conn = Revision_diff(old_rev, new_rev, &opt);
    if (!conn) {
        printf("Cannot differnciate two revisions\n");
        goto error;
    }
    /*
    printf("OLD NODE\n ===\n");
    print_node(conn->del_node, 0);
    printf("NEW NODE\n ===\n");
    print_node(conn->ins_node, 0);
    printf("=== CONNECTION ===\n");
    print_conn_ast(conn, 0);
    */

    UTF8IndexHint old_hint = {conn->del_node->source_revision->buffer, 0}, new_hint = {conn->ins_node->source_revision->buffer, 0};
    print_conn(conn, &old_hint, &new_hint);

    DiffNodeConnection_free(conn);

    Revision_free(old_rev, true);
    Revision_free(new_rev, true);

error:
    fclose(oldfile); 
    fclose(newfile);
    return 0;
}
#elif SIMPLE_NAMUBLAME_PROGRAM
int main(int argc, char **argv) {
    // TODO
    return 0;
}
#endif
