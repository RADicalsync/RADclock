#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <wolfssl/wolfcrypt/sha.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>


#include "dagapi.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>
#include <time.h>

#include <pcap.h>

/* Building not governed by a makefile, so refer to needed RADrepo includes explicitly */
#include "../radclock/proto_ntp.h"
#include "../radclock/ntp_auth.h"

#define CONNECT_TO_SERVER 1

#define PORT_NTP    123


/* Internals */
#define STRSIZE    512
#define BUFSIZE_IN    25000000	/* 25 MB */

/*
 * Masks for bit positions in ERF flags byte.
 */
#define FILTER_TRUNC_MASK  0x08
#define FILTER_RX_MASK     0x10
#define FILTER_DS_MASK     0x20

/* Edited from dag_extract.c
 * IP layer and above
 */
#if defined(__FreeBSD__) || defined (__APPLE__)
#define GET_UDP_SRC_PORT(x) ntohs(x->uh_sport)
#define GET_UDP_DST_PORT(x) ntohs(x->uh_dport)
#else
#define GET_UDP_SRC_PORT(x) ntohs(x->source)
#define GET_UDP_DST_PORT(x) ntohs(x->dest)
#endif



/* Override ntp_auth.c 's use of "verbose()" to get local console print. */
void verbose(int facility, const char* format, ...)
{
	va_list arglist;
	va_start( arglist, format );
	vprintf( format, arglist );
	va_end( arglist );
}

/* dagstamp data structure passed to the TrustNode for perfstamp matching. */
struct dag_cap {
	long double Tout;       // DAG timestamp of          outgoing NTP request
	long double Tin;        // DAG timestamp of matching incoming NTP response
	l_fp stampid;           // stamp matching id, in original l_fp format
	struct in_addr ip;      // IP the targeted server used when responding
};

/* dagstamp plus additional fields supporting the dagstamp matching. */
struct dag_ntp_record {
	int key_id;             // auth key  in outgoing NTP request
	struct in_addr IPout;   // server IP in outgoing NTP request
	struct dag_cap cap;     // dagstamp data to be collected
};

/* Circular buffer used to store and complete dagstamps
 * [using in-situ completion of half-dagstamps]
 */
#define PKT_LIST_SIZE 512
struct rec_list {
	struct dag_ntp_record *recs;
	int write_id;
	int size;
};



/* Same as in dag_extract.c
 * Helper
 */
#define subtract_timeval(a, b, result)                    \
  do {                                                    \
    (result)->tv_sec  = (a)->tv_sec  - (b)->tv_sec;       \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;      \
    if ((result)->tv_usec < 0) {                          \
      --(result)->tv_sec;                                 \
      (result)->tv_usec += 1000000;                       \
    }                                                     \
  } while (0)


///* Copied from RADrepo/radclock/misc.h */
//static inline long double
//NTPtime_to_UTCld(l_fp ntp_ts)
//{
//	long double  time;
//	time  = (long double)(ntohl(ntp_ts.l_int) - JAN_1970);
//	time += (long double)(ntohl(ntp_ts.l_fra)) / 4294967296.0;
//	return (time);
//}

/* Same as in dag_extract.c
 * Converts unsigned long long timestamp from the DAG card to s and ns.
 */
static void
ts_to_s_ns(unsigned long long ts, unsigned long *s, unsigned long *ns)
{
	*s = (ts>>32)&0xFFFFFFFFULL;
	ts = ((ts&0xFFFFFFFFULL)*1000*1000*1000);
	ts += (ts & 0x80000000ULL)<<1;
	*ns = ts>>32;
	if (*ns >= 1000000000) {
		*ns -= 1000000000;
		*s += 1;
	}
}


/* Same as in dag_extract.c
 * Input stream
 * This is DAG input only 
 * There should be one only, but in the future?
 */
struct instream_t {
	char* filename;
	int fd;
	long long int filesize;
	uint8_t filter_opts;      /* DAG header filers */
	unsigned char *buffer;    /* Input buffer of DAG records */
	unsigned char *cur;       /* Current record */
	long int bsize;           /* How much data in buffer */
	long long int read;       /* Chunk of file that has been read */
	uint8_t linktype;
	uint8_t pload_padding;
};


