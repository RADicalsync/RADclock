/*
 * Copyright (C) 2006 The RADclock Project (see AUTHORS file)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in_systm.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "proto_ntp.h"
#include "misc.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "ntohll.h"
#include "create_stamp.h"
#include "jdebug.h"


// TODO: we should not have to redefine this
# ifndef useconds_t
typedef uint32_t useconds_t;
# endif

struct stq_elt {
	struct stamp_t stamp;
	struct stq_elt *prev;
	struct stq_elt *next;
};

struct stamp_queue {
	struct stq_elt *start;
	struct stq_elt *end;
	int size;
};

/* To prevent allocating heaps of memory if stamps are not paired in queue */
#define MAX_STQ_SIZE	20



radpcap_packet_t *
create_radpcap_packet()
{
	radpcap_packet_t *pkt;

	JDEBUG

	pkt = (radpcap_packet_t*) malloc(sizeof(radpcap_packet_t));
	JDEBUG_MEMORY(JDBG_MALLOC, pkt);

	pkt->buffer 	= (void *) malloc(RADPCAP_PACKET_BUFSIZE);
	JDEBUG_MEMORY(JDBG_MALLOC, pkt->buffer);

	pkt->header = NULL;
	pkt->payload = NULL;
	pkt->type = 0;
	pkt->size = 0;

	return (pkt);
}


void
destroy_radpcap_packet(radpcap_packet_t *packet)
{
	JDEBUG

	packet->header = NULL;
	packet->payload = NULL;

	if (packet->buffer) {
		JDEBUG_MEMORY(JDBG_FREE, packet->buffer);
		free(packet->buffer);
	}
	packet->buffer = NULL;

	JDEBUG_MEMORY(JDBG_FREE, packet);
	free(packet);
	packet = NULL;
}


int
check_ipv4(struct ip *iph, int remaining)
{
	if (iph->ip_v != 4) {
		verbose(LOG_WARNING, "Failed to parse IPv4 packet");
		return (1);
	}

	if ((iph->ip_off & 0xff1f) != 0) {
		verbose(LOG_WARNING, "Fragmented IP packet");
		return (1);
	}

	if (iph->ip_p != 17) {
		verbose(LOG_WARNING, "Not a UDP packet");
		return (1);
	}

	if (remaining < (iph->ip_hl * 4U)) {
		verbose(LOG_WARNING, "Broken IP packet");
		return (1);
	}

	return (0);
}


// TODO should there be more to do?
int
check_ipv6(struct ip6_hdr *ip6h, int remaining)
{
	if (ip6h->ip6_nxt != 17) {
		verbose(LOG_ERR, "IPv6 packet with extensions no supported");
		return (1);
	}

	if (remaining < 40) {
		verbose(LOG_WARNING, "Broken IP packet");
		return (1);
	}

	return (0);
}


/*
 * Get the IP payload from the radpcap_packet_t packet.  Here also (in addition
 * to get_vcount) we handle backward compatibility since we changed the way the
 * vcount and the link layer header are managed.
 *
 * We handle 3 formats (historical order):
 * 1 - [pcap][ether][IP] : oldest format (vcount in pcap header timeval)
 * 2 - [pcap][sll][ether][IP] : libtrace-3.0-beta3 format, vcount is in sll header
 * 3 - [pcap][sll][IP] : remove link layer header, no libtrace, vcount in sll header
 * In live capture, the ssl header MUST be inserted before calling this function
 * Ideally, we would like to get rid of formats 1 and 2 to simplify the code.
 */
