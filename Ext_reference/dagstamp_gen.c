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

/* Building not governed by a makefile: refer to RADrepo includes explicitly */
#include "../radclock/proto_ntp.h"
#include "../radclock/ntp_auth.h"
#include "ext_ref.h"

#define PORT_NTP    123

/* Internals */
#define STRSIZE    512
#define BUFSIZE_IN    25000000	/* 25 MB */

/* Masks for bit positions in ERF flags byte.*/
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

/* Hacks that should become commandline options, or integrated into verbose */
#define CONNECT_TO_SERVER 1     /* suppress sending part of code */
#define VERB_INFO 1             /* more verb for unusual events */
#define VERB_MAX 1              /* max verb for troubleshooting */



/* Override ntp_auth.c 's use of "verbose()" to get local console print. */
void verbose(int facility, const char* format, ...)
{
	va_list arglist;
	va_start( arglist, format );
	vprintf( format, arglist );
	va_end( arglist );
}

/* dagstamp data structure passed to the TrustNode for perfstamp matching. */
//struct dag_cap { }         // see ext_ref.h

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



/* Convert from DAG timestamp format to long double */
static long double
ts_DAGtoLD(unsigned long long dag)
{
	unsigned long s, ns;

	/* First convert DAG to (n,ns) timespec-like pair */
	s = (dag>>32) & 0xFFFFFFFFULL;
	dag = ((dag & 0xFFFFFFFFULL)*1000*1000*1000);
	dag += (dag & 0x80000000ULL)<<1;
	ns = dag>>32;
	if (ns >= 1000000000) {
		ns -= 1000000000;
		s += 1;
	}
	/* Convert pair to seconds as a long double */
	return (s + (long double)(ns * 1e-9));
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
				if (VERB_MAX) printf( "NTP client authentication SUCCESS\n");
				auth_pass = 1;
			} else
				printf("NTP client authentication FAILURE\n");

			*auth_bytes = 24;
		} else
			printf("NTP client authentication request invalid key_id %d pkt_size:(%d %lu)\n",
			    *key_id, packet_size, sizeof(struct ntp_pkt));
	} else
		if ( packet_size <= LEN_PKT_NOMAC ) {
			if (VERB_INFO) printf("NTP Non auth message\n");
			auth_pass = 1;
		} else
			if ( !key_data )
				printf("NTP client authentication request when this server has no keys\n");

	return auth_pass;
}



/*
 * Does packet Insertion and in-situ matching in list buffer,
 * then sending of sucessfully matched dagstamps to the TN.
 * It is assumed that (IP,port) filtering is already applied, catching NTP
 * pkt exchange with the TN as the client, to any server.
 */
static long int
match_and_send(struct rec_list *list, char *tn_ip, int socket_desc, struct sockaddr_in tn_saddr,
    char **key_data, long double ts, struct ip *hdr_ip, struct ntp_pkt *ntp, int isSrc, int UDPpktsize)
{
	int ret, auth_bytes = 0;
	struct dag_ntp_record *thisrec;

	int key_id = -1;
	if (check_signature(UDPpktsize, ntp, key_data, &auth_bytes, &key_id))
	{
		if (VERB_MAX) printf("  passed auth check\n");
		if (isSrc) {
			thisrec = &list->recs[list->write_id];  // current rec
			memcpy(&thisrec->IPout, &hdr_ip->ip_dst, sizeof(thisrec->IPout));
			thisrec->key_id = key_id;
			if (VERB_MAX) printf("   request pkt with key_id: %d inserted at %d \n", key_id, list->write_id);
			thisrec->cap.Tout = ts;
			thisrec->cap.stampid = ntp->xmt;
			list->write_id = (list->write_id + 1) % list->size;
		} else {
			if (VERB_MAX) printf("   response pkt: looking for match with key = %d\n", thisrec->key_id);

			/* Set search order  (in each case, i = attempt count)
			 *   Original:  will find earliest first, but v-inefficient search
			 *   Efficient: backward from last insertion, finds match v-fast, but MAy not be 1st
			 */
			int match_id = -1;
//			for (int i = 0; i < list->size && match_id == -1; i++) {      // Original
//				thisrec = &list->recs[i];                                 // Original
			for (int i = 0; i < list->write_id && match_id == -1; i++) {  // Efficient
				thisrec = &list->recs[list->write_id - 1 - i];            // Efficient

				if (memcmp(&ntp->org, &thisrec->cap.stampid, sizeof(ntp->org)) == 0) {
					if (VERB_MAX) printf("    > stampid match\n");
					if (key_id == thisrec->key_id) {
						if (VERB_MAX) printf("    >> key_id match\n");
						if (memcmp(&hdr_ip->ip_src, &thisrec->IPout, sizeof(hdr_ip->ip_src)) == 0) {
							match_id = i;	// record the first match (will exit loop)
							if (VERB_MAX) printf("    >>> server IP match\n");
						} else
							printf("   matched pkt with server IP change: sent to %s, replied with %s\n",
								inet_ntoa(thisrec->IPout), inet_ntoa(hdr_ip->ip_src));
					}
				}
			}

			/* thisrec is a match, complete remaining fields and send rec */
			if (match_id != -1) {
				thisrec->cap.Tin = ts;
				memcpy(&thisrec->cap.ip, &hdr_ip->ip_src, sizeof(thisrec->cap.ip));

				if (VERB_INFO) printf("    Matched packet for %s. Auth %d\n",
					inet_ntoa(thisrec->cap.ip), key_id);
				if (CONNECT_TO_SERVER)
					ret = sendto(socket_desc,
						(char *)&thisrec->cap, sizeof(struct dag_cap), 0,
						(struct sockaddr *) &tn_saddr, sizeof(struct sockaddr_in));

				/* Match buffer diagnostics */
				if (VERB_MAX) printf("%d: (%.09Lf) %d\n\n", match_id, ts, UDPpktsize);

				/* Eliminate risk of re-matching on this buffer entry */
				memset(&thisrec->cap.stampid, 0, sizeof(ntp->org));

			} else {
				uint64_t id;    // match RADclock's convention for recording stampid
				id = ((uint64_t) ntohl(ntp->org.l_int)) << 32;
				id |= (uint64_t) ntohl(ntp->org.l_fra);
				printf("    Unable to find packet matching stampid %llu\n",
					(unsigned long long) id);
			}
		}
	}

	return ret;
}