/* Same as in dag_extract.c
 * Counter variables used to hold statistics by the report function.
 */
struct stats_t {
	struct timeval now;
	uint64_t bytes_in;
	uint64_t records_in;
	uint64_t records_out;
	struct timeval last;
	uint64_t last_bytes_in;
	uint64_t last_records_in;
	uint64_t last_records_out;
	uint64_t records_bad;
};


/* Same as in dag_extract2.c,  with part commented out
 * How fast we are reading/writing, how much data left
 */
static void
report(struct instream_t *istream, struct stats_t *stats)
{
	struct timeval time_diff;
	double in_rate;
	double out_rate;
	double byte_rate = 0;
	double second;

	gettimeofday(&(stats->now), NULL);
	subtract_timeval(&(stats->now), &(stats->last), &time_diff);

	second      = (double) time_diff.tv_sec + ((double) time_diff.tv_usec / 1000000.0);
	in_rate     = (double) (stats->records_in - stats->last_records_in) / second;
	out_rate    = (double) (stats->records_out - stats->last_records_out) / second;
	byte_rate   = (double) (stats->bytes_in - stats->last_bytes_in) / second;

//	fprintf(stdout, "   File: %5lld MB  |  Read: %10"PRIu64
//			" rec, %9.1f rec/s, %5.1f MB/s  |  Write: %10"PRIu64
//			" rec, %9.1f rec/s  |  Bad: %3"PRIu64"\r",
//			(in->filesize - in->read) / 1048576,
//			stats->records_in,
//			in_rate,
//			byte_rate / 1048576.0,
//			stats->records_out,
//			out_rate,
//			stats->records_bad
//			);
//	fflush(stdout);

	/* Record last stats for next time */
	stats->last_records_in  = stats->records_in;
	stats->last_records_out = stats->records_out;
	stats->last_bytes_in    = stats->bytes_in;
	stats->last             = stats->now;
}



int check_signature(int packet_size, struct ntp_pkt *pkt_in, char **key_data, int *auth_bytes, int *key_id)
{
	wc_Sha sha;
	unsigned char pck_dgst[20];
	*key_id = -1;        // No AUTH used
	packet_size -= 8;    // offset packet size
	int auth_pass = 0;
	*auth_bytes = 0;

	if (packet_size > LEN_PKT_NOMAC && key_data) {
		*key_id = ntohl( pkt_in->exten[0] );
		if (*key_id > 0 && *key_id < MAX_NTP_KEYS && key_data[*key_id]) {
			wc_InitSha(&sha);
			wc_ShaUpdate(&sha, (const byte *)key_data[*key_id], 20);  // DV: added the cast
			wc_ShaUpdate(&sha, (const byte *)pkt_in, LEN_PKT_NOMAC);
			wc_ShaFinal((wc_Sha*)&sha, (byte*)pck_dgst);
//			printf("size:%d %s %d key\n", packet_size, key_data[*key_id], *key_id);

			if (memcmp(pck_dgst, ((struct ntp_pkt*)pkt_in)->mac, 20) == 0) {
//				printf( "NTP client authentication SUCCESS\n");
				auth_pass = 1;
			} else
				printf("NTP client authentication FAILURE\n");

			*auth_bytes = 24;
		} else
			printf("NTP client authentication request invalid key_id %d pkt_size:(%d %lu)\n",
			    *key_id, packet_size, sizeof(struct ntp_pkt));
	} else
		if ( packet_size <= LEN_PKT_NOMAC ) {
			printf("NTP Non auth message\n");
			auth_pass = 1;
		} else
			if ( !key_data )
				printf("NTP client authentication request when this server has no keys\n");

	return auth_pass;
}



/*
 * Does pkt extraction, insertion and in-situ matching in cap_list buffer,
 * then sending of sucessfully matched dagstamps to the TN.
 */
static long int
match_and_send(dag_record_t *header, void *payload,
    struct rec_list *cap_list, char *tn_ip, int s_server, struct sockaddr_in s_to, char **key_data)
{
	struct timespec ts;
	struct ether_header *hdr_eth;
	struct ip *hdr_ip;
	struct udphdr *hdr_udp;
	struct ntp_pkt *ntp;
	struct dag_ntp_record *thisrec;
	int sport, dport;
	int isSrc = 0;
	long double ts_ld;