// TODO and for non NTP packets? (ie 1588)
// FIXME: the ip pointer is dirty and will break with IPv6 packets
int
get_valid_ntp_payload(radpcap_packet_t *packet, struct ntp_pkt **ntp,
		struct sockaddr_storage *ss_src, struct sockaddr_storage *ss_dst,
		int *ttl)
{
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct ip *iph;
	struct ip6_hdr *ip6h;
	struct udphdr *udph;
	linux_sll_header_t *sllh;
	uint16_t proto;
	int remaining;
	int err;

	JDEBUG

	remaining = ((struct pcap_pkthdr *)packet->header)->caplen;

	switch (packet->type) {

	/*
	 * This is format #1, skip 14 bytes ethernet header. Only NTP packets ever
	 * captured in this format are IPv4
	 */
	case DLT_EN10MB:
		iph = (struct ip *)(packet->payload + sizeof(struct ether_header));
		remaining -= sizeof(struct ether_header);
		ip6h = NULL;
		break;

	/*
	 * This is format #2 and #3. Here we take advantage of a bug in bytes order in
	 * libtrace-3.0-beta3 to identify the formats.
	 * - if sllh->hatype = ARPHRD_ETHER (0x0001), we have format 3.
	 * - if sllh->hatype is 256 (0x0100) it's a libtrace format.
	*/
	case DLT_LINUX_SLL:
		sllh = (linux_sll_header_t*) packet->payload;

		/* Format #2 */
		if (ntohs(sllh->hatype) != 0x0001) {
			iph = (struct ip *)(packet->payload + sizeof(struct ether_header) +
					sizeof(linux_sll_header_t));
			remaining -= sizeof(struct ether_header);
			ip6h = NULL;
			break;
		}

		/* This is format 3 */
		proto = ntohs(sllh->protocol);
		switch (proto) {

		/* IPv4 */
		case (ETHERTYPE_IP):
			ip6h = NULL;
			iph = (struct ip *)(packet->payload + sizeof(linux_sll_header_t));
			remaining -= sizeof(linux_sll_header_t);

			err = check_ipv4(iph, remaining);
			if (err)
				return (1);

			ss_src->ss_family = AF_INET;
			ss_dst->ss_family = AF_INET;
			sin = (struct sockaddr_in *)ss_src;
			sin->sin_addr = iph->ip_src;
			sin = (struct sockaddr_in *)ss_dst;
			sin->sin_addr = iph->ip_dst;
			*ttl = iph->ip_ttl;

			udph = (struct udphdr *)((char *)iph + (iph->ip_hl * 4));
			remaining -= sizeof(struct ip);
			break;

		/* IPv6 */
		case (ETHERTYPE_IPV6):
			iph = NULL;
			ip6h = (struct ip6_hdr *)(packet->payload + sizeof(linux_sll_header_t));
			remaining -= sizeof(linux_sll_header_t);

			err = check_ipv6(ip6h, remaining);
			if (err)
				return (1);

			ss_src->ss_family = AF_INET6;
			ss_dst->ss_family = AF_INET6;
			sin6 = (struct sockaddr_in6 *)ss_src;
			sin6->sin6_addr = ip6h->ip6_src;
			sin6 = (struct sockaddr_in6 *)ss_dst;
			sin6->sin6_addr = ip6h->ip6_dst;
			*ttl = ip6h->ip6_hops;

			udph = (struct udphdr *)((char *)ip6h + sizeof(struct ip6_hdr));
			remaining -= sizeof(struct ip6_hdr);
			break;

		/* IEEE 1588 over Ethernet */
		case (0x88F7):
			verbose(LOG_ERR, "1588 over Ethernet not implemented");
			return (1);

		default:
			verbose(LOG_ERR, "Unsupported protocol in SLL header %u", proto);
			return(1);
		}
		break;

	default:
		verbose(LOG_ERR, "MAC layer type not supported yet.");
		return (1);
		break;
	}

	if (remaining < sizeof(struct udphdr)) {
		verbose(LOG_WARNING, "Broken UDP datagram");
		return (1);
	}

	*ntp = (struct ntp_pkt *)((char *)udph + sizeof(struct udphdr));
	remaining -= sizeof(struct udphdr);

	/*
	 * Make sure the NTP packet is not truncated. A normal NTP packet is at
	 * least 48 bytes long, but a control or private request is as small as 12
	 * bytes.
	 */
	if (remaining < 12) {
		verbose(LOG_WARNING, "NTP packet truncated, payload is %d bytes "
			"instead of at least 12 bytes", remaining);
		return (1);
	}

	return (0);
}


/*
 * Retrieve the vcount value stored in the pcap header timestamp field.
 * This function is here for backward compatibility and may disappear one day,
 * especially because the naming convention is confusing. The ethernet frame is
 * used only for distinguishing the first raw file format.
 */
