#include <msgpuck.h>
#include <limits.h>
#include <time.h>
#include <errno.h>  

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
// todo move telemetry defines and functions to their own file
#define SAMPLE_FREQUENCY_MS 10 // Sample ring buffer every 10ms

// For compatability with freebsd that doesn't define HOST_NAME_MAX (256 should be big enough)
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif // HOST_NAME_MAX

#ifdef CLOCK_MONOTONIC_RAW
    #define SAMPLE_CLOCK CLOCK_MONOTONIC_RAW // Linux
#else// CLOCK_MONOTONIC_RAW
    #define SAMPLE_CLOCK CLOCK_MONOTONIC_FAST // FreeBSD
#endif // CLOCK_MONOTONIC_RAW
#define EXIT_ON_TERMINAL_ERROR 0
#define telemetry_consumer_callback_method packet_callback_msgpuck
#define TELEMETRY_CONSUMER_LOG_VERBOSITY 0
#define TELEMETRY_CONSUMER_LOG_PACKET_INTERVAL 1

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


void packet_callback_binary(void * data, int packetId, int dataSize, long double timestamp)
{
    // Add hostname to filename - because it is not included anywhere within the packet (packet was assumed to circulate
    // only internally to a system). For the purposes of future analysis including the host name will make future
    // processing easier
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX + 1);

	// Make sure the directory is set to permission read, write and exe for all users. 
	// As different plugins from other users need to read and move cache files around

    // Open file
    char char_buffer[128];
    sprintf(char_buffer, "%s/bin_%s_%d_%.09Lf", TELEMETRY_CACHE_OLD_DIR, hostname, packetId, timestamp);
    mode_t old_mask = umask(0);
    int fOut = open (char_buffer, O_WRONLY | O_CREAT, 0777 );
    if (fOut == -1)
    // FILE * fp = fopen(char_buffer, "w");
    // if (fp == 0)
    {
        umask(old_mask);
        verbose(LOG_ERR,"Telemetry Consumer - Error creating cache file %s", char_buffer);
        return;
    }
    // fwrite(data , 1 , dataSize , fp);
    // fclose(fp);
    write(fOut, data, dataSize);
    close(fOut);
    umask(old_mask);
}

