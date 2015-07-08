/* diff - compute a shortest edit script (SES) given two sequences
 * Copyright (c) 2004 Michael B. Allen <mba2000 ioplex.com> *
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* This algorithm is basically Myers' solution to SES/LCS with
 * the Hirschberg linear space refinement as described in the
 * following publication:
 *
 *   E. Myers, ``An O(ND) Difference Algorithm and Its Variations,''
 *   Algorithmica 1, 2 (1986), 251-266.
 *   http://www.cs.arizona.edu/people/gene/PAPERS/diff.ps
 *
 * This is the same algorithm used by GNU diff(1).
 */

/*
 * Modified by Alchan Kim, 2015
 * This file will be destributed as MIT license as the original version is.
 */

#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "namudiff.h"

#define FOWARD_V(k) _v(ctx, (k), 0)
#define REVERSE_V(k) _v(ctx, (k), 1)

#define SET_FORWARD_V(k, val) _setv(ctx, (k), 0, val)
#define SET_REVERSE_V(k, val) _setv(ctx, (k), 1, val)

struct _ctx {
    diff_idx_fn idx;
    diff_cmp_fn cmp;
    void *context;
    int *buf;
    struct diff_edit *ses;
    int ses_capacity;
    int si;
    int dmax;
    int v_size;
};

struct middle_snake {
    int x, y, u, v;
};


static inline void _setv(struct _ctx *ctx, int k, int reverse, int val) {
    int j;
    // Pack -N to N into 0 to N * 4
    //  j
    //  -2 -1  0  1
    // [pf pr nf nr] [       ] ....
    //  ^:          forward v for positive k
    //     ^:       reverse v for positive k
    //        ^:    forward v for negative k
    //           ^: reverse v for negative k
    // [ |k|=0     ] [ |k|=1 ] ...
    j = k <= 0 ? -k * 4 + reverse : k * 4 + (reverse - 2);
    assert (j >= 0);
    assert (j < ctx->v_size);
    ctx->buf[j] = val;
}

static inline int _v(struct _ctx *ctx, int k, int reverse) {
    int j;
    j = k <= 0 ? -k * 4 + reverse : k * 4 + (reverse - 2);
    assert (j >= 0);
    assert (j < ctx->v_size);

    return ctx->buf[j];
}

static int _find_middle_snake(const void *a, int aoff, int n,
                              const void *b, int boff, int m,
                              struct _ctx *ctx,
                              struct middle_snake *ms) {
    int delta, odd, mid, d;

    delta = n - m;
    odd = delta & 1;
    mid = (n + m) / 2;
    mid += odd;

    SET_FORWARD_V(1, 0);
    SET_REVERSE_V(delta - 1, n);

    for (d = 0; d <= mid; d++) {
        int k, x, y;

        if ((2 * d - 1) >= ctx->dmax) {
            return ctx->dmax;
        }

        // going ahead
        for (k = d; k >= -d; k -= 2) {
            if (k == -d || (k != d && FOWARD_V(k - 1) < FOWARD_V(k + 1))) {
                x = FOWARD_V(k + 1);
            } else {
                x = FOWARD_V(k - 1) + 1;
            }
            y = x - k;

            ms->x = x;
            ms->y = y;
            if (ctx->cmp) {
                while (x < n && y < m && 
                       ctx->cmp(ctx->idx(a, aoff + x, ctx->context),
                                ctx->idx(b, boff + y, ctx->context), ctx->context) == 0) {
                    x++; y++;
                }
            } else {
                const unsigned char *a0 = (const unsigned char *)a + aoff;
                const unsigned char *b0 = (const unsigned char *)b + boff;
                while (x < n && y < m && a0[x] == b0[y]) {
                    x++; y++;
                }
            }
            SET_FORWARD_V(k, x);

            if (odd && k >= (delta - (d - 1)) && k <= (delta + (d - 1))) {
                if (x >= REVERSE_V(k)) {
                    ms->u = x;
                    ms->v = y;
                    return 2 * d - 1;
                }
            }
        }

        // going backward
        for (k = d; k >= -d; k -= 2) {
            int kr = (n - m) + k;

            if (k == d || (k != -d && REVERSE_V(kr - 1) < REVERSE_V(kr + 1))) {
                x = REVERSE_V(kr - 1);
            } else {
                x = REVERSE_V(kr + 1) - 1;
            }
            y = x - kr;

            ms->u = x;
            ms->v = y;
            if (ctx->cmp) {
                while (x > 0 && y > 0 && ctx->cmp(ctx->idx(a, aoff + (x - 1), ctx->context),
                            ctx->idx(b, boff + (y - 1), ctx->context), ctx->context) == 0) {
                    x--; y--;
                } } else {
                const unsigned char *a0 = (const unsigned char *)a + aoff;
                const unsigned char *b0 = (const unsigned char *)b + boff;
                while (x > 0 && y > 0 && a0[x - 1] == b0[y - 1]) {
                    x--; y--;
                }
            }
            SET_REVERSE_V(kr, x);

            if (!odd && kr >= -d && kr <= d) {
                if (x <= FOWARD_V(kr)) {
                    ms->x = x;
                    ms->y = y;
                    return 2 * d;
                }
            }
        }
    }


    return -1;
}