int
get_vcount_from_etherframe(radpcap_packet_t *packet, vcounter_t *vcount)
{
	JDEBUG
	//verbose(LOG_NOTICE, "  ** Inside get_vcount_from_etherframe **");
	
	if (packet->size < sizeof(struct pcap_pkthdr)) {
		verbose(LOG_ERR, "No PCAP header found.");
		return (-1);
	}

	// TODO : Endianness !!!!
	/* This is the oldest raw file format where the vcount was stored into the
	 * timestamp field of the pcap header.
	 * tv_sec holds the left hand of the counter, then put right hand of the
	 * counter into empty RHS of vcount
	 */
	*vcount  = (u_int64_t) (((struct pcap_pkthdr*)packet->header)->ts.tv_sec) << 32;
	*vcount += (u_int32_t) ((struct pcap_pkthdr*)packet->header)->ts.tv_usec;

	return (0);
}

/*
 * Retrieve the vcount value from the address field of the LINUX SLL
 * encapsulation header
 */
int
get_vcount_from_sll(radpcap_packet_t *packet, vcounter_t *vcount)
{
	vcounter_t aligned_vcount;

	JDEBUG
	//verbose(LOG_NOTICE, "  ** Inside get_vcount_from_sll **");

	if (packet->size < sizeof(struct pcap_pkthdr) + sizeof(linux_sll_header_t)) {
		verbose(LOG_ERR, "No PCAP or SLL header found.");
		return (-1);
	}
	
	linux_sll_header_t *hdr = packet->payload;
	if (!hdr) {
		verbose(LOG_ERR, "No SLL header found.");
		return (-1);
	}
	// TODO What does this comment mean?
	/* memcopy to ensure word alignedness and avoid potential sigbus's */
	memcpy(&aligned_vcount, hdr->addr, sizeof(vcounter_t));
	*vcount = ntohll(aligned_vcount);

	return 0;
}



/*
 * Generic function to retrieve the vcount. Depending on the link layer type it
 * calls more specific one. This ensures backward compatibility with older format
 */
int
get_vcount(radpcap_packet_t *packet, vcounter_t *vcount) {

	int ret;

	JDEBUG

	ret = -1;
	switch ( packet->type ) {
	case DLT_EN10MB:
		ret = get_vcount_from_etherframe(packet, vcount);
		break;
	case DLT_LINUX_SLL:
		ret = get_vcount_from_sll(packet, vcount);
		break;
	default:
		verbose(LOG_ERR, "Unsupported MAC layer.");
		break;
	}
	return ret;
}


/* Routines to create and destroy a stamp queue. The queue is anchored
 * to a pointer within the passed argument, so storage for the
 * queue can be allocated and freed here.
 */
void
init_stamp_queue(struct bidir_algodata *algodata)
{
	algodata->q = calloc(1, sizeof(struct stamp_queue));
	algodata->q->start = NULL;
	algodata->q->end = NULL;
	algodata->q->size = 0;
}

void
destroy_stamp_queue(struct bidir_algodata *algodata)
{
	struct stq_elt *elt;

	elt = algodata->q->end;
	while (algodata->q->size) {
		if (elt->prev == NULL)
			break;
		elt = elt->prev;
		free(elt);
		algodata->q->size--;
	}
	free(algodata->q->end);
	free(algodata->q);
	algodata->q = NULL;
}


/*
 * Insert a client or server halfstamp into the stamp queue, perform duplicate
 * halfstamp detection and client request<-->server reply matching, and trim 
 * excessive queue if needed. 
 * The queue allows many client requests to be buffered while waiting for their
 * replies, and can cope with both reordering, and loss, of both client and
 * server packets.
 *
 * If a matching halfstamp is found in the queue then the existing queue entry
 * will be completed in situ, else a new halfstamp will be inserted at head.
 * Duplicate halfstamps are not a problem, they are simply dropped with warning.
 *
 * The stamp->id field as an (assumed) unique key. Since this is in fact a 
 * timestamp field, inserted c-halfstamps will very likely be in id-order as 
 * well as in true temporal order, but this is NOT assumed or used. A small 
 * exception is in the queue trimming, which drops the tail element.
 * In the very unlikely event that the tail element is not in fact the oldest
 * (most stale), this will not create any problems beyond the loss of a stamp.
 * Other kinds of halfstamp cleaning (based on temporal ordering, not id) are
 * dealt with in get_fullstamp_from_queue_andclean.
 *
 * Stamps from different servers are treated in the same way.
 *
 * The prev direction is toward the queue head/start.
 * Return codes: err = 0 : halfstamp insertion resulted in fullstamp
 *               err = 1 : insertion but no fullstamp, or halfstamp dropped
 */
