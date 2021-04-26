#include <stdio.h>
#include <syslog.h>

#include "radclock_telemetry.h"

Radclock_Telemetry_Latest make_telemetry_packet(int packetId, int PICN, int NTC_Count, long double timestamp, int accepted_public_ntp, int rejected_public_ntp, int inband_signal)
{
    // Make telemetry packet
    Radclock_Telemetry_Latest data;

    // Header Data - Should never change - Speficies first bytes of the packet to be the size and version of the packet
    data.header.size = sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_NTC_Latest)*NTC_Count + sizeof(Radclock_Telemetry_Footer);
    data.header.version = RADCLOCK_TELEMETRY_VERSION;
    data.header.packetId = packetId;

    // Sanity check on PICN
    // if (PICN < 0 || PICN >= NTC_Count)
    //     verbose(LOG_ERR, "Telemetry Producer - Error PICN is set to an invalid value %d NTC_Count:%d\n", PICN, NTC_Count);

    // OCN data 
    data.PICN = PICN;
    // data.asym = asym;
    data.NTC_Count = NTC_Count;
    data.timestamp = timestamp;

    data.accepted_public_ntp = accepted_public_ntp;
    data.rejected_public_ntp = rejected_public_ntp;
    data.inband_signal = inband_signal;

    return data;
}



Radclock_Telemetry_NTC_Latest make_telemetry_NTC_packet(unsigned int status_word, int NTC_id, double uA, double err_bound)
{
    Radclock_Telemetry_NTC_Latest data;

    // NTC server packet data
    data.status_word = status_word;
    data.NTC_id = NTC_id;
    data.uA = uA;
    data.err_bound = err_bound;

    return data;
}

Radclock_Telemetry_Footer make_telemetry_footer(int size)
{
    Radclock_Telemetry_Footer data;
    data.size = size;
    return data;
}

void print_telegraf(char * filename, const char * log_dir, const char * processed_log_dir, char * hostname)
{
    // Print format as per influx line protocol. Timestamp precision by telegraf set to '1ns'
 
    char old_name[128];
    char new_name[128];
    sprintf(old_name, "%s/%s", log_dir, filename);


    FILE * fp = fopen(old_name, "r");
    Radclock_Telemetry_Header  header;

    fread(&header, sizeof(header), 1, fp);

    if (header.version == 3)
    {
        struct Radclock_Telemetry_v3 ocn_data;
        // Offset by header bytes as they have already been read in
        fread((void*)(&ocn_data)+sizeof(Radclock_Telemetry_Header), sizeof(ocn_data) - sizeof(Radclock_Telemetry_Header), 1, fp);
        struct Radclock_Telemetry_ICN_v3 ntc_data;

        fread((void*)(&ntc_data), sizeof(ntc_data), 1, fp);

        printf("clock_stats,ocn=%s ", hostname);

        printf("picn=%d,",      ocn_data.PICN);
        // printf("asym=%d,",      ocn_data.asym);
        printf("ntc_count=%d,", ocn_data.NTC_Count);
        printf("ocn_clock_ts=%.09Lf,", ocn_data.timestamp); // Timestamp in seconds (cant pass in nanoseconds as far as I can tell)
        printf("ocn_clock_ICN_1_uA=%.20f,", ntc_data.uA); // uA
        printf("ocn_clock_ICN_1_err_bound=%.20f", ntc_data.err_bound); // err_bound
        // todo fix so this isnt a decimal (seems some precision issues occur when doing the operation below)
        printf(" %.0Lf\n",ocn_data.timestamp*1000000000); // End of the line must end with a timestamp 

    }
    else if (header.version == 5)
    {
        struct Radclock_Telemetry_v5 ocn_data;
        // Offset by header bytes as they have already been read in
        fread((void*)(&ocn_data)+sizeof(Radclock_Telemetry_Header), sizeof(ocn_data) - sizeof(Radclock_Telemetry_Header), 1, fp);
        struct Radclock_Telemetry_NTC_v5 ntc_data;

        fread((void*)(&ntc_data), sizeof(ntc_data), 1, fp);

        printf("clock_stats,ocn=%s ", hostname);

        printf("picn=%d,",      ocn_data.PICN);
        // printf("asym=%d,",      ocn_data.asym);
        printf("ntc_count=%d,", ocn_data.NTC_Count);
        printf("ocn_clock_ts=%.09Lf,", ocn_data.timestamp); // Timestamp in seconds (cant pass in nanoseconds as far as I can tell)
        printf("ocn_clock_ICN_1_uA=%.20f,", ntc_data.uA); // uA
        printf("ocn_clock_ICN_1_err_bound=%.20f", ntc_data.err_bound); // err_bound
        // todo fix so this isnt a decimal (seems some precision issues occur when doing the operation below)
        printf(" %.0Lf\n",ocn_data.timestamp*1000000000); // End of the line must end with a timestamp 

    }
    else

    {
        // Error unhandled version
        printf("Unknown packet version %d\n", header.version);
    }
    
    fclose(fp);

    // Move file to read folder
    sprintf(new_name, "%s/%s", processed_log_dir, filename);
    rename (old_name, new_name);
    
}
