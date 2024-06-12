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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "../config.h"
#include "radclock.h"
#include "verbose.h"
#include "sync_history.h"
#include "jdebug.h"


int history_init(history *hist, unsigned int buffer_sz, size_t item_sz)
{
	hist->buffer = malloc(buffer_sz * item_sz);
	JDEBUG_MEMORY(JDBG_MALLOC, hist->buffer);
	if (hist->buffer == NULL) {
		verbose(LOG_ERR, "malloc failed allocating memory");
		return 1;
	}
	memset(hist->buffer, 0, buffer_sz * item_sz);

	hist->buffer_sz   = buffer_sz;
	hist->oldest_i    = 0;  // ignored when item_count=0
	hist->newest_i    = -1;
	hist->item_count  = 0;
	hist->item_sz     = item_sz;

	return 0;
}

void history_free(history *hist)
{
	hist->buffer_sz  = 0;
	hist->item_count = 0;
	hist->item_sz    = 0;
	JDEBUG_MEMORY(JDBG_FREE, hist->buffer);
	free(hist->buffer);
	hist->buffer     = NULL;
}

/* Insert item with index i into history
 * If the location already holds a value, it is overwritten, and the oldest
 * element stored (smallest index value) is correspondingly lost.
 */
void history_add(history *hist, index_t i, const void *item)
{
	void *posn;

	if (i == hist->newest_i) {
		verbose(LOG_ERR, "history_add: item i=%lu is already stored, aborting", i);
		return;
	}

	posn = hist->buffer + (i % hist->buffer_sz) * hist->item_sz;
	memcpy(posn, item, hist->item_sz);
	hist->newest_i++;

	if (hist->item_count < hist->buffer_sz)  // if wasn't full yet
		hist->item_count++;  // oldest unchanged
	else
		hist->oldest_i++;    // was full, oldest just overwritten
}

/* Return the global index corresponding to the oldest entry held,
 * or an impossible index (-1) to flag an empty history. */
inline
index_t history_old(history *hist)
{
	if (hist->item_count == 0)
		return -1;
	else
		return hist->oldest_i;
}

/* Return the global index corresponding to the lastest entry held,
 * or an impossible index (-1) to flag an empty history. */
inline
index_t history_curr(history *hist)
{
	return hist->newest_i;
}

/* Get a pointer to the element with index i in history, if present.
 * It is assumed a future index (i>stamp_i) is never requested.
 * The element itself cannot be returned as its type is unknown.
 * "find" is a misnomer given its position (i) is supplied! but "get" is not
 * right either, and "get a ptr to" is too heavy.
 */
inline 
void * history_find(history *hist, index_t i)
{
	if (i < hist->oldest_i) {
		verbose(LOG_ERR, "history_find: item i=%lu absent from history, cannot access", i);
		return NULL;
	} else
		return hist->buffer + ( i % hist->buffer_sz ) * hist->item_sz;
}

/* TODO: consider creating a history_replace fn for in-situ replacment */


/* Resize history
 * Copies as many stored items as will fit into the new size, preserving the
 * stamp index positioning. The value of newest_i does not change.
 * How the algos handle perhaps not having the elements they need available in
 * the history is not handled here!
 */
int history_resize(history *hist, unsigned int new_size)
{
	unsigned long int j;
	void *new_buffer, *src, *dst;

	if (new_size == hist->buffer_sz) {
		verbose(VERB_CONTROL, "history_resize: new size is the same, nothing to do");
		return 0;
	}

	/* Allocate a new buffer to copy the correct items. Initialised to 0 */
	new_buffer = malloc(new_size * hist->item_sz);
	JDEBUG_MEMORY(JDBG_MALLOC, new_buffer);
	if (new_buffer == NULL) {
		verbose(LOG_ERR, "malloc failed allocating memory");
		return 1;
	}
	memset(new_buffer, 0, new_size * hist->item_sz);

	if (hist->item_count > 0) {
		/* Truncate held history if needed */
		if (hist->item_count > new_size) {
			hist->item_count = new_size;
			hist->oldest_i = hist->newest_i - new_size + 1;
		}

		/* Copy items maintaining stamp index positioning */
		for ( j=hist->oldest_i; j<=hist->newest_i; j++ ) {
			src = hist->buffer + ( j % hist->buffer_sz ) * hist->item_sz;
			dst = new_buffer   + ( j % new_size )        * hist->item_sz;
			memcpy(dst, src, hist->item_sz);
		}
	}

	hist->buffer_sz = new_size;

	JDEBUG_MEMORY(JDBG_FREE, hist->buffer);
	free(hist->buffer);
	hist->buffer = new_buffer;
	
	return 0;
}



/* =============================================================================
 * MINIMUM DETECTION AND TRACKING
 * ===========================================================================*/