int
insertandmatch_halfstamp(struct stamp_queue *q, struct stamp_t *new, int mode)
{
	struct stq_elt *qel;
	struct stamp_t *stamp;
	int foundhalfstamptofill;

	if ((mode != MODE_CLIENT) && (mode != MODE_SERVER)) {
		verbose(LOG_ERR, "Unsupported stamp matching mode: %d", mode);
		return (-1);
	}

	/* Scan queue for matching id, determine if duplicate, match, or new */
	foundhalfstamptofill = 0;
	qel = q->start;
	while (qel != NULL) {
		stamp = &qel->stamp;

		if (stamp->id == new->id) {		// this id in the queue already
			switch (mode) {
				case MODE_CLIENT:
					if (BST(stamp)->Ta != 0) {
						verbose(LOG_WARNING, "Dropping duplicate NTP client request.");
						return (1);
					}
					break;

				case MODE_SERVER:
					if (BST(stamp)->Tf != 0) {
						verbose(LOG_WARNING, "Dropping duplicate NTP server response.");
						return (1);
					}
					break;
			}

			foundhalfstamptofill = 1;		// qel will point to stamp to fill
			break;
		}
		qel = qel->next;
	}

	/* If this id is new, then create and insert blank queue element. */
	if (!foundhalfstamptofill)
	{
		/* If queue too long, trim off last element */
		if (q->size == MAX_STQ_SIZE) {
			verbose(LOG_WARNING, "Stamp matching queue has hit max size.");
			q->end = q->end->prev;
			free(q->end->next);
			q->end->next = NULL;
			q->size--;
		}

      /* Create blank element, *qel, and insert at head */
		qel = calloc(1, sizeof(struct stq_elt));
		qel->prev = NULL;
		qel->next = q->start;		// will be NULL as reqd if queue empty
		if (q->start == NULL)		// queue was empty
			q->end = qel;
		else
			q->start->prev = qel;
		q->start = qel;
		q->size++;

		/* Fill fields common to both halfstamps */
		switch (mode) {
			case MODE_CLIENT:
			case MODE_SERVER:
			qel->stamp.type = STAMP_NTP;
		}
		qel->stamp.id = new->id;
	}

	/* Selectively copy content of new halfstamp into stamp to fill (*qel). */
	stamp = &qel->stamp;
	switch (mode) {
		case MODE_CLIENT:
			strncpy(stamp->server_ipaddr, new->server_ipaddr, 16);
			BST(stamp)->Ta = BST(new)->Ta;
			break;
		case MODE_SERVER:
			stamp->ttl = new->ttl;
			stamp->refid = new->refid;
			stamp->stratum = new->stratum;
			stamp->LI = new->LI;
			stamp->rootdelay = new->rootdelay;
			stamp->rootdispersion = new->rootdispersion;
			BST(stamp)->Tb = BST(new)->Tb;
			BST(stamp)->Te = BST(new)->Te;
			BST(stamp)->Tf = BST(new)->Tf;
	}

	/* Print out queue from head to tail (youngest at top of printout). */
	if (VERB_LEVEL>1) {
		qel = q->start;

		while (qel != NULL) {
			stamp = &qel->stamp;
			if (stamp->type == STAMP_NTP) {
				verbose(VERB_DEBUG, "  stamp queue dump: [%llu]   %llu %llu %.6Lf %.6Lf %s",
				(long long unsigned) stamp->id,
				(long long unsigned) BST(stamp)->Ta, (long long unsigned) BST(stamp)->Tf,
				BST(stamp)->Tb, BST(stamp)->Te,
				stamp->server_ipaddr);
			}

			qel = qel->next;
		}
	}

	if (foundhalfstamptofill)
		return (0);
	else
		return (1);
}


/* Returns {0,1} if {identical,different} */
int
compare_sockaddr_storage(struct sockaddr_storage *first,
		struct sockaddr_storage *second)
{
	if (first->ss_family != second->ss_family)
		return (1);

