#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "radclock_telemetry.h"
#include "ring_buffer.h"

char * radclock_errorstr(int errorCode)
{
	if (RADCLOCK_ERROR_NO_NEW_DATA == errorCode)
		return "No new data to read";
	if (RADCLOCK_ERROR_MID_PACKET_SEGMENT_OVERRIDE == errorCode)
		return "Reading data from mid packet was overridden by producer.";
	if (RADCLOCK_ERROR_DATA_TOO_LARGE == errorCode)
		return "Data is too large to fit within ring buffer";
	if (RADCLOCK_ERROR_DATA_TOO_SMALL == errorCode)
		return "Data is smaller than 1 byte";
	if (RADCLOCK_ERROR_OVERIDDEN_DURING_COPY == errorCode)
		return "Data was overridden during the copy";
	return "Unknown error code";
}

void * open_shared_memory()
{
	if (RADCLOCK_HEAD_INT_THRESHOLD < RADCLOCK_RING_BASE_BUFFER_BYTES * 10) {
		printf("Error RADCLOCK_HEAD_INT_THRESHOLD too small %d %d\n",
				RADCLOCK_HEAD_INT_THRESHOLD, RADCLOCK_RING_BASE_BUFFER_BYTES * 10);
	}
	printf("Memory size:%dkB Thresh:%d Dif:%d Typical_Packet:%ld Buffer_Packets:%d\n",
	    RADCLOCK_RING_BASE_BUFFER_BYTES / 1024, RADCLOCK_HEAD_INT_THRESHOLD,
	    RADCLOCK_HEAD_INT_THRESHOLD_DIF, RADCLOCK_TYPICAL_TELEMETRY_PACKET_SIZE,
	    RADCLOCK_RING_BUFFER_PACKETS);
	// ftok to generate unique key
	//key_t key = ftok(PROJECT_FILE, PROJECT_ID);
	//printf("key %d\n", key);

	// Hardcoded key based on generated key from above
	key_t key = -1107143452;
	if (key == -1) {
		printf("Error opening shared memory pool. (ftok) Error code:%d - %s \n", errno, strerror(errno));
		return 0;
	}

	// shmget returns an identifier in shmid
	int shmid = shmget(key,RADCLOCK_RING_BUFFER_BYTES,0666|IPC_CREAT);

	if (shmid == -1) {
		printf("Error opening shared memory pool. Error code:%d - %s \n", errno, strerror(errno));
		return 0;
	}
	// shmat to attach to shared memory
	void *shared_memory_handle = shmat(shmid,(void*)0,0);

	if (shared_memory_handle == (void*)-1) {
		printf("Error opening shared memory pool. Error code:%d - %s \n", errno, strerror(errno));
		return 0;
	}
	return shared_memory_handle;
}

// Memory pool is still alive but this process disconnects from it
void detach_shared_memory(void * shared_memory_handle)
{
	shmdt(shared_memory_handle);
}

void destroy_shared_memory()
{
	// ftok to generate unique key
	key_t key = ftok(PROJECT_FILE, PROJECT_ID);

	// shmget returns an identifier in shmid
	int shmid = shmget(key,RADCLOCK_RING_BUFFER_BYTES,0666|IPC_CREAT);

	// Destroy the shared memory pool
	shmctl(shmid,IPC_RMID,NULL);
}

// Set shared memory read, write and fill heads to 0
void init_shared_memory(void * ring_buffer)
{
	memset(ring_buffer + RADCLOCK_RING_BUFFER_READ_HEAD,    0, sizeof(int));
	memset(ring_buffer + RADCLOCK_RING_BUFFER_WRITE_HEAD,   0, sizeof(int));
	memset(ring_buffer + RADCLOCK_RING_BUFFER_FILL_HEAD,    0, sizeof(int));
}

int get_shared_memory_read_header(void * ring_buffer)
{
	return * (int*) (ring_buffer + RADCLOCK_RING_BUFFER_READ_HEAD);
}

void flush_heads(void * ring_buffer, int start_ring_fill_pos, int ring_write_pos)
{
	int * read_head = ring_buffer + RADCLOCK_RING_BUFFER_READ_HEAD;
	// Set the read head to the data just pushed into the buffer
	*read_head = start_ring_fill_pos;

	int * write_head = ring_buffer + RADCLOCK_RING_BUFFER_WRITE_HEAD;
	*write_head = ring_write_pos;
}

