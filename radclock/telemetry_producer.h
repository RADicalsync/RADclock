#include "ring_buffer.h"


#define TELEMETRY_PRODUCER_LOG_VERBOSITY 1
#define TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL 1
#define TRIGGER_CA_ERR_THRESHOLD 1e-3 // 1 ms

int
active_trigger(struct radclock_handle *handle, struct timespec *current_time)
{

    if (current_time->tv_sec - handle->telemetry_data.last_msg_time.tv_sec >= TELEMETRY_KEEP_ALIVE_SECONDS)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - Synchronous keep alive trigger %d seconds since last message", current_time->tv_sec - handle->telemetry_data.last_msg_time.tv_sec);
        return 1;
    }

    // If clock status word has changed
    if (handle->telemetry_data.prior_status != handle->rad_data.status)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - status changed %d", current_time->tv_sec - handle->telemetry_data.last_msg_time.tv_sec);
        return 1;
    }

    // If PICN has changed - currently doesn't exist
    // if (handle->telemetry_data.prior_PICN != handle->rad_data.PICN)
    //     return true;

    // If clock underlying asymmetry has changed - currently doesn't exist
    // if (handle->telemetry_data.prior_uA != handle->rad_data.uA)
        // return true;

    // If clock underlying asymmetry has changed - currently doesn't exist
    if (handle->rad_data.ca_err > TRIGGER_CA_ERR_THRESHOLD)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - Error bound exceeded %d", current_time->tv_sec - handle->telemetry_data.last_msg_time.tv_sec);
        return 1;
    }

    // If clock has just passed a leap second
    if (handle->rad_data.leapsec_total != handle->telemetry_data.prior_leapsec_total)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - Change in leap seconds %d", current_time->tv_sec - handle->telemetry_data.last_msg_time.tv_sec);
        return 1;
    }

    return 0;
}

void
set_last_telemetry_signal_time(struct radclock_handle *handle, struct timespec current_time)
{
    handle->telemetry_data.last_msg_time.tv_sec = current_time.tv_sec;
    handle->telemetry_data.last_msg_time.tv_nsec = current_time.tv_nsec;  
}

int
push_telemetry(struct radclock_handle *handle, int keep_alive_trigger, int *holding_buffer_size, void* holding_buffer, long double * ref_timestamp)
{
    // Check if we need to send a keep alive message
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);

    if (keep_alive_trigger || active_trigger(handle, &current_time))
    {
        // If this push was triggered by a timer then we need a new vcount. Otherwise use last set value
        long double timestamp;
        if (keep_alive_trigger)  
        {
            if (*ref_timestamp == 0)
            {
                vcounter_t vcount;
                radclock_get_vcounter(handle->clock, &vcount);
                read_RADabs_UTC(&handle->rad_data, &vcount,  &timestamp, 0);
                *ref_timestamp = timestamp;
            }
            else 
                timestamp = *ref_timestamp;
        }
        else
            read_RADabs_UTC(&handle->rad_data, &handle->rad_data.last_changed,  &timestamp, 0);

        // If this function is called not as a result of the keep_alive_trigger timer use producer holding buffer
        if (! keep_alive_trigger)
        {
            holding_buffer_size = &handle->telemetry_data.holding_buffer_size;
            holding_buffer = handle->telemetry_data.holding_buffer;
        }

        push_telemetry_batch(handle->telemetry_data.packetId, &handle->telemetry_data.write_pos, handle->telemetry_data.buffer, holding_buffer_size, holding_buffer, handle, timestamp, keep_alive_trigger);

        // Set the trigger values to detect changes
        if (! keep_alive_trigger )
        {
            handle->telemetry_data.packetId += 1;
            set_last_telemetry_signal_time(handle, current_time);
            handle->telemetry_data.prior_status = handle->rad_data.status;
            handle->telemetry_data.prior_PICN = 0;  // doesn't exist yet
            handle->telemetry_data.prior_uA = 0; // doesn't exist yet
            handle->telemetry_data.prior_leapsec_total = handle->rad_data.leapsec_total;
        }
    }

    return (0);
}