	if (first->ss_family == AF_INET) {
		if (memcmp(&(((struct sockaddr_in *) first)->sin_addr),
				&(((struct sockaddr_in *) second)->sin_addr),
				sizeof(struct in_addr)) == 0)
			return (0);
	}
	// address family is AF_INET6
	else {
		if (memcmp(&(((struct sockaddr_in6 *) first)->sin6_addr),
				&(((struct sockaddr_in6 *) second)->sin6_addr),
				sizeof(struct in6_addr)) == 0)
			return (0);
	}

	return (1);
}

int
is_loopback_sockaddr_storage(struct sockaddr_storage *ss)
{
	struct in_addr *addr;
	struct in6_addr *addr6;

	if (ss->ss_family == AF_INET) {
		addr = &((struct sockaddr_in *)ss)->sin_addr;
		if (addr->s_addr == htonl(INADDR_LOOPBACK))
			return (1);
	} else {
		addr6 = &((struct sockaddr_in6 *)ss)->sin6_addr;
		if (IN6_IS_ADDR_LOOPBACK(addr6))
			return (1);
	}

	return (0);
// Not sure where to put this at the moment or a cleaner way
#define NOXENSUPPORT 0x01

}

/*
 * Check the client's request.
 * The radclock may serve NTP clients over the network. The BPF filter may not
 * be tight enough either. Make sure that requests from clients are discarded.
 */
int
bad_packet_client(struct ntp_pkt *ntp, struct sockaddr_storage *ss_if,
		struct sockaddr_storage *ss_dst, struct timeref_stats *stats)
{
	int err;

	err = compare_sockaddr_storage(ss_if, ss_dst);
	if (err == 0) {
		if (!is_loopback_sockaddr_storage(ss_dst)) { 
			verbose(LOG_WARNING, "Destination address in client packet. "
					"Check the capture filter.");
			return (1);
		}
	}
	return (0);
}

/*
 * Check the server's reply.
 * Make sure that this is not one of our reply to our NTP clients.
 * Make sure the leap second indicator is ok.
 * Make sure the server's stratum is not insane.
 */
int
bad_packet_server(struct ntp_pkt *ntp, struct sockaddr_storage *ss_if,
		struct sockaddr_storage *ss_src, struct timeref_stats *stats)
{
	int err;

	err = compare_sockaddr_storage(ss_if, ss_src);
	if (err == 0) {
		if (!is_loopback_sockaddr_storage(ss_src)) { 
			verbose(LOG_WARNING, "Source address in server packet. "
					"Check the capture filter.");
			return (1);
		}
	}

	/* Check if the server is a RADserver */
//	static int c = 0;
//	u_char NTPv;
//	if ( c<1 && (NTPv = PKT_VERSION(ntp->li_vn_mode)) != NTP_VERSION) {
//		verbose(LOG_WARNING, "NTP version of %d non-standard", NTPv);
//		c++;
//	}

	/* If the server is unsynchronised we skip this packet */
	if (PKT_LEAP(ntp->li_vn_mode) == LEAP_NOTINSYNC) {
		verbose(LOG_WARNING, "NTP server has LEAP_NOTINSYNC, packet ignored.");
		stats->badqual_count++;
		return (1);
	}

	/* Check if the server clock is synchroninsed or not */
	if (ntp->stratum == STRATUM_UNSPEC) {
		verbose(LOG_WARNING, "Stratum unspecified, server packet ignored.");
		return (1);
	}

	return (0);
}

/*
 * Create a stamp structure, fill it with client side information and pass it
 * for insertion in the stamp queue.  The server address used by the client
 * to send the request is essentially a serverID: record it here.
 */
int
push_client_halfstamp(struct stamp_queue *q, struct ntp_pkt *ntp,
	vcounter_t *vcount, struct sockaddr_storage *ss_dst)
{
	struct stamp_t stamp;

	JDEBUG

	stamp.id = ((uint64_t) ntohl(ntp->xmt.l_int)) << 32;
	stamp.id |= (uint64_t) ntohl(ntp->xmt.l_fra);
	stamp.type = STAMP_NTP;
	
	if (ss_dst->ss_family == AF_INET)
		inet_ntop(ss_dst->ss_family,
			&((struct sockaddr_in *)ss_dst)->sin_addr, stamp.server_ipaddr,
			INET6_ADDRSTRLEN);
	else
		inet_ntop(ss_dst->ss_family,
			&((struct sockaddr_in6 *)ss_dst)->sin6_addr, stamp.server_ipaddr,
			INET6_ADDRSTRLEN);
			
	BST(&stamp)->Ta = *vcount;

	verbose(VERB_DEBUG, "Stamp queue: inserting client stamp->id: %llu",
			(long long unsigned)stamp.id);

	return (insertandmatch_halfstamp(q, &stamp, MODE_CLIENT));
}