static void _edit(struct _ctx *ctx, int op, int off, int len)
{
    struct diff_edit *e, *top;

    if (len <= 0 || !ctx->ses) {
        return;
    } 

    if (ctx->si >= ctx->ses_capacity) {
        ctx->ses_capacity *= 2;
        ctx->ses = realloc(ctx->ses, sizeof(struct diff_edit) * ctx->ses_capacity);
    }

    /* 
     * Add an edit to the SES (or
     * coalesce if the op is the same)
     */
    if (ctx->si > 0) {
        top = &ctx->ses[ctx->si - 1];
        if (top->op == op) {
            top->len += len;
            return;
        } else if (top->op == DIFF_INSERT && op == DIFF_DELETE) {
            // apply DI rule
            if (ctx->si <= 1 || ctx->ses[ctx->si - 2].op == DIFF_MATCH) {
                ctx->ses[ctx->si] = ctx->ses[ctx->si - 1];
                e = &ctx->ses[ctx->si - 1];
                e->op = op;
                e->off = off;
                e->len = len;
                ctx->si++;
                return;
            } else {
                // ctx->ses[ctx->si - 2] must be of type of DIFF_DELETE
                ctx->ses[ctx->si - 2].len += len;
                return;
            }
        }
    } 

    e = &ctx->ses[ctx->si++];
    e->op = op;
    e->off = off;
    e->len = len;
}

static int _ses(const void *a, int aoff, int n,
                const void *b, int boff, int m,
                struct _ctx *ctx)
{
    struct middle_snake ms;
    int d;