	/* Extract DAG timestamp, convert to format expected by dap_cap */
	ts_to_s_ns(header->ts, (unsigned long *) &ts.tv_sec, (unsigned long *) &ts.tv_nsec);
	ts_ld = ts.tv_sec + (long double)(ts.tv_nsec * 1e-9);

	/* Extract all pkt headers Eth[IP[UDP[NTP]]] from DAG record payload */
	hdr_eth = (struct ether_header *) ((char *) payload);
	hdr_ip  = (struct ip *) ((char *) hdr_eth + sizeof(struct ether_header));
	hdr_udp = (struct udphdr *) ((char *) hdr_ip + hdr_ip->ip_hl*4);
	ntp     = (struct ntp_pkt *) ((char *) hdr_udp + sizeof(struct udphdr));

	/* Extract source and destination IP and port values */
	char src[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(hdr_ip->ip_src), src, INET_ADDRSTRLEN);
	char dst[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(hdr_ip->ip_dst), dst, INET_ADDRSTRLEN);
	sport = GET_UDP_SRC_PORT(hdr_udp);
	dport = GET_UDP_DST_PORT(hdr_udp);

	if (strcmp(src, tn_ip) == 0)
		isSrc = 1;    // is an outgoing pkt from the TN
	printf("Found %s (%d)-> %s (%d)  isSRc = %d\n", src, sport, dst, dport, isSrc);

	/* Begin packet filtering then matching */
	if ((sport == PORT_NTP && strcmp(dst, tn_ip) == 0) ||
		(dport == PORT_NTP && isSrc) )
	{
		printf("  passed (IP,port) filter\n");
		int packet_size = ntohs(hdr_udp->uh_ulen);
		int auth_bytes = 0;

		int key_id = -1;
		if (check_signature(packet_size, ntp, key_data, &auth_bytes, &key_id)) {
			printf("  passed auth check\n");
			if (isSrc) {
				thisrec = &cap_list->recs[cap_list->write_id];  // current rec
				memcpy(&thisrec->IPout, &hdr_ip->ip_dst, sizeof(thisrec->IPout));
				thisrec->key_id = key_id;
				printf("   request pkt with key_id: %d inserted at %d \n", key_id, cap_list->write_id);
				thisrec->cap.Tout = ts_ld;
				thisrec->cap.stampid = ntp->xmt;
				cap_list->write_id = (cap_list->write_id + 1) % cap_list->size;
			} else {
				printf("   response pkt: looking for match with key = %d\n", thisrec->key_id);

				/* Set search order  (in each case, i = attempt count)
				 *   Original:  will find earliest first, but v-inefficient search
				 *   Efficient: backward from last insertion, finds match v-fast, but MAy not be 1st
				 */
				int match_id = -1;
//				for (int i = 0; i < cap_list->size && match_id == -1; i++) {      // Original
//					thisrec = &cap_list->recs[i];                                 // Original
				for (int i = 0; i < cap_list->write_id && match_id == -1; i++) {  // Efficient
					thisrec = &cap_list->recs[cap_list->write_id - 1 - i];        // Efficient

					if (memcmp(&ntp->org, &thisrec->cap.stampid, sizeof(ntp->org)) == 0) {
						printf("    > stampid match\n");
						if (key_id == thisrec->key_id) { // same auth method must be present in sent and rcv
							printf("    >> key_id match\n");
							if (memcmp(&hdr_ip->ip_src, &thisrec->IPout, sizeof(hdr_ip->ip_src)) == 0) {
								match_id = i;	// record the first match (will exit loop)
								printf("    >>> server IP match\n");
							} else
								printf("   matched pkt with server IP change: sent to %s, repied with %s\n",
								    inet_ntoa(thisrec->IPout), inet_ntoa(hdr_ip->ip_src));
						}
					}
				}

				/* thisrec is a match, complete remaining fields and send rec */
				if (match_id != -1) {
					thisrec->cap.Tin = ts_ld;
					memcpy(&thisrec->cap.ip, &hdr_ip->ip_src, sizeof(thisrec->cap.ip));

					printf("    Matched packet for %s. Auth %d\n", inet_ntoa(thisrec->cap.ip), key_id);
					if (CONNECT_TO_SERVER) {
						int ret = sendto(s_server,
						    (char *)&thisrec->cap, sizeof(struct dag_cap), 0,
						    (struct sockaddr *) &(s_to), sizeof(struct sockaddr_in));
					}

					/* Match buffer diagnostics */
					printf("%d: (%ld.%09ld %.09Lf) %d\n\n", match_id, ts.tv_sec,
					    ts.tv_nsec, ts_ld, ntohs(hdr_udp->uh_ulen));

					/* Eliminate risk of re-matching on this buffer entry */
					memset(&thisrec->cap.stampid, 0, sizeof(ntp->org));

				} else {
//					long double org_ts = NTPtime_to_UTCld(ntp->org);
//					printf("    Unable to find packet matching stampid %Lf\n", org_ts);
					uint64_t id;    // match RADclock's convention for recording stampid
					id = ((uint64_t) ntohl(ntp->org.l_int)) << 32;
					id |= (uint64_t) ntohl(ntp->org.l_fra);
					printf("    Unable to find packet matching stampid %llu\n",
					    (unsigned long long) id);
				}
			}
		}
	}
	return 0;
}



