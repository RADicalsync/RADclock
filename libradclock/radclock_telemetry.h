#ifndef RADCLOCK_TELEMETRY
#define RADCLOCK_TELEMETRY

#define TELEMETRY_CACHE_OLD_DIR "/radclock.old"
#define TELEMETRY_CACHE_DIR "/radclock"

#define TELEMETRY_KEEP_ALIVE_SECONDS (5 * 60) // Send keep alive messages once every 5 minutes
#define RADCLOCK_TELEMETRY_VERSION 2

typedef struct Radclock_Telemetry_v2 Radclock_Telemetry_Latest;
typedef struct Radclock_Telemetry_ICN_v2 Radclock_Telemetry_ICN_Latest;
typedef struct Radclock_Telemetry_Header Radclock_Telemetry_Header;
typedef struct Radclock_Telemetry_Footer Radclock_Telemetry_Footer;

struct Radclock_Telemetry_Header
{
    int size;
    int version;
    int packetId;
};

struct Radclock_Telemetry_Footer
{
    int size;
};

struct Radclock_Telemetry_v1
{
    // Header must always be first
    Radclock_Telemetry_Header header;

    // OCN specific data
    int PICN;
    int asym;
    int ICN_Count;
    double timestamp;
};

struct Radclock_Telemetry_v2
{
    // Header must always be first
    Radclock_Telemetry_Header header;

    // OCN specific data
    int PICN;
    double asym;
    int ICN_Count;
    long double timestamp;
};


struct Radclock_Telemetry_ICN_v1
{
   int status_word;
   int ICN_id;
   int uA;
   int err_bound;
};

struct Radclock_Telemetry_ICN_v2
{
   unsigned int status_word;
   int ICN_id;
   double uA;
   double err_bound;
};

Radclock_Telemetry_Latest make_telemetry_packet(int packetId, int PICN, double asym, int ICN_Count, long double timestamp);

Radclock_Telemetry_ICN_Latest make_telemetry_ICN_packet(unsigned int status_word, int ICN_id, double uA, double err_bound);

Radclock_Telemetry_Footer make_telemetry_footer(int size);

void print_telegraf(char * filename, const char * log_dir, const char * processed_log_dir, char * hostname);


#endif // #ifndef RADCLOCK_TELEMETRY