int 
push_telemetry_batch(int packetId, int *ring_write_pos, void * shared_memory_handle, int *holding_buffer_size, void* holding_buffer, struct radclock_handle *handle, long double timestamp, int keep_alive_trigger)
{
    int bytes_written = 0;

    // Setup dummy clock stats
    double asym = 234;
    int ICN_Count = 30;
    int PICN = 25; 

    // If the packet is larger than the holding buffer then make a new larger holding buffer
    int packet_size = sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_ICN_Latest) * ICN_Count  + sizeof(Radclock_Telemetry_Footer);
    check_and_grow_holding_buffer(holding_buffer_size, packet_size, holding_buffer);

    // Create a telemetry OCN specific packet
    Radclock_Telemetry_Latest * header_data = (Radclock_Telemetry_Latest *) holding_buffer;
    *header_data = make_telemetry_packet(packetId, PICN, asym, ICN_Count, timestamp);

    if (packet_size > RADCLOCK_RING_BASE_BUFFER_BYTES)
    {
        verbose(LOG_ERR, "Telemetry Producer Error packet too large %d vs max_size:%d", packet_size, RADCLOCK_RING_BASE_BUFFER_BYTES);
        return RADCLOCK_ERROR_DATA_TOO_LARGE;
    }

    int start_write_buffer_pos = *ring_write_pos;

    // Write OCN specific telemetry packet to shared memory
    // bytes_written += ring_buffer_write(&header_data, sizeof(Radclock_Telemetry_Latest), shared_memory_handle, ring_write_pos);
    for (int i = 0; i < ICN_Count; i++)
    {
        // Dummy ICN data
        unsigned int status_word = handle->rad_data.status;
        int ICN_id = i+1;
        double uA = 0; 
        double err_bound = handle->rad_data.ca_err;

        // Create a telemetry ICN specific packet
        Radclock_Telemetry_ICN_Latest * ICN_data = (Radclock_Telemetry_ICN_Latest *) (holding_buffer + sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_ICN_Latest)*i);
        *ICN_data = make_telemetry_ICN_packet(status_word, ICN_id, uA, err_bound);
    }

    // Create and copy footer data to holding buffer
    Radclock_Telemetry_Footer * footer_data = (Radclock_Telemetry_Footer *)  (holding_buffer + sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_ICN_Latest)*ICN_Count);
    *footer_data = make_telemetry_footer(packet_size);

    // Push all data into the ring buffer in a single batch write
    // If keep_alive_trigger is used data doesn't need to go into ring_buffer. It will go directly to the consumer
    if (!keep_alive_trigger)
    {
        bytes_written += ring_buffer_write(holding_buffer, packet_size, shared_memory_handle, ring_write_pos);

        // Update the ring head positions
        flush_heads(shared_memory_handle, start_write_buffer_pos, *ring_write_pos);
    } 
    else
        bytes_written += packet_size;

    if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 0 && !keep_alive_trigger && header_data->header.packetId % TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL == 0) // && header_data.header.packetId % 1000 == 0
        verbose(LOG_NOTICE, "Telemetry Producer: start ts:%Lf wp:%d current wp:%d: v:%d ICNS:%d size:%d id:%d", timestamp, start_write_buffer_pos, *ring_write_pos, header_data->header.version, header_data->ICN_Count, header_data->header.size, header_data->header.packetId);

    if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 2 && !keep_alive_trigger )
        for (int i = 0; i < ICN_Count; i++)
        {
            int offset = sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_ICN_Latest)*i;
            Radclock_Telemetry_ICN_Latest * ICN_data = (Radclock_Telemetry_ICN_Latest *) (holding_buffer + offset);
            verbose(LOG_NOTICE, "Telemetry Producer ICN: wp:%d: stat:%u id:%d uA:%le err:%le (%d, %d)", *ring_write_pos, ICN_data->status_word, ICN_data->ICN_id, ICN_data->uA, ICN_data->err_bound, offset, *holding_buffer_size);
        }

    if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 1)
        verbose(LOG_NOTICE, "Telemetry Producer Footer: wp:%d footer_size:%d", *ring_write_pos, footer_data->size);


    if (bytes_written != header_data->header.size)
        verbose(LOG_ERR, "Telemetry Producer Error packet bytes: %d different from packet.header.size: %d", bytes_written, header_data->header.size);

    return bytes_written;
}