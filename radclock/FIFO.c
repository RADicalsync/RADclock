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

#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>

#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "FIFO.h"

/* Initialize the queue, its buffer, and return its address */
int FIFO_init(struct FIFO **fifo, int buff_size)
{
	struct FIFO *new_fifo;

	new_fifo = malloc(sizeof(struct FIFO));
	if (!new_fifo)
	  return -1;

	new_fifo->getID = 0;
	new_fifo->putID = 0;
	new_fifo->capacity = buff_size;

	new_fifo->buffer = malloc(sizeof(int)*buff_size);
	if (!new_fifo->buffer) {
	  free(new_fifo);
	  new_fifo = NULL;
	  return -1;
	}

	*fifo = new_fifo;
	return 0;
}

/* Pop the oldest entry in the queue. If the queue is empty, return -1. */
int FIFO_get(struct FIFO *fifo, int *data)
{
	if (fifo->putID == fifo->getID)
	  return -1;

	/* Return the oldest entry and advance, no need to clear it */
	*data = fifo->buffer[fifo->getID % fifo->capacity];
	fifo->getID++;

	// verbose(VERB_DEBUG, "FIFO GET: putID = %d, getID = %d, size = %d, "
	//                     "capacity = %d, data = %d", fifo->putID % fifo->capacity,
	//                     fifo->getID % fifo->capacity, fifo->putID - fifo->getID,
	//							fifo->capacity, *data);
	verbose(VERB_DEBUG, "After FIFO_get: %d remaining", fifo->putID - fifo->getID);

	return 0;
}

/* Insert into the FIFO buffer with a drop-HEAD policy, ie, if the buffer is
 * full, insert the new by overwritting the oldest. */
int FIFO_put(struct FIFO *fifo, int data)
{
	int ret = 0;

	/* If buffer full, oldest value will be overwritten, advance to next */
	if ((fifo->putID - fifo->getID) >= fifo->capacity) {	// if full
		fifo->getID++;
		ret = -1;
	}

	/* Always insert, even if full */
	fifo->buffer[fifo->putID % fifo->capacity] = data;
	fifo->putID++;

	// verbose(VERB_DEBUG, "FIFO PUT: putID = %d, getID = %d, size = %d, "
	//                     "capacity = %d, data = %d", fifo->putID % fifo->capacity,
	//                     fifo->getID % fifo->capacity, fifo->putID - fifo->getID,
	//							fifo->capacity, data);
	//verbose(VERB_DEBUG, "FIFO PUT: size = %d", fifo->putID - fifo->getID);
	return (ret);
}

/* Deallocates memory for the buffer and the fifo instance */
int FIFO_destroy(struct FIFO *fifo)
{
	if (!fifo)
		return -1;
    
	free(fifo->buffer);
	fifo->buffer = NULL;

	free(fifo);

	return 0;
}