/* Based on dag_extract.c, with some dag_extract2.c improvements
 * Init input stream
 */
static int 
init_instream( struct instream_t *s, const char* filename )
{
	struct stat st;
	dag_record_t rec;

	memset(s, 0, sizeof(struct instream_t));
	s->filename = (char *) malloc (STRSIZE * sizeof(char));
	strcpy(s->filename, filename);

	/* Initialise input buffer */
	s->cur      = s->buffer;
	s->bsize    = 0;
	s->buffer   = (unsigned char *) malloc(BUFSIZE_IN);

	/* Set filter options. Remove rx errors (link layer), ds errors (framing)
	 * and truncated packets
	 */
	s->filter_opts = 0;
	s->filter_opts |= FILTER_RX_MASK;
	s->filter_opts |= FILTER_DS_MASK;  /* Code relies on this being set */
	s->filter_opts |= FILTER_TRUNC_MASK;

	/* Get file size, with LFS support, this is defined as a long long int */
	s->read = 0;
	s->filesize = 0;
	if (stat(s->filename, &st) < 0) {
		fprintf(stderr, "Stat failed on instream: %s\n", strerror(errno));
		return 1;
	}
	s->filesize = (long long int)st.st_size;
	fprintf(stderr, "Instream size is %.1f MB\n", (double)s->filesize / 1048576.0);

	/* Open input ERF file */
	//s->fd = open(s->filename, O_RDONLY|O_LARGEFILE);
	s->fd = open(filename, O_RDONLY);
	if (s->fd == -1) {
		fprintf(stderr, "Could not open %s for reading.\n", filename);
		return 1;
	}

	/* Get the link type from the ERF file */	
	read(s->fd, &rec, dag_record_size);
	lseek(s->fd, 0, SEEK_SET);

	switch (rec.type & 0x7f) {
		case TYPE_ATM:
		case TYPE_MC_ATM:
		case TYPE_AAL5:
		case TYPE_MC_AAL5:
		case TYPE_AAL2:
		case TYPE_MC_AAL2:
		case TYPE_ETH:
		case TYPE_COLOR_ETH:
		case TYPE_DSM_COLOR_ETH:
		case TYPE_MC_HDLC:
		case TYPE_COLOR_MC_HDLC_POS:
		case TYPE_HDLC_POS:
		case TYPE_COLOR_HDLC_POS:
		case TYPE_DSM_COLOR_HDLC_POS:
		case TYPE_MC_RAW:
		case TYPE_MC_RAW_CHANNEL:
#ifdef TYPE_RAW_LINK
		case TYPE_RAW_LINK:
#endif
			s->linktype = rec.type & 0x7f;
			break;
		default:
			fprintf(stderr, "ERROR: Cannot init input stream, bad linktype\n");
			return 1;
	}

	s->pload_padding = 0;
	if (s->linktype == TYPE_ETH)
		s->pload_padding = 2;

	if (s->linktype != TYPE_ETH) {
		fprintf(stderr, "ERROR: bpffilter does not know this link layer. Implement it!\n");
		return 1;
	}

	return 0;
}


