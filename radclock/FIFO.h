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

int FIFO_getSize(const struct FIFO *fifo);

int FIFO_destroy(struct FIFO *fifo);

#endif