/* Note on arg min ambiguity **
 * A given minimum M may occur at multiple indicies.
 * Although output values are not affected by this ambiguity, reported indices
 * are, and this can having knock on effects in algos as well as impacting verb.
 * The preferred index i_M associated to M is the first one seen (the smallest),
 * and this is a convention adhered to in the routines below, as well as more
 * broadly in the radclock code, through updating if  new<best, not new≤best.
 * There are several reasons for this (we use RTT as the canonical eg):
 *  - RTThat time series is fully characterized by the {(i_M,RTT_i)} of new minima
 *    Any "updating" of indices for a given RTThat level would destroy this.
 *    Another way of saying this is that `repeat indicies' are irrelevant, they
 *    do not change what matters, namely that since the (first) i_m, RTT reached
 *    a new level and never rose back above it (ignoring Upshifts here).
 *  - Updating distorts timing information, eg reducing the ∆T baseline distances,
 *    making it seem like a given RTTmin value had not reached this quality
 *    until later (maybe much later in extreme cases) than it really did
 *  - efficiency:  updating creates additional processing, it is better to allow
 *    a given new i_M to enjoy its maximum lifetime, only being updated when
 *    essential (falls out of a defined maximum window, or a smaller value
 *    is encountered [there are situations where taking the largest index in
 *    a window reduces computation locally, but this is a minor consideration).
 *
 * Although we expect RTT minima not to repeat with high resolution counters
 * such as the TSC, it may still occur, and for low resolution counters, this
 * may be quite common, making it even more important to reduce impacts, noise
 * and confusion by suppressing the updates.
 */


/* Subroutine to find the minimum of a set of contiguous array elements.
 * Finds minimum between indicies j and i inclusive,  j<=i .
 * Does NOT change array elements.
 * Returns the First (smallest) index yielding the minimal element.
 * Note i, j, and ind_curr are true indices. They are circularly mapped into
 *   the array It is up to the calling function to ensure
 *   that the values needed are still in the array.
 */

/* Version operating on vcounter_t timeseries */
//index_t history_min(vcounter_t *x, index_t j, index_t i, index_t lenx)
index_t history_min(history *hist, index_t j, index_t i)
{
   /* Current minimum found and corresponding index */
	vcounter_t *min_curr;
	vcounter_t *tmp;
	index_t ind_curr;

	if ( i < j )
		verbose(LOG_ERR,"Error in history_min, index range bad, j= %u, i= %u", j,i);

	/* Initialise at j end */
	//	min_curr = hist->buffer[j % hist->buffer_sz];    // old way
	min_curr = (vcounter_t*) history_find(hist, j);
	ind_curr = j;

	while ( j < i ) {
		j++;
		tmp = (vcounter_t*) history_find(hist, j);
		if ( *tmp < *min_curr ) {  // < not ≤ : ignore larger repeat arg min indicies
//			if ( *tmp == *min_curr )
//				verbose(LOG_ERR,"history_min: repeat minimum : j= %u, i= %u (%llu %llu)", j , i, *tmp, *min_curr);
			min_curr = tmp;
			ind_curr = j;
		}
	}
	return ind_curr;
}

/* Version operating on double timeseries */
index_t history_min_dbl(history *hist, index_t j, index_t i)
{
   /* Current minimum found and corresponding index */
	double *min_curr;
	double *tmp;
	index_t ind_curr;

	if ( i < j )
		verbose(LOG_ERR,"Error in history_min_dbl, index range bad, j= %u, i= %u", j,i);

	/* Initialise at j end */
	min_curr = history_find(hist, j);
	ind_curr = j;

	while ( j < i ) {
		j++;
		tmp = history_find(hist, j);
		if ( *tmp < *min_curr ) {
			min_curr = tmp;
			ind_curr = j;
		}
	}

	return ind_curr;
}




/* Subroutines to efficiently update the minimum of a sliding window operating
 * over a time series held in contiguous array elements.
 * Array elements are Not altered.
 * The current window is defined by the indicies [j,i] inclusively, j<=i, and
 * The window slides by 1:  old element j is dropped in favor of new element at
 * i+1, that is assumed to already be held in the history.
 * All indicies are unique time-series indicies that never wrap, they are
 * circularly mapped into the array of finite length. It is up to the calling
 * function to ensure that the values needed are still in the array.
 *
 * The point of sliding functions is efficiency: to reduce computation when
 * possible, only using brute force searching (using history_min) when essential.
 */


/* Forms that takes the index of the current minimum and returns the new index.
 */

/* Version operating on vcounter_t timeseries */
index_t history_min_slide(history *hist, index_t index_curr,  index_t j, index_t i)
{
	vcounter_t *tmp_curr;
	vcounter_t *new;

	if ( i < j ) {
		verbose(LOG_ERR,"Error in min_slide, window width < 1: %u %u %u", j,i,i-j+1);
		return i+1;
	}
	/* Window only 1 wide anyway, easy */
	if (i == j)
		return i+1;

	/* New one must be new min */
	new      = history_find(hist, i+1);
	tmp_curr = history_find(hist, index_curr);
	if ( *new < *tmp_curr )  // equality should be impossible, but best to update
		return i+1;

	/* One being dropped was min, must do work */
	if (j == index_curr)
		return history_min(hist, j+1, i+1);  // i+1 may be min now old min gone

	/* min_curr inside window and still valid, easy */
	return index_curr;
}

