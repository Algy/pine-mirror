#ifndef _DIFF_H
#define _DIFF_H
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

#endif // _DIFF_H