/*
 * Reads ERF headers and payloads from an input reader, applies filtering,
 * Packets that pass are send to  match_and_send .
 * Filtering is needed even if the DAG capture is already performing it, for
 * robustness and flexibility.
 * TODO:  capture matching stats, deal with error codes.
 */
static void
process( struct instream_t *istream, struct stats_t *stats,
    struct rec_list *list, char *tn_ip, int socket_desc, struct sockaddr_in tn_saddr, char **key_data)
{
	/* Frame oriented */
	dag_record_t *header = NULL;
	void *payload = NULL;
	struct stat st;
	int length;

	/* Packet extraction oriented */
	struct ether_header *hdr_eth;
	struct ip *hdr_ip;
	struct udphdr *hdr_udp;
	struct ntp_pkt *ntp;
	int sport, dport, isSrc, UDPpktsize;

	/* Get the next frame and process it indefinitely */
	while(1) {
		length = lseek(istream->fd, 0, SEEK_CUR);

		/* Get the next frame from the instream */
		if (get_next_erf_header(istream, stats, &header, &payload) == NULL) {
			// fprintf(stderr, "get_next_erf_header returned NULL\n");
			sleep(1);    // no frames left, wait for more

			lseek(istream->fd, length, SEEK_SET);

			if (stat(istream->filename, &st) < 0) {
				fprintf(stderr, "Stat failed on instream: %s\n", strerror(errno));
				return;
			}
			// Why refresh/check filesize here?
			istream->filesize = (long long int)st.st_size;

			continue;    // no frame to process, try again
		}
		length = lseek(istream->fd, 0, SEEK_CUR);

		stats->records_in++;
		stats->bytes_in += ntohs(header->rlen);

		/* Extract all pkt headers Eth[IP[UDP[NTP]]] from DAG frame payload */
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
			isSrc = 1;  // outgoing pkt send from the TN
		else
			isSrc = 0;  // incoming pkt destined for the TN
		if (VERB_MAX) printf("Found %s (%d)-> %s (%d)  isSRc = %d\n", src, sport, dst, dport, isSrc);


		/* Packet filtering and processing */
		if ((sport == PORT_NTP && strcmp(dst, tn_ip) == 0) ||
			(dport == PORT_NTP && isSrc) )
		{
			if (VERB_MAX) printf("  passed (IP,port) filter\n");
			UDPpktsize = ntohs(hdr_udp->uh_ulen);    // for auth checking
			match_and_send(list, tn_ip, socket_desc, tn_saddr, key_data,
			    ts_DAGtoLD(header->ts), hdr_ip, ntp, isSrc, UDPpktsize);
	//		stats->filter_pass++;    // possible counter name

		} else {
			//if (VERB_MAX) printf("  failed (IP,port) filter\n");
	//		stats->filter_fail++;    // possible counter name
		}

		stats->records_out++;    // dont want this if match fails, need new stats

	}
}


void init_rec_list(struct rec_list *list)
{
	list->recs = calloc(PKT_LIST_SIZE, sizeof(struct dag_ntp_record));
	list->size = PKT_LIST_SIZE;
	list->write_id = 0;
}


int
main(int argc, char **argv)
{
	/* TODO:  extend and improve arg processing */
	if (argc < 3) {
		printf("Usage:  dagstamp_gen  cap.erf  TN_IP\n");
		printf("Eg: ./dagstamp_gen  /tmp/trustnode_NTP.erf  10.0.0.55\n");
		return 0;
	}
	int err;

	/* TrustNode communication related */
	int socket_desc = 0;
	struct sockaddr_in tn_saddr;
	char *tn_ip = argv[2];

	/* Prepare for sending to the TrustNode.*/
	if (CONNECT_TO_SERVER) {   // skip if just testing
		/* Open socket (recall this assigns a (hostIP, randomport) address */
		if ((socket_desc = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
			perror("socket");
			printf("socket error\n");
			exit(0);
		}
		/* Fill socket address info for sending to TrustNode */
		memset((char *) &tn_saddr, 0, sizeof(struct sockaddr_in));
		tn_saddr.sin_family = AF_INET;
		tn_saddr.sin_port = htons(DAG_PORT);
		if (inet_aton(tn_ip, &tn_saddr.sin_addr) == 0) {
			fprintf(stderr, "inet_aton() failed, mistyped IP argument?\n");
			exit(1);
		}
	}

	/* Load NTC auth keys */
	char **key_data = read_keys();

	/* Setup instream to read from the DAG capture */
	char* filename_in = argv[1];
	struct instream_t instream;
	struct stats_t stats;
	stats.records_out = 0;

	struct rec_list list;
	init_rec_list(&list);

	err = init_instream(&instream, filename_in);

	/* Process dag records until forced to exit */
	if (!err)
		process(&instream, &stats, &list, tn_ip, socket_desc, tn_saddr, key_data);

	/* Shutdown */
	destroy_instream(&instream);
	free(list.recs);
	if (socket_desc >= 0)
		close(socket_desc);

	return err;
}
