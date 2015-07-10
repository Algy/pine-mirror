#ifndef _DIFF_H
#define _DIFF_H
#include <stdbool.h>
#include <wchar.h>

#include "varray.h"
#include "parson/parson.h"

typedef int (*diff_idx_fn)(const void* data, int offset, void* context);
typedef int (*diff_cmp_fn)(int idxA, int idxB, void* context);

enum diff_operation {
    DIFF_MATCH = 1,
    DIFF_INSERT,
    DIFF_DELETE
};

struct diff_edit {
    enum diff_operation op;
    int len;
    int off;
};

int diff(const void *a, int aoff, int n,
         const void *b, int boff, int m,
         diff_idx_fn idx, diff_cmp_fn cmp, void *context, int dmax,
         struct diff_edit **ses_ret, int *ses_n_ret);


enum diff_node_type {
    diff_node_type_word = 0,
    diff_node_type_sentence,
    diff_node_type_paragraph,
    diff_node_type_article,
    diff_node_type_N,
};

enum diff_coherency_algorithm {
    diff_coherency_none,
    diff_coherency_counter,
    diff_coherency_percentage
};

enum diff_dmax_algorithm {
    diff_dmax_none,
    diff_dmax_counter,
    diff_dmax_percentage
};

enum namublame_error {
    namublame_error_ok,
    namublame_error_invalid_json,
    namublame_error_invalid_json_type,
    namublame_error_invalid_node_type,
    namublame_error_invalid_owner_revision_id,
    namublame_error_invalid_diff_distance,
    namublame_error_invalid_source_revision,
    namublame_error_invalid_source_offset,
    namublame_error_invalid_source_len,
    namublame_error_invalid_index_array,
    namublame_error_invalid_children,
    namublame_error_revision_id_mismatch
};

typedef struct {
    char *author;
    int revision_id;
    char *date; // %Y-%m-%d %H:%M:%S
    char *comment;
} RevisionInfo;

typedef struct {
    RevisionInfo* info;
    int32_t *uni_buffer;
    size_t uni_len;
    char *buffer;
    size_t buffer_size; // the number of size of buffer (in bytes)
} Revision;

typedef struct DiffNode {
    int refcount;
    enum diff_node_type type;

    const RevisionInfo *owner;

    const Revision *source_revision;
    size_t source_offset;
    size_t source_len; // utf8 length

    varray* children;
} DiffNode;

typedef struct {
    enum diff_node_type node_type;
    int diff_distance;
    bool full_match;
    DiffNode *del_node, *ins_node;
    varray *diff_matches;
} DiffNodeConnection;

typedef struct {
    size_t del_off, ins_off;
    size_t len; // if parent's node_type is diff_node_type_sentence, it is the utf8 length of word that two sentences share. Otherwise, it is always 1.
    DiffNodeConnection* subconn; // if parent's node type is diff_node_type_sentence, the value is NULL
} DiffMatch;

typedef struct {
    enum diff_dmax_algorithm dmax_algorithm;
    enum diff_coherency_algorithm coherency_algorithm;
    union {
        int i;
        double d;
    } dmax_arg[diff_node_type_N], coherency_arg[diff_node_type_N];
} DiffOption;

typedef struct {
    char* document;
    varray* revision_info_array;
    Revision *source_revision;
    int previous_revision_id; // may be NULL
    DiffNode* article;
} NamuBlameContext;

RevisionInfo* RevisionInfo_new(const char *author, int revision_id, const char* date, const char* comment);
void RevisionInfo_free(RevisionInfo *ptr);

Revision* Revision_new(RevisionInfo *revinfo, char *buffer, size_t buffer_size);

void Revision_free(Revision *rev, bool remove_revision_info);

DiffNodeConnection *DiffNodeConnection_new(enum diff_node_type node_type, int diff_distance, DiffNode *del_node, DiffNode *ins_node);
DiffMatch* DiffNodeConnection_add(DiffNodeConnection *conn, size_t del_off, size_t ins_off, size_t len, DiffNodeConnection *subconn);
void DiffNodeConnection_free(DiffNodeConnection *conn);

DiffNode* DiffNode_new(enum diff_node_type type, const RevisionInfo *owner, const Revision *source_revision, size_t source_offset, size_t source_len);
DiffNode* DiffNode_shallow_clone(const DiffNode* src, bool copy_children);
void DiffNode_add_child(DiffNode *parent, DiffNode *node);
DiffNode* DiffNode_parse(const Revision *rev);
void DiffNode_obtain(DiffNode *node);
void DiffNode_release(DiffNode *node);

JSON_Value *DiffNode_jsonify(DiffNode *node, enum namublame_error *error_ret);
DiffNode* DiffNode_parse_json(NamuBlameContext *ctx, JSON_Value *json, enum namublame_error *error_ret);

DiffNodeConnection* DiffNode_diff(DiffNode *old_node, DiffNode *new_node, const DiffOption *option);
DiffNodeConnection* Revision_diff(const Revision *old_rev, const Revision *new_rev, const DiffOption *option);

// CAUTION: namublame functions 'steal' all pointers of type of Revision and free it when namublame_remove called.
void namublame_init(NamuBlameContext *ctx, const char *document, Revision *initial_revision);
int namublame_add(NamuBlameContext *ctx, Revision *revision, const DiffOption *option);
void namublame_remove(NamuBlameContext *ctx);
DiffNode* namublame_obtain_article(const NamuBlameContext *ctx);
const Revision* namublame_recent_revision(const NamuBlameContext *ctx);

#endif // _DIFF_H
