// Shared memory ring buffer
// Assumes single producer - single/ multi consumer - No locks - No waiting
// The algorithm favours speed for the producer with the cost of some missed packets on the behalf of consumers
// should they be slower than the producer and/or the ring buffer size allows only few packets
// Consumers don't write anything to the buffer - Therefore, no impact on any number of consumers
// Jason Traish 29/10/2020 - jason.traish@uts.edu.au

// Packets are formed on the ring buffer of a header, content, footer
// The header and footer structures should be immutable and are required for the ring buffer to work
// Changes to the content structures can take place as long as the max packet length <= allocated ring buffer size
// This ring buffer supports variable length content packets
// The header contains the size of the total packet, version and packetId for verification upon receipt
// The footer only contains the size of the packet. It is used for consumers to read prior packets they didn't see
// because they started after the producer created several packets or because the consumer didn't process packets 
// while the producer was creating new ones

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#include "radclock_telemetry.h"

// Typical expected size of a packet assuming 64 NTC servers
#define RADCLOCK_TYPICAL_TELEMETRY_PACKET_SIZE (sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_NTC_Latest)*64 + sizeof(Radclock_Telemetry_Footer))

// Max number of typical packets that can fit within the buffer
#define RADCLOCK_RING_BUFFER_PACKETS 2064

// Size of ring buffer in bytes - includes three integers to track write, read and filled positions within the ring
#define RADCLOCK_RING_BASE_BUFFER_BYTES (int)(RADCLOCK_TYPICAL_TELEMETRY_PACKET_SIZE * RADCLOCK_RING_BUFFER_PACKETS)
#define RADCLOCK_RING_BUFFER_BYTES RADCLOCK_RING_BASE_BUFFER_BYTES+sizeof(int)*3

#define PROJECT_FILE "testtk"
#define PROJECT_ID 190

#define RADCLOCK_RING_BUFFER_WRITE_HEAD RADCLOCK_RING_BASE_BUFFER_BYTES
#define RADCLOCK_RING_BUFFER_READ_HEAD  (RADCLOCK_RING_BASE_BUFFER_BYTES + sizeof(int)*1)
#define RADCLOCK_RING_BUFFER_FILL_HEAD  (RADCLOCK_RING_BASE_BUFFER_BYTES + sizeof(int)*2)

// Wrap integer values for write, fill and read heads when they exceed the following values
#define RADCLOCK_HEAD_INT_THRESHOLD RADCLOCK_RING_BASE_BUFFER_BYTES*20 // (32768) // 2^15
#define RADCLOCK_HEAD_INT_THRESHOLD_DIF (RADCLOCK_HEAD_INT_THRESHOLD - RADCLOCK_RING_BASE_BUFFER_BYTES * 2)

// ERROR CODES - CLIENT READ CODES
#define RADCLOCK_ERROR_NO_NEW_DATA -1
#define RADCLOCK_ERROR_MID_PACKET_SEGMENT_OVERRIDE -2
#define RADCLOCK_ERROR_DATA_TOO_SMALL -3
#define RADCLOCK_ERROR_DATA_TOO_LARGE -4
#define RADCLOCK_ERROR_OVERIDDEN_DURING_COPY -5

struct Ring_Buffer_Prior_Data
{
    unsigned int prior_status;
    double prior_uA;
    int prior_leapsec_total;
    double prior_minRTT;
    double prior_clockErr;

    unsigned int prior_servertrust;
    unsigned int prior_sa;

	// long double last_msg_time;
};

struct Ring_Buffer_Producer_Data
{
    int buffer_size;
    void* buffer;
    int is_ipc_share;

    int write_pos;
    int packetId;

    int holding_buffer_size;
    void* holding_buffer;

    int prior_PICN;

    struct Ring_Buffer_Prior_Data * prior_data;
    int prior_data_size;
    // unsigned int prior_status;
    // double prior_uA;
    // int prior_leapsec_total;
	long double last_msg_time;
};

typedef struct Ring_Buffer_Producer_Data Ring_Buffer_Producer_Data;

char * radclock_errorstr(int errorCode);

void * open_shared_memory();

// Memory pool is still alive but this process disconnects from it
void detach_shared_memory(void * shared_memory_handle);

void destroy_shared_memory();

// Set shared memory read, write and fill heads to 0
void init_shared_memory(void * ring_buffer);

int get_shared_memory_read_header(void * ring_buffer);

void flush_heads(void * ring_buffer, int start_ring_fill_pos, int ring_write_pos);

int ring_buffer_write(void * data, int bytes, void * ring_buffer, int *ring_write_pos);


int ring_buffer_read(void * data, int bytes, void * ring_buffer, int *ring_read_pos, int first_packet_segment);

int get_prior_packet_available(void * ring_buffer, int ring_read_pos, int * prior_packet_read_pos, int * new_packet_size);

// Grows holding buffer to accomadate data as required
void check_and_grow_holding_buffer(int *current_size, int required_size, void * holding_buffer);

#endif //RINGBUFFER_H