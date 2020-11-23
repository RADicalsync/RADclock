#include "ring_buffer.h"


#define TELEMETRY_PRODUCER_LOG_VERBOSITY 1
#define TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL 1
#define TRIGGER_CA_ERR_THRESHOLD 1e-3 // 1 ms

int
active_trigger(struct radclock_handle *handle, long double *radclock_ts)
{

    // If clock status word has changed
    if (handle->telemetry_data.prior_status != handle->rad_data.status)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - status changed");
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
        verbose(LOG_NOTICE, "Telemetry Producer - Error bound exceeded");
        return 1;
    }

    // If clock has just passed a leap second
    if (handle->rad_data.leapsec_total != handle->telemetry_data.prior_leapsec_total)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - Change in leap seconds %d");
        return 1;
    }


    vcounter_t vcount;
    radclock_get_vcounter(handle->clock, &vcount);
    read_RADabs_UTC(&handle->rad_data, &vcount, radclock_ts, 0);
    long double a = 0.123456789;

    if (*radclock_ts - handle->telemetry_data.last_msg_time >= TELEMETRY_KEEP_ALIVE_SECONDS)
    {
        verbose(LOG_NOTICE, "Telemetry Producer - Synchronous keep alive trigger %d seconds since last message", (int)(*radclock_ts - handle->telemetry_data.last_msg_time));
        return 1;
    }

    return 0;
}

int
push_telemetry(struct radclock_handle *handle)
{
    // If telemetry is disabled return
    if (handle->conf->server_telemetry != BOOL_ON) {
        return 0;
    }

    // Check if we need to send a keep alive message
    long double radclock_ts = 0;
    int keep_alive_trigger = 0;

    if (active_trigger(handle, &radclock_ts))
    {
        // If the telemetry data changed then use the time of the last packet
        // Otherwise use the current time as its a keep alive message - set flag keep_alive_trigger
        if (radclock_ts == 0)
            read_RADabs_UTC(&handle->rad_data, &handle->rad_data.last_changed,  &radclock_ts, 0);
        else
            keep_alive_trigger = 1;
        
        push_telemetry_batch(handle->telemetry_data.packetId, &handle->telemetry_data.write_pos, handle->telemetry_data.buffer, &handle->telemetry_data.holding_buffer_size, handle->telemetry_data.holding_buffer, handle, radclock_ts, keep_alive_trigger);

        handle->telemetry_data.packetId += 1;
        handle->telemetry_data.last_msg_time = radclock_ts;
        handle->telemetry_data.prior_status = handle->rad_data.status;
        handle->telemetry_data.prior_PICN = 0;  // doesn't exist yet
        handle->telemetry_data.prior_uA = 0; // doesn't exist yet
        handle->telemetry_data.prior_leapsec_total = handle->rad_data.leapsec_total;
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

    if (packet_size > RADCLOCK_RING_BASE_BUFFER_BYTES)
    {
        verbose(LOG_ERR, "Telemetry Producer Error packet too large %d vs max_size:%d", packet_size, RADCLOCK_RING_BASE_BUFFER_BYTES);
        return RADCLOCK_ERROR_DATA_TOO_LARGE;
    }

    int start_write_buffer_pos = *ring_write_pos;

    // Create a telemetry OCN specific packet
    Radclock_Telemetry_Latest * header_data = (Radclock_Telemetry_Latest *) holding_buffer;

    if (keep_alive_trigger)
    {
        // If it is a keep alive trigger assume all the data is the same. But change the header data
        header_data->header.packetId = packetId;
        header_data->timestamp = timestamp;
    }
    else
    {
        *header_data = make_telemetry_packet(packetId, PICN, asym, ICN_Count, timestamp);

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

        if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 1)
            verbose(LOG_NOTICE, "Telemetry Producer Footer: wp:%d footer_size:%d", *ring_write_pos, footer_data->size);        
    }

    // Push all data into the ring buffer in a single batch write
    bytes_written += ring_buffer_write(holding_buffer, packet_size, shared_memory_handle, ring_write_pos);

    // Update the ring head positions
    flush_heads(shared_memory_handle, start_write_buffer_pos, *ring_write_pos);

    if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 0 && header_data->header.packetId % TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL == 0) // && header_data.header.packetId % 1000 == 0
        verbose(LOG_NOTICE, "Telemetry Producer: start ts:%.09Lf wp:%d current wp:%d: v:%d ICNS:%d size:%d id:%d", timestamp, start_write_buffer_pos, *ring_write_pos, header_data->header.version, header_data->ICN_Count, header_data->header.size, header_data->header.packetId);

    if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 2 )
        for (int i = 0; i < ICN_Count; i++)
        {
            int offset = sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_ICN_Latest)*i;
            Radclock_Telemetry_ICN_Latest * ICN_data = (Radclock_Telemetry_ICN_Latest *) (holding_buffer + offset);
            verbose(LOG_NOTICE, "Telemetry Producer ICN: wp:%d: stat:%u id:%d uA:%le err:%le (%d, %d)", *ring_write_pos, ICN_data->status_word, ICN_data->ICN_id, ICN_data->uA, ICN_data->err_bound, offset, *holding_buffer_size);
        }

    if (bytes_written != header_data->header.size)
        verbose(LOG_ERR, "Telemetry Producer Error packet bytes: %d different from packet.header.size: %d", bytes_written, header_data->header.size);

    return bytes_written;
}