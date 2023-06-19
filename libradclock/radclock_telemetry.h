#ifndef RADCLOCK_TELEMETRY
#define RADCLOCK_TELEMETRY

#include  <stdint.h> //uint64_t

#define TELEMETRY_CACHE_OLD_DIR "/radclock.old"
#define TELEMETRY_CACHE_DIR "/radclock"

#define TELEMETRY_KEEP_ALIVE_SECONDS (5 * 60) // Send keep alive messages every 5 min
#define RADCLOCK_TELEMETRY_VERSION 8

typedef struct Radclock_Telemetry_v8 Radclock_Telemetry_Latest;
typedef struct Radclock_Telemetry_NTC_v8 Radclock_Telemetry_NTC_Latest;
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

struct Radclock_Telemetry_v3
{
	// Header must always be first
	Radclock_Telemetry_Header header;

	// RAD node specific data
	int PICN;
	int NTC_Count;
	long double timestamp;
};

struct Radclock_Telemetry_v5
{
	// Header must always be first
	Radclock_Telemetry_Header header;

	// RAD node specific data
	int PICN;
	int NTC_Count;
	long double timestamp;
	int accepted_public_ntp;
	int rejected_public_ntp;

	int inband_signal;
};

struct Radclock_Telemetry_v6
{
	// Header must always be first
	Radclock_Telemetry_Header header;

	// RAD node specific data
	int PICN;
	int NTC_Count;
	long double timestamp;
	int delta_accepted_public_ntp;
	int delta_rejected_public_ntp;
	unsigned int public_ntp_state;
	uint64_t servertrust;
	uint64_t sa;
};


struct Radclock_Telemetry_v7
{
	// Header must always be first
	Radclock_Telemetry_Header header;

	// RAD node specific data
	int PICN;
	int NTC_Count;
	long double timestamp;
	int delta_accepted_public_ntp;
	int delta_rejected_public_ntp;
	unsigned int public_ntp_state;
	uint64_t servertrust;
	uint64_t sa;
};

struct Radclock_Telemetry_v8
{
	// Header must always be first
	Radclock_Telemetry_Header header;

	// RAD node specific data
	int PICN;
	int NTC_Count;
	long double timestamp;
	int delta_accepted_public_ntp;
	int delta_rejected_public_ntp;
	unsigned int public_ntp_state;
	uint64_t servertrust;
	uint64_t sa;              // defined but zero for non-ICNs with telemetry
};


/* Structure holding data of a single (RADclock) maintained by a RAD node */
struct Radclock_Telemetry_ICN_v3
{
	unsigned int status_word;
	int NTC_id;
	double uA;
	double err_bound;
};

struct Radclock_Telemetry_NTC_v5
{
	unsigned int status_word;
	int NTC_id;
	double uA;
	double err_bound;
};

struct Radclock_Telemetry_NTC_v6
{
	unsigned int status_word;
	int NTC_id;
	double uA;
	double err_bound;
};

struct Radclock_Telemetry_NTC_v7
{
	unsigned int status_word;
	int NTC_id;
	double uA;
	double err_bound;
	double min_RTT;
};

struct Radclock_Telemetry_NTC_v8
{
	unsigned int status_word;
	int NTC_id;
	double uA;
	double err_bound;
	double min_RTT;
	double clockErr;	       // defined but zero for non-ICNs with telemetry
};


Radclock_Telemetry_Latest make_telemetry_packet(int packetId, int PICN,
    int NTC_Count, long double timestamp, int delta_accepted_public_ntp,
    int delta_rejected_public_ntp, uint64_t servertrust,
    unsigned int public_ntp_state, uint64_t sa);

Radclock_Telemetry_NTC_Latest make_telemetry_NTC_packet(unsigned int status_word,
    int NTC_id, double uA, double err_bound, double min_RTT, double clockErr);

Radclock_Telemetry_Footer make_telemetry_footer(int size);

void print_telegraf(char * filename, const char * log_dir,
    const char * processed_log_dir, char * hostname);


#endif // #ifndef RADCLOCK_TELEMETRY