/* Version operating on double timeseries */
index_t history_min_slide_dbl(history *hist, index_t index_curr,  index_t j, index_t i)
{
	double *tmp_curr;
	double *tmp;

	if ( i < j ) {
		verbose(LOG_ERR,"Error in min_slide_dbl, window width less than 1: %u %u %u", j,i,i-j+1);
		return i+1;
	}
	/* Window only 1 wide anyway, easy */
	if (i == j)
		return i+1;

	/* New one must be new min */
	tmp = history_find(hist, i+1);
	tmp_curr = history_find(hist, index_curr);
	if ( *tmp < *tmp_curr )
		return i+1;

	/* One being dropped was min, must do work */
	if (j == index_curr)
		return history_min(hist, j+1, i+1);  // i+1 may be min now old min gone

	/* min_curr inside window and still valid, easy */
	return index_curr;
}




/* Forms that operates on values rather than indices
 * Must initialise properly, if the min is not in the window, may never be replaced!
 */

/* Version operating on vcounter_t timeseries */
vcounter_t history_min_slide_value(history *hist, vcounter_t min_curr, index_t j, index_t i)
{
	vcounter_t *tmp;
	index_t tmp_i;
//	static index_t catchmin_i;    // when a new min enters, record its index
//	static long exittrap = 0;

	tmp = (vcounter_t *) history_find(hist, i+1);  // new value entering

	if ( i < j ) {
		verbose(LOG_ERR,"Error in min_slide_value, window width less than 1: %u %u %u", j,i,i-j+1);
		return *tmp;
	}

	/* Window only 1 wide anyway, easy */
	if (i == j)
		return *tmp;

	/* New one must be new min */
	if ( *tmp < min_curr ) {
//		verbose(LOG_ERR,"new min case : %lu %lu mincurr =  %llu, newmin = %llu", j,i, min_curr, *tmp);
//		catchmin_i = i+1;
//		tmp = (vcounter_t *) history_find(hist, catchmin_i);
//		verbose(LOG_ERR,"caughtnewmin:  index %llu   value:  %llu", catchmin_i, *tmp);
//		exittrap=1;
		return *tmp;
	}
	
//	if ( exittrap > 0 ) {
//		exittrap++;
//		if (exittrap <10) {
//			tmp = (vcounter_t *) history_find(hist, catchmin_i);
//			verbose(LOG_ERR,"caughtmin:  index %llu   value:  %llu", catchmin_i, *tmp);
//		}
//	   if (exittrap > 482 - 2) {
//			tmp = (vcounter_t *) history_find(hist, catchmin_i);
//			verbose(LOG_ERR,"caughtmin:  index %llu   value:  %llu", catchmin_i, *tmp);
//			verbose(LOG_ERR,"  item  count %u  , buf_size  %u", hist->item_count, hist->buffer_sz );
//			tmp = (vcounter_t *) history_find(hist, j);
//			verbose(LOG_ERR,"context check near exit of last min: %lu %lu mincurr =  %llu, jmin = %llu", j,i, min_curr, *tmp);
//			if (exittrap > 1501 + 2) exittrap=0;
//		}
//	}

	
	/* One being dropped was min, must do work */
	tmp = (vcounter_t *) history_find(hist, j);  // value exiting
	if ( *tmp == min_curr ) {
//		verbose(LOG_ERR,"min_slide_value, do work case : %llu %llu %llu", j,i,i-j+1);
		tmp_i = history_min(hist, j+1, i+1);	// i+1 may be min now old min gone
		tmp = (vcounter_t *) history_find(hist, tmp_i);
		return *tmp;
	}

//	verbose(LOG_ERR,"min_slide_value, no change :  tmp_j = %llu  min_curr = %llu,  j - curr %llu",
//	    *tmp, min_curr, *tmp - min_curr );  // min_curr inside window and still valid, easy
	return min_curr;
}


/* Version operating on double timeseries */
double history_min_slide_value_dbl(history *hist, double min_curr, index_t j, index_t i)
{
	double *tmp;
	index_t tmp_i;

	tmp = history_find(hist, i+1);  // new value entering

	if ( i < j ) {
		verbose(LOG_ERR,"Error in min_slide_value_dbl, window width less than 1: %u %u %u", j,i,i-j+1);
		return *tmp;
	}

	/* Window only 1 wide anyway, easy */
	if (i == j)
		return *tmp;

	/* New one must be new min */
	if ( *tmp < min_curr )
		return *tmp;

	/* One being dropped was min, must do work */
	tmp = history_find(hist, j);  // value exiting
	if ( *tmp == min_curr ) {
		tmp_i = history_min_dbl(hist, j+1, i+1);  // i+1 may be min now old min gone
		tmp = history_find(hist, tmp_i);
		return *tmp;
	}

	/* min_curr inside window and still valid, easy */
	return min_curr;
}

