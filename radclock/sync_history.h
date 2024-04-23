/*
 * Copyright (C) 2006 The RADclock Project (see AUTHORS file)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYNC_HISTORY_H
#define _SYNC_HISTORY_H


typedef unsigned long int index_t;    // sufficient even if only 4 bytes

/* Support for the storage of a limited past of a timeseries as items arrive.
 * New items are always accepted, and inserted using the stamp_i index (0,1,2,...)
 * which is mapped circularly - the index itself is never modified. When the
 * history is full, a new insertion naturally overwrites the oldest stored item.
 *
 * Given {item_count,oldest_i}, there is no need to store the last inserted index
 *  curr_i = oldest_i + item_count -1  , particularly as curr_i
 * is passed in by the calling program when needed. However doing so is clearer
 * and enables an additional useful check.
 *
 * The structure records the size of the type being stored, but not the
 * type itself, this must be cast by the calling program who is aware of which
 * time series they are operating on.
 */
typedef struct sync_hist {
	void *buffer;             // data buffer
	unsigned int buffer_sz;   // buffer size (max number of items)
	unsigned int item_count;  // current number of items held
	size_t item_sz;           // size of each item
	index_t oldest_i;         // global index of oldest item stored
	index_t newest_i;         // global index of newest item stored
} history;

int  history_init(history *hist, unsigned int buffer_sz, size_t item_sz);
void history_free(history *hist);
void history_add(history *hist, index_t i, const void *item);
index_t history_old(history *hist);
index_t history_new(history *hist);
void *history_find(history *hist, index_t index);
int history_resize(history *hist, unsigned int buffer_sz);

/* Forms that operate on vcounter_t */
index_t    history_min(history *hist, index_t j, index_t i);
index_t    history_min_slide(history *hist,        index_t index_curr,  index_t j, index_t i);
vcounter_t history_min_slide_value(history *hist, vcounter_t min_curr,  index_t j, index_t i);

/* Forms that operate on double */
index_t history_min_dbl(history *hist, index_t j, index_t i);
index_t history_min_slide_dbl(history *hist,    index_t index_curr, index_t j, index_t i);
double  history_min_slide_value_dbl(history *hist, double min_curr, index_t j, index_t i);


#endif    /* _SYNC_HISTORY_H */