int ring_buffer_write(void * data, int bytes, void * ring_buffer, int *ring_write_pos)
{
	// Sanity check that we can't write more than the buffer size
	if (bytes > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		printf("Error attempting to write data larger than the buffer. Data %d bytes cannot fit in %d buffer\n",
		    bytes, RADCLOCK_RING_BASE_BUFFER_BYTES);
		return RADCLOCK_ERROR_DATA_TOO_LARGE;
	}
	if (bytes < 1) {
		printf("Error attempting to write less than 1 byte of data. Data %d bytes\n", bytes);
		return RADCLOCK_ERROR_DATA_TOO_SMALL;
	}

	int wrapped_ring_write_pos = *ring_write_pos % RADCLOCK_RING_BASE_BUFFER_BYTES;

	// Check if packet must be split to wrap around buffer or can be done in one go
	if (wrapped_ring_write_pos+bytes > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		// Split the data into two calls. One to fit into remaining bytes of the ring buffer.
		// Second segment will fill from the start of the buffer
		int first_segment_size = RADCLOCK_RING_BASE_BUFFER_BYTES - wrapped_ring_write_pos;
		int written_bytes = ring_buffer_write(data, first_segment_size, ring_buffer, ring_write_pos);

		int second_segment_size = bytes - first_segment_size;
		written_bytes += ring_buffer_write(data + first_segment_size, second_segment_size, ring_buffer, ring_write_pos);

		return written_bytes;
	} else {
		// Keeps track of how full of data the ring buffer is - once a full cycle takes place it will show its full of data
		int * buffer_fill = (int*) (ring_buffer + RADCLOCK_RING_BUFFER_FILL_HEAD);
		*buffer_fill += bytes;

		// Avoid integer overflow issues - Detect and control
		if (*buffer_fill > RADCLOCK_HEAD_INT_THRESHOLD)
				*buffer_fill -= RADCLOCK_HEAD_INT_THRESHOLD_DIF;

		// Write data to the buffer
		memcpy(ring_buffer + wrapped_ring_write_pos, data, bytes);

		// Move internal write position forward
		*ring_write_pos += bytes;

		// Avoid integer overflow issues - Detect and control
		if (*ring_write_pos > RADCLOCK_HEAD_INT_THRESHOLD)
				*ring_write_pos -= RADCLOCK_HEAD_INT_THRESHOLD_DIF;

		return bytes;
	}
}


int ring_buffer_read(void * data, int bytes, void * ring_buffer, int *ring_read_pos, int first_packet_segment)
{
	// Sanity check that we can't read more than the buffer size
	if (bytes > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		printf("Error attempting to read data larger than the buffer. Data %d bytes cannot fit in %d buffer\n",
		    bytes, RADCLOCK_RING_BASE_BUFFER_BYTES);
		return RADCLOCK_ERROR_DATA_TOO_LARGE;
	}

	// Sanity check that we can't read less than 1 byte
	if (bytes < 1) {
		printf("Error attempting to read less than 1 byte of data. Data %d bytes\n", bytes);
		return RADCLOCK_ERROR_DATA_TOO_SMALL;
	}

	int * buffer_fill = (int*) (ring_buffer + RADCLOCK_RING_BUFFER_FILL_HEAD);

	// Check if the heads have wrapped around the threshold value. This will cause the read head to be in front of the fill head
	// We can therefore wrap the read head as well
	if (*ring_read_pos > *buffer_fill) {
		// printf("Detected INT threshold moving value from bs:%d fill:%d dif:%d rp:%d -> rp:%d\n",
		// RADCLOCK_RING_BASE_BUFFER_BYTES, *buffer_fill, RADCLOCK_HEAD_INT_THRESHOLD_DIF, *ring_read_pos,
		// *ring_read_pos-RADCLOCK_HEAD_INT_THRESHOLD_DIF);
		*ring_read_pos -= RADCLOCK_HEAD_INT_THRESHOLD_DIF;
	}

	// Since we last tried to read from the buffer the entire buffer was filled. So we have to reset our read position
	if (*buffer_fill - *ring_read_pos >= RADCLOCK_RING_BASE_BUFFER_BYTES) {
		*ring_read_pos = get_shared_memory_read_header(ring_buffer);

		// Check if the heads have wrapped around the threshold value. This will cause the read head to be in front of the fill head
		// We can therefore wrap the read head as well
		if (*ring_read_pos > *buffer_fill)
			*ring_read_pos -= RADCLOCK_HEAD_INT_THRESHOLD_DIF;

		// If we are reading the first packet segment then we can allow the read header to jump as this won't corrupt data
		// Otherwise we'll need to set a read error indicating that the current data was overridden and try again
		if (! first_packet_segment)
			return RADCLOCK_ERROR_MID_PACKET_SEGMENT_OVERRIDE;
	}

	// There is no new data to read
	int * global_ring_read_pos = (int*) (ring_buffer + RADCLOCK_RING_BUFFER_WRITE_HEAD);
	if (*global_ring_read_pos - *ring_read_pos == 0)
		return RADCLOCK_ERROR_NO_NEW_DATA;

	int wrapped_ring_read_pos = *ring_read_pos % RADCLOCK_RING_BASE_BUFFER_BYTES;
	if (wrapped_ring_read_pos+bytes > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		// Split the data into two calls. One to fit into remaining bytes of the ring buffer.
		// Second segment will fill from the start of the buffer
		int first_segment_size = RADCLOCK_RING_BASE_BUFFER_BYTES - wrapped_ring_read_pos;
		int read_bytes = ring_buffer_read(data, first_segment_size, ring_buffer, ring_read_pos, first_packet_segment);
		if (read_bytes < 1)
			return read_bytes;

		int second_segment_size = bytes - first_segment_size;
		int read_bytes2 = ring_buffer_read(data + first_segment_size, second_segment_size, ring_buffer, ring_read_pos, 0);
		if (read_bytes2 < 1)
			return read_bytes2;

		return read_bytes + read_bytes2;
	} else {
		memcpy(data, ring_buffer+wrapped_ring_read_pos, bytes);

		int end_buffer = *buffer_fill;
		if (end_buffer - *ring_read_pos > RADCLOCK_RING_BASE_BUFFER_BYTES)
			return RADCLOCK_ERROR_OVERIDDEN_DURING_COPY;

		*ring_read_pos += bytes;

		return bytes;
	}
}


