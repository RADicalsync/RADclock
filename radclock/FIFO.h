/*
 * Copyright (C) 2021 The RADclock Project (see AUTHORS file)
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

#ifndef _FIFO_H
#define _FIFO_H

/* Structure to hold a buffer of integer elements.
 * Will be used as a FIFO queue with drop-HEAD dropping policy.
 * Drop-HEAD means that an insertion will always succeed, overwritting the
 * oldest entry if the queue is full.
 * (get,put) = (pop,insert) indices are 64bit so it is safe to assume they will
 * never wrap, so can be used as absolute values and be compared safely.
 * Indexing into the bounded `array' is handling my modding with the capacity.
 * Allocation is dynamic so the capacity may be determined at run-time.
 * An empty buffer is signalled by putID = getID, whereas a full queue
 * corresponds to putID = getID + capacity = poised to overwrite.
 */
struct FIFO {
    uint64_t getID;   // index of head of FIFO queue
    uint64_t putID;   // index of next write location (putID >= getID)

    int *buffer;      // storage for buffer elements
    int capacity;     // maximum number of elements
};

int FIFO_init(struct FIFO **fifo, int buff_size);

int FIFO_get(struct FIFO *fifo, int *data);

int FIFO_put(struct FIFO *fifo, int data);

int FIFO_destroy(struct FIFO *fifo);

#endif