/* Same as in dag_extract.c
 * Destroy input stream
 */
static void
destroy_instream ( struct instream_t *s )
{
	close(s->fd);
	free(s->filename);
	free(s->buffer);
}




/* Same as in dag_extract.c
 * Get pointer to the ERF header for the next packet in the input stream and
 * play a bit with buffers.
 * Returns null if no further packets are available.
 */
static dag_record_t *
get_next_erf_header(struct instream_t *istream, struct stats_t *stats,
    dag_record_t **header, void **payload)
{
	int rd = 0;
	int rlen = 0;
	dag_record_t *rec = NULL;

	/*
	 * Keep looking for a valid record that passes the DAG filtering.
	 */
	while (1) {
		rd = 0;
		rlen = 0;
		rec = NULL;

		/* Fill input buffer when it gets empty
		 * Make sure we have a full DAG record, then a full payload.
		 * If not, we are probably done with the job
		 */
		if ( (istream->bsize) < dag_record_size ) {
			/* Partial DAG record, reposition at beginning and fill buffer */
			memcpy(istream->buffer, istream->cur, istream->bsize);
			istream->cur = istream->buffer;
			report(istream, stats);

			if ( (istream->filesize - istream->read) > 0 ) {
				rd = read(istream->fd, istream->cur + istream->bsize,
				    BUFSIZE_IN - istream->bsize);
				istream->read += rd;
				istream->bsize += rd;
			} else
				return NULL;
		}

		/* We have a complete DAG record */
		rec = (dag_record_t *) (istream->cur);
		rlen = ntohs(rec->rlen);

		if (rlen == 20) {
			/* 
			 * DS error truncates the packet, but the packet has effectively been
			 * padded to 28 bytes by the card.
			 */
			rlen = 28;
			fprintf(stdout, "# WARNING: %d DS adjustment rlen = %d\n", __LINE__, rlen);
		}

		/*
		 * Make sure we have a complete payload
		 */
		if ( istream->bsize < rlen ) {
			/* Partial payload, reposition at beginning and fill buffer */
			memcpy(istream->buffer, istream->cur, istream->bsize);
			istream->cur = istream->buffer;
			report(istream, stats);

			if ( (istream->filesize - istream->read) > 0 ) {
				rd = read(istream->fd, istream->cur + istream->bsize,
				    BUFSIZE_IN - istream->bsize);
				istream->read  += rd;
				istream->bsize += rd;
			} else
				return NULL;
		}
		rec = (dag_record_t *) (istream->cur);
		rlen = ntohs(rec->rlen);

		/*
		 * Filter based on ERF flags 
		 * for efficiency the dag_header is cast to an anonymous
		 * structure which replaces the bitfields of dag_header_t
		 * with an u_char
		 */
		if (istream->filter_opts && (((struct {
					 uint64_t ts;
					 unsigned char type;
					 unsigned char flags;}
					 *)rec)->flags & istream->filter_opts))
		{
			/* Filter suggests packet should be discarded. Now we are in trouble
			 * so take the time to do some recovery
			*/
			stats->records_bad++;
			if (rlen == 0) {
				istream->cur   += dag_record_size;
				istream->bsize -= dag_record_size;
			} else {
				istream->cur   += rlen;
				istream->bsize -= rlen;
			}
			continue;
		}
		
		/* Sanity check */
		if ( rec->type != istream->linktype ) {
			fprintf(stderr, "ERROR: Record and stream have different linktype. %s %d\n",
			    __FILE__, __LINE__);
			return NULL;
		}

		/* Sanity check */
		// TODO that one should be watched for problems ...
		if (rlen == 0) {
			fprintf(stderr, "ERROR: rlen=0 record uncaught by input filters. %s %d\n",
			    __FILE__, __LINE__);
			istream->cur   += dag_record_size;
			istream->bsize -= dag_record_size;
			*header = NULL;
			*payload = NULL;
			return NULL;
		}

		/* XXX Some DAG magic in here. There are some extras bytes for the ETHERNET
		 * captures. Should make this generic, to handle different linklayer types.
		 */
		*header = (dag_record_t *)istream->cur;
		*payload = (void *)(istream->cur + (dag_record_size + istream->pload_padding));
		istream->cur += rlen;
		istream->bsize -= rlen;

/*
 * New DAG type? What for?
		if (rec->type == TYPE_PAD) {
			fprintf(stderr, "%d  !!! rec TYPE_PAD\n", __LINE__);
			istream->cur += dag_record_size;
			continue;
		}
*/

		return rec;
	}
}