void packet_callback_msgpuck(void * data, int packetId, int dataSize, long double timestamp)
{
    // Temporary - Dump cache files in both binary and msgpuck form
    packet_callback_binary(data, packetId, dataSize, timestamp);
    
    // Access data and store to file
    Radclock_Telemetry_Latest * ocn_data = (Radclock_Telemetry_Latest *) data;
    Radclock_Telemetry_NTC_Latest * NTC_data;

    // Split the timestamp into two parts - seconds and nano seconds as msgpuck doesn't support long double
    unsigned int ts_sec = (unsigned int) timestamp;
    unsigned int ts_nsec = (unsigned int) (1000000000*(timestamp - ts_sec) + 0.5);

    // ToDO dynamically size buffers incase of message size ramp up
    // Open file
    char buf[1024*4];

    // Cached file is named after the second and packet id. 
    // As the packet id can reset and the time can shift and multiple packets can occur each second
    // This should ensure no two file names have the same name
    sprintf(buf, "%s/msgpuck_%d_%.09Lf", TELEMETRY_CACHE_DIR, packetId, ocn_data->timestamp);
    FILE * fp;
    
    // This checks if the file already exists. It shouldn't 
    // If it does then something very strange has occured
    fp = fopen(buf, "r");
    while (fp != 0)
    {
        fclose(fp);
        verbose(LOG_ERR,"Telemetry Consumer - Error cache telemetry file already exists");
        return;
    }

	// Make sure the directory is set to permission read, write and exe for all users. 
	// As different plugins from other users need to read and move cache files around
    mode_t old_mask = umask(0);
    int fOut = open (buf, O_WRONLY | O_CREAT, 0777 );
    if (fOut == -1)
    {
        umask(old_mask);
        verbose(LOG_ERR,"Telemetry Consumer - Error creating cache file %s", buf);
        return;
    }
    // printf ("Processing:%s\n", buf);

    // Push data to file using msgpuck
    char *w = buf;
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX + 1);

    // Build msgpack
    w = mp_encode_map(w, 4);

    w = mp_encode_str(w, "name", 4);
    w = mp_encode_str(w, "clock_stats", strlen("clock_stats"));

    w = mp_encode_str(w, "ocn", 3);
    w = mp_encode_str(w, hostname, strlen(hostname));

    w = mp_encode_str(w, "v", 1);
    w = mp_encode_uint(w, ocn_data->header.version);    

    w = mp_encode_str(w, "ocn_clock", strlen("ocn_clock"));
    w = mp_encode_map(w, 9);
    // A map of 9 elements

    w = mp_encode_str(w, "picn", 4);
    w = mp_encode_uint(w, ocn_data->PICN);

    w = mp_encode_str(w, "ntc_count", strlen("ntc_count"));
    w = mp_encode_uint(w, ocn_data->NTC_Count);

    // w = mp_encode_str(w, "ts_sec", strlen("ts_sec"));
    // w = mp_encode_uint(w, ts_sec);

    // w = mp_encode_str(w, "ts_nsec", strlen("ts_nsec"));
    // w = mp_encode_uint(w, ts_nsec);

    w = mp_encode_str(w, "ts", strlen("ts"));
    w = mp_encode_double(w, (double)timestamp);

    w = mp_encode_str(w, "ntp_acc", strlen("ntp_acc"));
    w = mp_encode_uint(w, ocn_data->delta_accepted_public_ntp);

    w = mp_encode_str(w, "ntp_rej", strlen("ntp_rej"));
    w = mp_encode_uint(w, ocn_data->delta_rejected_public_ntp);

    w = mp_encode_str(w, "servertrust", strlen("servertrust"));
    w = mp_encode_uint(w, ocn_data->servertrust);

    w = mp_encode_str(w, "pub_ntp_s", strlen("pub_ntp_s"));
    w = mp_encode_uint(w, ocn_data->public_ntp_state);

    w = mp_encode_str(w, "sa", strlen("sa"));
    w = mp_encode_uint(w, ocn_data->sa);

    // A map of NTC server elements
    w = mp_encode_str(w, "NTC", 3);
    w = mp_encode_map(w, ocn_data->NTC_Count);
    for (int i = 0; i < ocn_data->NTC_Count; i++)
    {
        NTC_data =  (Radclock_Telemetry_NTC_Latest *)(data + sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_NTC_Latest)*i);
        char ntc_name[128];
        sprintf(ntc_name, "%d", NTC_data->NTC_id);
        
        w = mp_encode_str(w, ntc_name, strlen(ntc_name));
        w = mp_encode_map(w, 5);

        w = mp_encode_str(w, "stat", 4);
        w = mp_encode_uint(w, NTC_data->status_word);

        w = mp_encode_str(w, "ua", 2);
        w = mp_encode_double(w, NTC_data->uA);

        w = mp_encode_str(w, "err_bound", strlen("err_bound"));
        w = mp_encode_double(w, NTC_data->err_bound);

        w = mp_encode_str(w, "min_RTT", strlen("min_RTT"));
        w = mp_encode_double(w, NTC_data->min_RTT);        

        w = mp_encode_str(w, "clockErr", strlen("clockErr"));
        w = mp_encode_double(w, NTC_data->clockErr);            
    }

    // Write OCN data to file
    // fwrite(buf, w - buf ,1, fp);
    // fclose(fp);
    write(fOut, buf, w - buf);
    close(fOut);
    umask(old_mask);
}