    if (n == 0) {
        _edit(ctx, DIFF_INSERT, boff, m);
        d = m;
    } else if (m == 0) {
        _edit(ctx, DIFF_DELETE, aoff, n);
        d = n;
    } else {
                    /* Find the middle "snake" around which we
                     * recursively solve the sub-problems.
                     */
        d = _find_middle_snake(a, aoff, n, b, boff, m, ctx, &ms);
        if (d == -1) {
            return -1;
        } else if (d >= ctx->dmax) {
            return ctx->dmax;
        } else if (ctx->ses == NULL) {
            return d;
        } else if (d > 1) {
            if (_ses(a, aoff, ms.x, b, boff, ms.y, ctx) == -1) {
                return -1;
            }

            _edit(ctx, DIFF_MATCH, aoff + ms.x, ms.u - ms.x);

            aoff += ms.u;
            boff += ms.v;
            n -= ms.u;
            m -= ms.v;
            if (_ses(a, aoff, n, b, boff, m, ctx) == -1) {
                return -1;
            }
        } else {
            int x = ms.x;
            int u = ms.u;

                 /* There are only 4 base cases when the
                  * edit distance is 1.
                  *
                  * n > m   m > n
                  *
                  *   -       |
                  *    \       \    x != u
                  *     \       \
                  *
                  *   \       \
                  *    \       \    x == u
                  *     -       |
                  */

            if (m > n) {
                if (x == u) {
                    _edit(ctx, DIFF_MATCH, aoff, n);
                    _edit(ctx, DIFF_INSERT, boff + (m - 1), 1);
                } else {
                    _edit(ctx, DIFF_INSERT, boff, 1);
                    _edit(ctx, DIFF_MATCH, aoff, n);
                }
            } else {
                if (x == u) {
                    _edit(ctx, DIFF_MATCH, aoff, m);
                    _edit(ctx, DIFF_DELETE, aoff + (n - 1), 1);
                } else {
                    _edit(ctx, DIFF_DELETE, aoff, 1);
                    _edit(ctx, DIFF_MATCH, aoff + 1, m);
                }
            }
        }
    }

    return d;
}

int diff(const void *a, int aoff, int n,
         const void *b, int boff, int m,
         diff_idx_fn idx, diff_cmp_fn cmp, void *context, int dmax,
         struct diff_edit **ses_ret, int *ses_n_ret) {
    int d, x, y;

    dmax = dmax >= 0? dmax : INT_MAX;
    const int initial_capacity = dmax > 128? dmax: 128;
    int v_size = 8 * (m + n + 1);
    struct _ctx ctx = {
        .idx = idx,
        .cmp = cmp,
        .context = context,
        .buf = calloc(v_size, sizeof(int)), 
        .ses = calloc(initial_capacity, sizeof(struct diff_edit)),
        .si = 0,
        .ses_capacity = initial_capacity,
        .dmax = dmax,
        .v_size = v_size
    };

    if (!idx != !cmp) { /* ensure both NULL or both non-NULL */
        goto failed;
    }

    /* The _ses function assumes the SES will begin or end with a delete
     * or insert. The following will insure this is true by eating any
     * beginning matches. This is also a quick to process sequences
     * that match entirely.
     */
    x = y = 0;
    if (cmp) {
        while (x < n && y < m && 
               cmp(idx(a, aoff + x, context),
                   idx(b, boff + y, context), context) == 0) {
            x++; y++;
        }
    } else {
        const unsigned char *a0 = (const unsigned char *)a + aoff;
        const unsigned char *b0 = (const unsigned char *)b + boff;
        while (x < n && y < m && a0[x] == b0[y]) {
            x++; y++;
        }
    }
    _edit(&ctx, DIFF_MATCH, aoff, x);

    if ((d = _ses(a, aoff + x, n - x, b, boff + y, m - y, &ctx)) == -1) {
        goto failed;
    }
    free(ctx.buf);
    if (ses_n_ret)
        *ses_n_ret = ctx.si;
    if (ses_ret)
        *ses_ret = ctx.ses;
    else
        free(ctx.ses);

    return d;
failed:
    free(ctx.buf);
    free(ctx.ses);
    if (ses_n_ret)
        *ses_n_ret = -1;
    if (ses_ret)
        *ses_ret = NULL;
    return -1;
}

#ifdef SIMPLE_DIFF_PROGRAM
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
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

static inline size_t utf8_count(const char *utf8, size_t bufsize, bool *valid_utf8) {
    const char *p = utf8, *end = utf8 + bufsize;
    bool success = true;
    size_t result = 0;
    while (p < end) { 
        if (iter_utf8(p, end, &p) == -1) {
            success = false;
            p++;
        }
        result++;
    }
    if (valid_utf8)
        *valid_utf8 = success;
    return result;
}