/*
 * Create a stamp structure, fill it with server side information and pass it
 * for insertion in the stamp queue.
 */
int
push_server_halfstamp(struct stamp_queue *q, struct ntp_pkt *ntp,
		vcounter_t *vcount, int *ttl)
{
	struct stamp_t stamp;

	JDEBUG

	stamp.type = STAMP_NTP;
	stamp.id = ((uint64_t) ntohl(ntp->org.l_int)) << 32;
	stamp.id |= (uint64_t) ntohl(ntp->org.l_fra);

	stamp.ttl = *ttl;
	stamp.refid = ntohl(ntp->refid);
	stamp.stratum = ntp->stratum;
	stamp.LI = PKT_LEAP(ntp->li_vn_mode);
	stamp.rootdelay      = ntohl(ntp->rootdelay) / 65536.;
	stamp.rootdispersion = ntohl(ntp->rootdispersion) / 65536.;
	BST(&stamp)->Tb = NTPtime_to_UTCld(ntp->rec);
	BST(&stamp)->Te = NTPtime_to_UTCld(ntp->xmt);
	BST(&stamp)->Tf = *vcount;

	verbose(VERB_DEBUG, "Stamp queue: inserting server stamp->id: %llu",
			(long long unsigned)stamp.id);

	return (insertandmatch_halfstamp(q, &stamp, MODE_SERVER));
}


/*
 * Check that packet captured is a sane input, independent from its direction
 * (client request or server reply). Pass it to subroutines for additional
 * checks and insertion/matching in the stamp queue.
 */
int
update_stamp_queue(struct stamp_queue *q, radpcap_packet_t *packet,
		struct timeref_stats *stats)
{
	struct ntp_pkt *ntp;
	struct sockaddr_storage ss_src, ss_dst, *ss;
	vcounter_t vcount;
	int ttl;
	int err;
	char ipaddr[INET6_ADDRSTRLEN];

	JDEBUG

	/* Retrieve vcount from link layer header, if this fails, things are bad */
	if (get_vcount(packet, &vcount)) {
		verbose(LOG_ERR, "Error getting raw vcounter from link layer.\n");
		return (-1);
	}

	err = get_valid_ntp_payload(packet, &ntp, &ss_src, &ss_dst, &ttl);
	if (err) {
		verbose(LOG_WARNING, "Not an NTP packet.");
		return (1);
	}

