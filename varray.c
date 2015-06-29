/* Dynamically growing array implementation
 * Imported from part of libvarnam. 
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



#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "varray.h"

#define INITIAL_SIZE 32

varray*
varray_init()
{
    return varray_initc(INITIAL_SIZE);
}

varray*
varray_initc(size_t capacity) 
{
    varray *array = (varray*) malloc (sizeof(varray));
    array->memory = calloc(capacity, sizeof(void *));
    array->allocated = capacity;
    array->used = 0;
    array->index = -1;

    return array;
}

void
varray_push(varray *array, void *data)
{
    size_t toallocate;
    size_t size = sizeof(void*);

    if (data == NULL) return;

    if (array->allocated <= array->used) {
        toallocate = array->allocated == 0 ? size : (array->allocated * 2);
        array->memory = realloc(array->memory, sizeof(void *) * toallocate);
        array->allocated = toallocate;
    }

    array->memory[++array->index] = data;
    array->used++;
}

void
varray_copy(varray *source, varray *destination)
{
    int i;
    void *item;

    if (source == NULL) return;
    if (destination == NULL) return;

    for (i = 0; i < varray_length (source); i++) {
        item = varray_get (source, i);
        varray_push (destination, item);
    }
}

void*
varray_get_last_item(varray *array)
{
    assert (array);

    return varray_get (array, array->index);
}

void
varray_remove_at(varray *array, int index)
{
    int i, len;

    if (index < 0 || index > array->index)
        return;

    len = varray_length(array);
    for(i = index + 1; i < len; i++)
    {
        array->memory[index++] = array->memory[i];
    }

    array->used--;
    array->index--;
}

void*
varray_pop_last_item(varray *array)
{
    void *item;
    assert (array);

    item = varray_get_last_item (array);
    if (item != NULL)
        varray_remove_at (array, array->index);

    return item;
}

int
varray_length(varray *array)
{
    if (array == NULL)
        return 0;

    return array->index + 1;
}

void
varray_clear(varray *array)
{
    int i;
    for(i = 0; i < varray_length(array); i++)
    {
        array->memory[i] = NULL;
    }
    array->used = 0;
    array->index = -1;
}

void
varray_free(varray *array, void (*destructor)(void*))
{
    int i;
    void *item;

    if (array == NULL)
        return;

    if (destructor != NULL)
    {
        for(i = 0; i < varray_length(array); i++)
        {
            item = varray_get (array, i);
            if (item != NULL) destructor(item);
        }
    }

    if (array->memory != NULL)
        free(array->memory);
    free(array);
}

void*
varray_get(varray *array, int index)
{
    if (index < 0 || index > array->index)
        return NULL;

    assert(array->memory);

    return array->memory[index];
}

void
varray_insert(varray *array, int index, void *data)
{
    if (index < 0 || index > array->index)
        return;

    array->memory[index] = data;
}

bool
varray_is_empty (varray *array)
{
    return (varray_length (array) == 0);
}

bool
varray_exists (varray *array, void *item, bool (*equals)(void *left, void *right))
{
    int i;

    for (i = 0; i < varray_length (array); i++)
    {
        if (equals(varray_get (array, i), item))
            return true;
    }

    return false;
}