typedef struct {
    const char *p;
    size_t index;
} UTF8IndexHint;

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
        if (iter_index == index)
            codepoint = iter_utf8(p, end, &dummy);
        else
            codepoint = -1;
    } else {
        while (p < end && iter_index < index) {
            if (iter_utf8(p, end, &p) == -1)
                p++;
            iter_index++;
        }
        if (iter_index == index)
            codepoint = iter_utf8(p, end, &dummy);
        else
            codepoint = -1;
    }
    if (hint) {
        hint->p = p;
        hint->index = iter_index;
    }
    return codepoint;
}

struct utf8_ctx {
    char *bufA, *bufB;
    size_t bufsizeA, bufsizeB;
    UTF8IndexHint hintA, hintB;
};

int utf8_idx_fn(const void *data, int offset, void *context) {
    return offset;
}

int utf8_cmp_fn(int idxA, int idxB, void* context) {
    struct utf8_ctx *ctx = context;
    int32_t lhs, rhs;
    lhs = utf8_get(ctx->bufA, ctx->bufsizeA, idxA, &ctx->hintA);
    rhs = utf8_get(ctx->bufB, ctx->bufsizeB, idxB, &ctx->hintB);
    return (lhs > rhs)? 1 : ((lhs < rhs)? -1 : 0);
}

int main (int argc, char** argv) {
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

    int sn, idx;
    struct diff_edit* ses = NULL;

    struct utf8_ctx ctx = {
        .bufA = oldbuf,
        .bufB = newbuf,
        .bufsizeA = oldfile_len,
        .bufsizeB = newfile_len,
        .hintA = {oldbuf, 0},
        .hintB = {newbuf, 0}
    };
    int ret = diff(NULL, 0, utf8_count(oldbuf, oldfile_len, NULL), 
                   NULL, 0, utf8_count(newbuf, newfile_len, NULL),
                   utf8_idx_fn, utf8_cmp_fn, &ctx, 100000, &ses, &sn);

    for (idx = 0; idx < sn; idx++) {
        struct diff_edit *e = &ses[idx];
        const char *st, *ed;
        switch (e->op) {
        case DIFF_MATCH:
            utf8_get(oldbuf, oldfile_len, e->off, &ctx.hintA);
            st = ctx.hintA.p;
            utf8_get(oldbuf, oldfile_len, e->off + e->len, &ctx.hintA);
            ed = ctx.hintA.p;
            fwrite(st, 1, ed - st, stdout);
            break;
        case DIFF_INSERT:
            printf("\x1b[32;4m");
            utf8_get(newbuf, newfile_len, e->off, &ctx.hintB);
            st = ctx.hintB.p;
            utf8_get(newbuf, newfile_len, e->off + e->len, &ctx.hintB);
            ed = ctx.hintB.p;

            fwrite(st, 1, ed - st, stdout);
            printf("\x1b[0m");
            break;
        case DIFF_DELETE:
            printf("\x1b[31;4m");
            utf8_get(oldbuf, oldfile_len, e->off, &ctx.hintA);
            st = ctx.hintA.p;
            utf8_get(oldbuf, oldfile_len, e->off + e->len, &ctx.hintA);
            ed = ctx.hintA.p;
            fwrite(st, 1, ed - st, stdout);
            printf("\x1b[0m");
            break;
        }
    }
    printf("\n");
    size_t cnt = utf8_count(oldbuf, oldfile_len, NULL);
    UTF8IndexHint hint = {oldbuf, 0};
    for (idx = cnt - 1; idx >= 0; idx--) {
        int32_t codepoint = utf8_get(oldbuf, oldfile_len, idx, &hint);
        assert (codepoint != -1);
        // printf("\\u%X%X%X%X", (codepoint >> 12) & 0xF, (codepoint >> 8) & 0xF, (codepoint >> 4) & 0xF, (codepoint) & 0xF);
    }
    if (ses)
        free(ses);
    free(oldbuf);
    free(newbuf);
    fclose(oldfile); 
    fclose(newfile);
}
#endif