	ss = &packet->ss_if;		// host's interface address
	err = 0;
	switch (PKT_MODE(ntp->li_vn_mode)) {
	case MODE_BROADCAST:
		ss = &ss_src;
		if (ss->ss_family == AF_INET)
			inet_ntop(ss->ss_family,
				&((struct sockaddr_in *)&ss)->sin_addr, ipaddr, INET6_ADDRSTRLEN);
		else
			inet_ntop(ss->ss_family,
				&((struct sockaddr_in6 *)ss)->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
		verbose(VERB_DEBUG, "Received NTP broadcast packet from %s (Silent discard)",
				ipaddr);
		break;

	case MODE_CLIENT:		// here ss_dst is the server address TRIGGER sent to
		err = bad_packet_client(ntp, ss, &ss_dst, stats);
		if (err)
			break;
		err = push_client_halfstamp(q, ntp, &vcount, &ss_dst);
		break;

	case MODE_SERVER:		// here ss_src is the address the server responded with
		err = bad_packet_server(ntp, ss, &ss_src, stats);
		if (err)
			break;
		err = push_server_halfstamp(q, ntp, &vcount, &ttl);
		break;

	default:
		verbose(VERB_DEBUG, "Missed pkt due to invalid mode: mode = %d",
			PKT_MODE(ntp->li_vn_mode));
		err = 1;
		break;
	}

	return (err);
}


/*
 * Remove the oldest full stamp from the queue, and clean dangerous halfstamps
 * (should be exactly one fullstamp, but code allows for 0 or more than 1).
 *
 * Principle is that fullstamps should never be removed out of order.  Here 
 * stamp order is defined by the outgoing raw stamp Ta. This implies that client
 * halfstamps older than the fullstamp should be cleaned out, since should they
 * ever be filled, they would be out of order when removed. This also prevents
 * c-halfstamps which are never matched (missing server replies) from bloating
 * the queue permanently.
 * An alternative ordering based on Tf is possible, but has more disadvantages.
 * Note:  older = in queue longer = smaller value.
 * 
 * Server halfstamps shouldn't normally occur but sometimes do, for example if
 * a c-halfstamp is cleaned, and its matching response arrives later. This can
 * arise when a retry response returns before the (in fact not lost) original
 * response. They are cleaned safely in reasonable time by comparison on Tf
 * against the fullstamp, since Ta is not available for them.
 *
 * This routine makes NO assumptions on element ordering within the queue, and
 * in particular uses the stamp.id field (see insertandmatch_halfstamp)
 * only to match two halfstamps into a fullstamp, not to establish stamp order).
 *
 * To support stamps from multiple servers coexisting in the queue, c-halfstamp
 * cleanout is only performed when the serverID matches that of the full stamp.
 * This check is dropped for s-halfstamps, as they shouldn't be there anyway.
 *
 * The prev direction is toward the queue head/start.
 * Error codes:  err = 0 : success, fullstamp found and returned
 *               err = 1 : failure, couldn't find fullstamp, nothing returned
 */
int
get_fullstamp_from_queue_andclean(struct stamp_queue *q, struct stamp_t *stamp)
{
	struct stq_elt *qel, *qelcopy;
	struct stamp_t *st, *full_st;
	vcounter_t	full_time=0;			// used to record stamp order
	vcounter_t	Tc;
	int startsize;
	int c_halfstamp, s_halfstamp, fullstamp, dangerous;
	int c_older, s_older;				// records if halfstamps older than full one

	JDEBUG

	if (q->size == 0) {
		verbose(LOG_WARNING, "Stamp matching queue empty, no stamp returned");
		return (1);
	} else
	  	startsize = q->size;

   /* Scan queue from tail to head to find and pass back oldest full stamp */
	qel = q->end;
	while (qel != NULL) {
		st = &qel->stamp;
		Tc = BST(st)->Ta;			// client side timestamp
		fullstamp = (Tc != 0) && (BST(st)->Tf != 0);
		if (fullstamp) {
			if (full_time == 0 || Tc < full_time) {		// oldest so far
				full_st = st;
				full_time = Tc;
		   }
		}
		qel = qel->prev;
	}

	if (full_time > 0)
		memcpy(stamp, full_st, sizeof(struct stamp_t));
	else {
		verbose(LOG_WARNING, "Did not find any full stamp in stamp queue");
		return (1);
	}


   /* Scan queue, removing chosen fullstamp, and dangerous halfstamps */
	qel = q->end;
	while (qel != NULL) {
		st = &qel->stamp;

		/* Determine the scenario posed by this stamp */
		c_halfstamp = (BST(st)->Ta != 0) && (BST(st)->Tf == 0);
		s_halfstamp = (BST(st)->Ta == 0) && (BST(st)->Tf != 0);
		if (s_halfstamp)
			verbose(LOG_WARNING, "Found server halfstamp in stamp queue");
		c_older = c_halfstamp && BST(st)->Ta < full_time;
		s_older = s_halfstamp && BST(st)->Tf < BST(full_st)->Tf;
		dangerous = s_older ||
						(c_older && strcmp(st->server_ipaddr,full_st->server_ipaddr) == 0);
		if (dangerous)
			verbose(VERB_DEBUG, "Clearing out dangerous halfstamp");

		if (st == full_st || dangerous)
	   {	/* Remove *qel from queue, reset qel to continue loop */
			if (qel==q->end) {
				if (qel==q->start) {			// only 1 stamp in queue
					free(qel);
					qel = NULL;					// reached head, will exit while
					q->start = NULL;
					q->end = NULL;
			   } else {
					qel = qel->prev;			// next target (is the new end)
					free(qel->next);
					qel->next = NULL;
					q->end = qel;
				}
			} else {
				if (qel==q->start) {			// reached head, will exit while
					q->start = qel->next;
					qel->next->prev = NULL;
					free(qel);
					qel = NULL;
			   } else {							// generic case, neither start nor end
					qel->next->prev = qel->prev;
					qel->prev->next = qel->next;
		   		qelcopy = qel;
					qel = qel->prev;			// next target
					free(qelcopy);
				}
			}
			q->size--;
		} else
			qel = qel->prev;
	}
	
	verbose(VERB_DEBUG, "Stamp queue had %d stamps, freed %d, %d left",
		startsize, startsize - q->size, q->size);

	return (0);
}

 
 
/*
 * Retrieve network packet from live or dead pcap device.
 * The stamp queue infrastructure is used to handle out of order packets (for
 * both dead and live input) and other aberrations.
 */
int
get_network_stamp(struct radclock_handle *handle, void *userdata,
	int (*get_packet)(struct radclock_handle *, void *, radpcap_packet_t **),
	struct stamp_t *stamp, struct timeref_stats *stats)
{
	struct stamp_queue *q;
	radpcap_packet_t *packet;
	int attempt, maxattempts;
	int err;

