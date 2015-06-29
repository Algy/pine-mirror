/* varnam-array.h
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Navaneeth.K.N
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#ifndef VARNAM_ARRAY_H_INCLUDED_0924
#define VARNAM_ARRAY_H_INCLUDED_0924

#include <stdbool.h>


/**
 * Array to hold pointers. This expands automatically.
 *
 **/
typedef struct varray_t
{
    void **memory;
    size_t allocated;
    size_t used;
    int index;
} varray;

typedef struct vpool_t
{
    varray *array;
    int next_slot;
    varray *free_pool;
} vpool;

extern varray* 
varray_init();

extern varray* 
varray_initc(size_t capacity);

void 
varray_push(varray *array, void *data);

extern void
varray_copy(varray *source, varray *destination);

extern void
varray_remove_at(varray *array, int index);

extern int
varray_length(varray *array);

extern bool
varray_is_empty (varray *array);

extern bool
varray_exists (varray *array, void *item, bool (*equals)(void *left, void *right));

extern void
varray_clear(varray *array);

extern void*
varray_get(varray *array, int index);

extern void*
varray_get_last_item(varray *array);

extern void*
varray_pop_last_item(varray *array);

extern void
varray_insert(varray *array, int index, void *data);

extern void
varray_free(varray *array, void (*destructor)(void*));
#endif