int get_prior_packet_available(void * ring_buffer, int ring_read_pos, int * prior_packet_read_pos, int * new_packet_size)
{
	// This is the origin of the buffer. No packets can exist before this
	if (ring_read_pos == 0)
		return 0;

	// Get the footer of the previous packet.
	int wrapped_prior_footer_pos = (ring_read_pos - sizeof(Radclock_Telemetry_Footer)) % RADCLOCK_RING_BASE_BUFFER_BYTES;
	Radclock_Telemetry_Footer prior_packet_footer;

	if (wrapped_prior_footer_pos + sizeof(Radclock_Telemetry_Footer) > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		// wrapped read
		int first_segment_size = RADCLOCK_RING_BASE_BUFFER_BYTES - wrapped_prior_footer_pos;
		memcpy(&prior_packet_footer, ring_buffer, first_segment_size);

		int second_segment_size = sizeof(Radclock_Telemetry_Footer) - first_segment_size;
		memcpy( ((void*)&prior_packet_footer) + first_segment_size, ring_buffer, first_segment_size);
		printf("Performed wrapped size read %d %d %d\n", first_segment_size, second_segment_size, prior_packet_footer.size);
	} else
		prior_packet_footer = * (Radclock_Telemetry_Footer*) (ring_buffer + wrapped_prior_footer_pos);



	int buffer_fill = *(int*) (ring_buffer + RADCLOCK_RING_BUFFER_FILL_HEAD);

	// Check if the heads have wrapped around the threshold value. This will cause the read head to be in front of the fill head
	// We can therefore wrap the read head as well
	if (ring_read_pos > buffer_fill)
		ring_read_pos -= RADCLOCK_HEAD_INT_THRESHOLD_DIF;

	// Check that the recently retrieved footer wasn't potientially overriden
	if (buffer_fill > (ring_read_pos - sizeof(Radclock_Telemetry_Footer)) + RADCLOCK_RING_BASE_BUFFER_BYTES)
		return 0;

	// Check if the buffer has not overriden any data in the previous packet
	if (buffer_fill < (ring_read_pos - prior_packet_footer.size) + RADCLOCK_RING_BASE_BUFFER_BYTES) {
		*prior_packet_read_pos = ring_read_pos - prior_packet_footer.size;
		*new_packet_size = prior_packet_footer.size;
		return 1;
	}

	return 0;
}


// Grows holding buffer to accommodate data as required
void check_and_grow_holding_buffer(int *current_size, int required_size, void * holding_buffer)
{
	if (required_size > *current_size) {
		// printf("Increasing hold buffer size from %d -> %d\n", *holding_buffer_size, packet_size);
		free(holding_buffer);
		holding_buffer = malloc(required_size);
		*current_size = required_size;
	}
}