int pull_telemetry_batch(int *ring_read_pos, int *prior_packet_id, void * shared_memory_handle, int *packets_recorded, int priorId, int *holding_buffer_size, void* holding_buffer, int prior_packet_size )
{

    int starting_ring_read_pos = * ring_read_pos;

    int read_bytes = 0;
    int read_bytes2 = 0;

    // If we don't know the size of the packet we are looking at next (only available from prior packet data)
    // Then perform two reads on the ring buffer. One for the head to get the size of the packet. Then the rest of the contents.
    if (prior_packet_size < 1)
    {
        Radclock_Telemetry_Latest data;

        read_bytes += ring_buffer_read(&data.header, sizeof(Radclock_Telemetry_Header), shared_memory_handle, ring_read_pos, 1);
        if (read_bytes < 1)
        {
            // if (read_bytes != RADCLOCK_ERROR_NO_NEW_DATA)
            //     printf("Error: %d - %s\n", read_bytes, radclock_errorstr(read_bytes));
            return read_bytes;        
        }

        if (*prior_packet_id >= data.header.packetId)
            return RADCLOCK_ERROR_NO_NEW_DATA;        
            
        // If the packet is larger than the holding buffer then make a new larger holding buffer
        check_and_grow_holding_buffer(holding_buffer_size, data.header.size, holding_buffer);

        read_bytes2 = ring_buffer_read(holding_buffer + sizeof(Radclock_Telemetry_Header), data.header.size - sizeof(Radclock_Telemetry_Header), shared_memory_handle, ring_read_pos, 0);
        if (read_bytes2 < 1)
        {
            // if (read_bytes2 != RADCLOCK_ERROR_NO_NEW_DATA)
            //     printf("Error: %d - %s\n", read_bytes2, radclock_errorstr(read_bytes2));
            return read_bytes2;        
        }   

        read_bytes += read_bytes2;

        // Copy header into the holding buffer
        memcpy(holding_buffer, &data, sizeof(Radclock_Telemetry_Header));
    }
    else
    {
        // If the packet is larger than the holding buffer then make a new larger holding buffer
        check_and_grow_holding_buffer(holding_buffer_size, prior_packet_size, holding_buffer);

        read_bytes += ring_buffer_read(holding_buffer, prior_packet_size, shared_memory_handle, ring_read_pos, 1);
        
        // data = *(Radclock_Telemetry_Latest *) holding_buffer;

        if (read_bytes < 1)
        {
            if (read_bytes != RADCLOCK_ERROR_NO_NEW_DATA)
                verbose(LOG_ERR, "Telemetry Consumer - Error: %d - %s", read_bytes, radclock_errorstr(read_bytes));
            return read_bytes;        
        }     
        if (prior_packet_size!= read_bytes)
            verbose(LOG_ERR, "Telemetry Consumer - prior_packet_size error2 %d %d %d %d", prior_packet_size, read_bytes, starting_ring_read_pos%RADCLOCK_RING_BASE_BUFFER_BYTES, RADCLOCK_RING_BASE_BUFFER_BYTES);             
    }
    

    Radclock_Telemetry_Latest new_data = *(Radclock_Telemetry_Latest *) holding_buffer;

    if (read_bytes != new_data.header.size)
    {
        verbose(LOG_ERR, "Telemetry Consumer - Error packet bytes: %d different from packet.header.size: %d (%d, %d)", read_bytes, new_data.header.size, read_bytes-read_bytes2, read_bytes2);
        if (EXIT_ON_TERMINAL_ERROR)
            exit(0);
    }
    
    if (RADCLOCK_TELEMETRY_VERSION != new_data.header.version)
    {
        verbose(LOG_ERR, "Telemetry Consumer - Error packet version: %d is different from expected: %d", new_data.header.version, RADCLOCK_TELEMETRY_VERSION);
        if (EXIT_ON_TERMINAL_ERROR)
            exit(0);
    }

    // Sanity check on ring_read_pos
    if (*ring_read_pos < 0)
    {
        verbose(LOG_ERR, "Telemetry Consumer - Error read head set %d less than 0", *ring_read_pos);
        if (EXIT_ON_TERMINAL_ERROR)
            exit(0);
    }

    // Callback to process received packet - For radclock this will store to file
    telemetry_consumer_callback_method(holding_buffer, new_data.header.packetId, new_data.header.size, new_data.timestamp);

    int current_packet_id = new_data.header.packetId;

    // Where this consumer has started later than the producer and there is a backlog of producer messages to read
    // Otherwise the consumer stopped reading packets for some time 
    // In which case attempt to read prior packets 
    if (current_packet_id != *prior_packet_id + 1)
    {
        int prior_packet_read_pos = -1;
        int new_prior_packet_id = -1;
        int new_packet_size = -1;
        if (get_prior_packet_available(shared_memory_handle, starting_ring_read_pos, &prior_packet_read_pos, &new_packet_size))
            pull_telemetry_batch(&prior_packet_read_pos, prior_packet_id, shared_memory_handle, packets_recorded, priorId+1, holding_buffer_size, holding_buffer, new_packet_size);
    }

    if (TELEMETRY_CONSUMER_LOG_VERBOSITY && new_data.header.packetId % TELEMETRY_CONSUMER_LOG_PACKET_INTERVAL == 0)
        verbose(LOG_ERR, "Telemetry Consumer - Client: rp:%d prior:%d packet:%d v:%d picn:%d size:(%d, %d) (%d %d %4.2f%%)", *ring_read_pos, priorId, new_data.header.packetId, new_data.header.version, new_data.PICN, new_data.header.size, read_bytes, new_data.header.packetId, *packets_recorded, (double)(*packets_recorded) *100.0 / (double)new_data.header.packetId);
    *packets_recorded += 1;

    *prior_packet_id = current_packet_id;
    return read_bytes;
}