/*
 * The heart of dagconvert.  Reads ERF headers and payloads from an input
 * reader, applies any filtering and sends (possibly modified) ERF headers and
 * payloads to output writer.
 */
static void
process( struct instream_t *istream, int os_len, struct stats_t *stats,
    struct rec_list *cap_list, char *tn_ip, int s_server, struct sockaddr_in s_to, char **key_data)
{
	int i;
	struct outstream_t *s = NULL;
	dag_record_t *header = NULL;
	void *payload = NULL;
	struct stat st;
	int length;

	while(1) {
		length = lseek(istream->fd, 0, SEEK_CUR);

		if (get_next_erf_header(istream, stats, &header, &payload) == NULL) {
			// In the case that we have hit the end of the file
			// Wait a 1 second then attempt to read more data

			sleep(1);

			// fprintf(stderr, "get_next_erf_header returned NULL\n");
			lseek(istream->fd, length, SEEK_SET);

			if (stat(istream->filename, &st) < 0) {
				fprintf(stderr, "Stat failed on instream: %s\n", strerror(errno));
				return;
			}
			istream->filesize = (long long int)st.st_size;

			continue;
			// break;
		}
		length = lseek(istream->fd, 0, SEEK_CUR);

		stats->records_in++;
		stats->bytes_in += ntohs(header->rlen);

		stats->records_out++;    // want this if match fails?
		match_and_send(header, payload, cap_list, tn_ip, s_server, s_to, key_data);

	}
}


void init_cap_list(struct rec_list *cap_list)
{
	cap_list->recs = malloc(sizeof(struct dag_ntp_record) * PKT_LIST_SIZE); // use calloc instead
	cap_list->size = PKT_LIST_SIZE;
	cap_list->write_id = 0;

	// Set all the rcv time stamps to 0 to avoid incorrect matches
	// TODO: why only receive timestamps? why not the entire structure.
	for (int i = 0; i < PKT_LIST_SIZE; i++)
		memset(&cap_list->recs[i].cap.stampid, 0, sizeof(cap_list->recs[i].cap.stampid));
}


int
main(int argc, char **argv)
{
	if (argc < 3) {
		printf("Usage:  dagstamp_gen  cap.erf  TN_IP\n");
		printf("Eg: ./dagstamp_gen  /tmp/trustnode_NTP.erf  10.0.0.55\n");
		return 0;
	}
	int s_server = 0;
	char *tn_ip = argv[2];
	int port = 5671;

	/* Build server infos */
	struct sockaddr_in s_to;
	memset(&s_to, 0, sizeof(s_to));
	s_to.sin_family = AF_INET;
	s_to.sin_port = htons(port);
	if (inet_aton(tn_ip, &s_to.sin_addr) == 0) { // Create datagram with server IP and port
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	if (CONNECT_TO_SERVER)
		if ((s_server = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
			printf("socket error\n");
			exit(0);
		}


	char **key_data = read_keys();

	/* Setup instream */
	struct instream_t instream;
	char* filename_in = argv[1];
	int err;
	struct stats_t stats;
	stats.records_out = 0;
	struct rec_list cap_list;

	init_cap_list(&cap_list);
	err = init_instream(&instream, filename_in);

	/* Process dag records until forced to exit */
	process(&instream, 4, &stats, &cap_list, tn_ip, s_server, s_to, key_data);

	destroy_instream(&instream);
	free(cap_list.recs);

	return 0;
}