	JDEBUG

	err = 0;
	maxattempts = 20;
	q = ((struct bidir_algodata*)handle->algodata)->q;
	packet = create_radpcap_packet();

	/*
	 * Used to have both live and dead PCAP inputs dealt the same way. But extra
	 * processing to give a chance to out-of-order packets make it too hard to
	 * keep the same path. Cases are decoupled below, the dead input does not
	 * need to sleep when reading PCAP file.
	 */
	switch(handle->run_mode) {

	/* Read packet from pcap tracefile. */
	case RADCLOCK_SYNC_DEAD:
		err = get_packet(handle, userdata, &packet); // 1 = no rbd data or error
		if (err) {
			destroy_radpcap_packet(packet);
			return (-1);
		}
		/* Counts pkts, regardless of content (initialised to 0 in main) */
		stats->ref_count++;

		/*
		 * Convert packet to stamp and push it to the stamp queue, errors are:
		 * -1: Low level / input problem worth stopping, break with error code.
		 *  0: halfstamp match made, fullstamp available, take it and process
		 *     [ do not want to fill stamp queue with the entire trace file ! ]
		 *  1: halfstamp didn't result in match
	 	*/
		err = update_stamp_queue(q, packet, stats);
		switch (err) {
		case -1:
			verbose(LOG_ERR, "Stamp queue error");
			break;
		case  0:
			verbose(VERB_DEBUG, "Inserted packet and found match");
			break;
		case  1:
			verbose(VERB_DEBUG, "Inserted packet but no match");
			break;
		}
		break;


	/* Read packet from raw data queue and insert into stamp queue until find a
	 * fullstamp to return, or until no data left. Cap the maximum number of
	 * pkts inserted before returning to stop PROC taking too many resources.
	 */
	case RADCLOCK_SYNC_LIVE:
	
		for (attempt=maxattempts; attempt>0; attempt--) {
			err = get_packet(handle, userdata, &packet); // 1= no rbd data or error
			if (err) {
//				verbose(VERB_DEBUG, " get_network_stamp: out of data on attempt %d", maxattempts-attempt+1);
				break;
			} else {		// found a packet, process it
				stats->ref_count++;
				/* Convert packet to stamp and push it to the stamp queue */
				err = update_stamp_queue(q, packet, stats);

				/* Error codes as for dead case */
				if (err == -1)
					break;
				if (err == 0) {	// fullstamp available
					verbose(VERB_DEBUG, " get_network_stamp: stamp search success "
					"after %d attempts out of %d", maxattempts-attempt+1, maxattempts);
					break;
				}

				if (attempt == 1)
					verbose(VERB_DEBUG, " get_network_stamp: giving up full stamp "
						"search after %d attempts, though data still available", maxattempts);
			}
		}
		break;

	case RADCLOCK_SYNC_NOTSET:
	default:
		verbose(LOG_ERR, "Run mode not set!");
		err = -1;
		break;
	}
	
	/* Make sure we don't leak memory */
	destroy_radpcap_packet(packet);

	/* Error, something wrong worth killing everything */
	if (err == -1)
		return (-1);

	/* Either no raw data found, or no fullstamp in queue, nothing to do */
	if (err == 1)
		return (1);

	/* At least one full stamp in the queue (err=0), get it. */
	err = get_fullstamp_from_queue_andclean(q, stamp);
	if (err)
		return (1);

	return (0);
}

