/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2006-2007, Thomas Young <tfyoung@gmail.com>
 * All rights reserved.
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

#include "../config.h"
#ifdef WITH_FFKERNEL_LINUX

#include <asm/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <netlink/netlink.h>
#include <netlink/genl/ctrl.h>

#include <errno.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"
#include "logger.h"


/* Definitions also given in kernel file radclock.h */
#define RADCLOCK_NAME "radclock"		// kernel side define in radclock.h

enum {
	RADCLOCK_ATTR_DUMMY,
	RADCLOCK_ATTR_DATA,
	RADCLOCK_ATTR_FIXEDPOINT,
	__RADCLOCK_ATTR_MAX,
};
#define RADCLOCK_ATTR_MAX (__RADCLOCK_ATTR_MAX - 1)
enum {
	RADCLOCK_CMD_UNSPEC,
	RADCLOCK_CMD_GETATTR,
	RADCLOCK_CMD_SETATTR,
	__RADCLOCK_CMD_MAX,
};
#define RADCLOCK_CMD_MAX (__RADCLOCK_CMD_MAX - 1)


static struct nla_policy radclock_attr_policy[RADCLOCK_ATTR_MAX+1] = {
	[RADCLOCK_ATTR_DUMMY]      = { .type = NLA_U16 },
	[RADCLOCK_ATTR_DATA]       = { .minlen = sizeof(struct radclock_data) },
	[RADCLOCK_ATTR_FIXEDPOINT] = { .minlen = sizeof(struct radclock_fixedpoint) },
};


/* Obtain and record the genl ID of the RADclock protocol family */
int
init_kernel_clock(struct radclock *clock)
{
	struct nl_sock *sk;
	int id;

	/* Ask the kernel for the numerical id of the RADclock protocol family */
	sk = nl_socket_alloc();		// allocate a new netlink socket for the request
	if (sk) {
		genl_connect(sk);			// just calls:  nl_connect(sk, NETLINK_GENERIC);
		id = genl_ctrl_resolve(sk, RADCLOCK_NAME);
		nl_socket_free(sk);		// this also closes the socket
	} else {
		logger(RADLOG_ERR, "Cannot allocate netlink socket\n");
		return (1);
	}

	if (id < 0) {
		logger(RADLOG_ERR, "Couldn't get the radclock netlink ID:  %s", nl_geterror(id));
		return (1);
	}

	logger(RADLOG_NOTICE, "RADclock netlink is up, with genl ID = %d", id);
	PRIV_DATA(clock)->radclock_gnl_id = id;
	logger(RADLOG_NOTICE, "Feed-Forward Kernel initialised");

	return (0);
}



/* Workhorse fn for radclock_gnl_get_attr handling attributes.
 * Extracts and checks all attributes for a single nl message.
 * There is no request for a particular attribute, one simply picks up all available.
 * The fn act only on the desired attribute type attrib_id, copies data out to *into .
 */
static int
radclock_gnl_receive(int id, struct nlmsghdr *h, int attrib_id, void *into)
{
	struct nlattr * attrs[RADCLOCK_ATTR_MAX +1];	// array of pts to attribute headers
	struct genlmsghdr *ghdr = NLMSG_DATA(h);
	int len = h->nlmsg_len;
	int err;

	if (h->nlmsg_type != id) {
		logger(RADLOG_ERR, "message id not from the RADclock family");
		return (-1);
	}
	if (ghdr->cmd != RADCLOCK_CMD_GETATTR) {
		logger(RADLOG_ERR, "message cmd was not a get attr");
		return (-1);
	}
	len -= NLMSG_LENGTH(GENL_HDRLEN);
	if (len < 0) {
		logger(RADLOG_ERR, "message was not long enough to be a generic netlink");
		return (-1);
	}

	/* Validate the message, parse it to fill attrs array with all attributes present */
	err = nlmsg_parse(h,GENL_HDRLEN,attrs,RADCLOCK_ATTR_MAX,radclock_attr_policy);
	if (err < 0) {
		logger(RADLOG_ERR, "Error in netlink parsing, error code %d", -err);
		return (-1);
	}
//	if (attrs[RADCLOCK_ATTR_DATA])
//		logger(RADLOG_NOTICE, "  parsed netlink message: found raddata attribute");
//	else
//		logger(RADLOG_NOTICE, "  parsed netlink message:    no raddata  attribute");
//
//	if (attrs[RADCLOCK_ATTR_FIXEDPOINT])
//		logger(RADLOG_NOTICE, "  parsed netlink message: found rad fp attribute");
//	else
//		logger(RADLOG_NOTICE, "  parsed netlink message:    no rad fp  attribute");


	/* Grab the payload we want and pass it out */
	if (attrib_id == RADCLOCK_ATTR_DATA && attrs[RADCLOCK_ATTR_DATA]) {
		void *attr_payload = nla_data(attrs[RADCLOCK_ATTR_DATA]);
		if (attr_payload) {
			//logger(RADLOG_NOTICE, "found desired raddata attribute");
			memcpy(into, attr_payload, sizeof(struct radclock_data));
			//nl_data_free(attr_payload);  // entire buf gets freed in radclock_gnl_get_attr
			return (0);
		} else
			logger(RADLOG_ERR, "Could not allocate global raddata attribute");
	}
	if (attrib_id == RADCLOCK_ATTR_FIXEDPOINT && attrs[RADCLOCK_ATTR_FIXEDPOINT]) {
		void *attr_payload = nla_data(attrs[RADCLOCK_ATTR_FIXEDPOINT]);
		if (attr_payload) {
			//logger(RADLOG_NOTICE, "found desired rad fp attribute");
			memcpy(into, attr_payload, sizeof(struct radclock_fixedpoint));
			return (0);
		} else
			logger(RADLOG_ERR, "Could not allocate global rad fp attribute");
	}

	logger(RADLOG_ERR, "  Couldn't find desired attribute in netlink message");
	return (-1);
}


/* Construct and send a netlink message pulling data from the kernel
 * All available atribute data will be accessed, but only attribute attrib_id is sought.
 */
static int radclock_gnl_get_attr(int radclock_gnl_id, int attrib_id, void *into)
{
	struct nl_sock *sk;
	unsigned char *buf;
	struct nlmsghdr *hdr;
	struct sockaddr_nl peer;
	int ret = -1;
	int recv_len;

	struct genlmsghdr generic_header = {
		.cmd = RADCLOCK_CMD_GETATTR,
		.version = 0,
		.reserved =0,
	};

	//logger(RADLOG_NOTICE, "Entering radclock_gnl_get_attr");

	/* Allocate a msg suitable for an ack'd request */
	struct nl_msg *msg;
	msg = nlmsg_alloc_simple(radclock_gnl_id, NLM_F_REQUEST | NLM_F_ACK);
	if (!msg) {
		logger(RADLOG_ERR, "Error allocating message");
		goto errout;
	}
	nlmsg_append(msg, (void *) &generic_header, sizeof(generic_header), 0);

	/* Create and bind socket for the request, complete and send the message */
	sk = nl_socket_alloc();
	if (!sk) {
		logger(RADLOG_ERR, "Error allocating socket");
		goto msg_errout;
	}
	if (nl_connect(sk, NETLINK_GENERIC) < 0) {	//  equivalent to genl_connect(sk);
		logger(RADLOG_ERR, "Error connecting to generic netlink socket");
		goto destroy_errout;
	}
	if (nl_send_auto(sk, msg) < 0) {
		logger(RADLOG_ERR, "Error sending to generic netlink socket");
		goto close_errout;
	}

	/* Obtain pointer to waiting message stream, interate over waiting
	 * messages to find the first acceptable one */
	recv_len = nl_recv(sk, &peer, &buf, NULL);		// allocates memory to buf
	if (recv_len >=0) {
		hdr = (struct nlmsghdr *) buf;
		while (nlmsg_ok(hdr, recv_len)) {	// while more complete messages
			if (radclock_gnl_receive(radclock_gnl_id, hdr, attrib_id, into) < 0) {
				logger(RADLOG_ERR, "Error extracting attribute from netlink message");
				free(buf);
				goto close_errout;
			}
			hdr = nlmsg_next(hdr, &recv_len);
		}
	} else {
		logger(RADLOG_ERR, "Got error code %d when attempted netlink message read", -recv_len);
		goto close_errout;
	}
	free(buf);
	ret = 0;		// success

close_errout:
	nl_close(sk);
destroy_errout:
	nl_socket_free(sk);
msg_errout:
	nlmsg_free(msg);
errout:
	return (ret);
}


/* Construct and send a netlink message carrying *data for attribute type
 * attrib_id to the kernel.  The kernel-side code is capable of receiving more
 * than one attribute in a single message, but here we only send one at a time.
 */
static int
radclock_gnl_set_attr(int radclock_gnl_id, int attrib_id, void *data)
{
	struct nl_sock *sk;
	int ret = -1;

	struct genlmsghdr generic_header = {	// payload header for the nl message
		.cmd = RADCLOCK_CMD_SETATTR,
		.version = 0,
		.reserved = 0,
	};

	/* Allocate a msg with a header for an ack'd request, and append payload hdr */
	struct nl_msg *msg;
	msg = nlmsg_alloc_simple(radclock_gnl_id, NLM_F_REQUEST | NLM_F_ACK);
	if (!msg) {
		logger(RADLOG_ERR, "Error allocating message");
		goto errout;
	}
	nlmsg_append(msg, &generic_header, GENL_HDRLEN, 0);

	/* Create and bind socket for the request */
	sk = nl_socket_alloc();
	if (!sk) {
		logger(RADLOG_ERR, "Error allocating handle");
		goto msg_errout;
	}
	if (nl_connect(sk, NETLINK_GENERIC) < 0) {
		logger(RADLOG_ERR, "Error connecting to generic netlink socket");
		goto destroy_errout;
	}

	/* Append desired attribute payload in message */
	if (attrib_id == RADCLOCK_ATTR_DATA) {
		if (nla_put(msg, RADCLOCK_ATTR_DATA, sizeof(struct radclock_data), data)) {
			logger(RADLOG_ERR, "Couldn't set attr");
			goto close_errout;
		}
	} else if (attrib_id == RADCLOCK_ATTR_FIXEDPOINT) {
		if (nla_put(msg, RADCLOCK_ATTR_FIXEDPOINT, sizeof(struct radclock_fixedpoint), data)) {
			logger(RADLOG_ERR, "Couldn't set attr");
			goto close_errout;
		}
	}

	/* Complete and send the message, wait for ACK */
	if (nl_send_auto(sk, msg) < 0) {
		logger(RADLOG_ERR, "Error sending to generic netlink socket");
		goto close_errout;
	}
	nl_wait_for_ack(sk);
	ret = 0;		// success

	logger(RADLOG_NOTICE, "Exiting radclock_gnl_set_attr");


close_errout:
	nl_close(sk);
destroy_errout:
	nl_socket_free(sk);
msg_errout:
	nlmsg_free(msg);
errout:
	return (ret);
}


// TODO the set_kernel_ffclock should be in the library too?
int
get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
{
	logger(RADLOG_ERR, "Not yet getting ffclock data in the kernel");
	if (clock->kernel_version < 2) {
		logger(RADLOG_ERR, "  get_kernel_ffclock with unfit kernel!");
		return (1);
	}

	return (0);
}


int
set_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
{
//	JDEBUG
//	int err;
//	struct ffclock_data fdata;
//	vcounter_t vcount;
//	long double time;
//	uint64_t period;
//	uint64_t period_shortterm;
//	uint64_t frac;
//
	logger(RADLOG_ERR, "Not yet setting ffclock data in the kernel");
	if (clock->kernel_version < 2) {
		logger(RADLOG_ERR, "set_kernel_ffclock with unfit kernel!");
		return (1);
	}

//	/*
//	 * Build the data structure to pass to the kernel
//	 */
//	vcount = RAD_DATA(clock)->last_changed;
//
//	/* Convert vcount to long double time and to bintime */
//	if (radclock_vcount_to_abstime_fp(clock, &vcount, &time))
//		logger(RADLOG_ERR, "Error calculating time");
//
//	/* What I would like to do is:
//	 * fdata->time.frac = (time - (time_t) time) * (1LLU << 64);
//	 * but cannot push '1' by 64 bits, does not fit in LLU. So push 63 bits,
//	 * multiply for best resolution and loose resolution of 1/2^64.
//	 * Same for phat.
//	 */
//	fdata.time.sec = (time_t) time;
//	frac = (time - (time_t) time) * (1LLU << 63);
//	fdata.time.frac = frac << 1;
//
//	period = ((long double) RAD_DATA(clock)->phat) * (1LLU << 63);
//	fdata.period = period << 1;
//
//	period_shortterm = ((long double) RAD_DATA(clock)->phat_local) * (1LLU << 63);
//	fdata.period_shortterm = period_shortterm << 1;
//
//	fdata.last_update = vcount;
//	fdata.status = RAD_DATA(clock)->status;
//	fdata.error_bound_avg = (uint32_t) RAD_ERROR(clock)->error_bound_avg * 1e9;
//
//	
//	/* Push */
//	err = radclock_gnl_set_attr(PRIV_DATA(clock)->radclock_gnl_id, RADCLOCK_ATTR_DATA, &fdata);
//	if ( err < 0 ) {
//		logger(RADLOG_ERR, "error on syscall set_ffclock: %s", strerror(errno));
//		return (1);
//	}

	return (0);
}


//extern struct radclock_handle *clock_handle;

/*
 * XXX Deprecated
 * Old way of pushing clock updates to the kernel.
 * TODO: remove when backward compatibility for kernel versions < 2 is dropped.
 */
inline int
set_kernel_fixedpoint(struct radclock *handle, struct radclock_fixedpoint *fpdata)
{
	int err, err_get;
	struct radclock_fixedpoint kernfpdata;
//	struct radclock_data kernraddata;
////	struct radclock_data *raddata = &(clock_handle->rad_data[0]);
//	struct radclock_data rd;
//
//	rd.status = 1;
//	rd.phat = 1e-9;

	switch (handle->kernel_version)
	{
	case 0:
	case 1:
		err     = radclock_gnl_set_attr(PRIV_DATA(handle)->radclock_gnl_id, RADCLOCK_ATTR_FIXEDPOINT, fpdata);
	 	err_get = radclock_gnl_get_attr(PRIV_DATA(handle)->radclock_gnl_id, RADCLOCK_ATTR_FIXEDPOINT, &kernfpdata);
		if ( memcmp(fpdata,  &kernfpdata,sizeof(struct radclock_fixedpoint)) != 0 )
			logger(RADLOG_ERR, "netlink inversion test for fp data failed, err_get = %d", err_get);

//		err     = radclock_gnl_set_attr(PRIV_DATA(handle)->radclock_gnl_id, RADCLOCK_ATTR_DATA, &rd);
//		err_get = radclock_gnl_get_attr(PRIV_DATA(handle)->radclock_gnl_id, RADCLOCK_ATTR_DATA, &kernraddata);
//		if ( memcmp(&rd,&kernraddata,sizeof(struct radclock_data)) != 0 ) {
//			logger(RADLOG_ERR, "netlink inversion test for raddata failed, err_get = %d", err_get);
//			logger(RADLOG_NOTICE, "rd:      status = %d,  phat = %.10lg", rd.status, rd.phat);
//			logger(RADLOG_NOTICE, "kernrd:  status = %d,  phat = %.10lg", kernraddata.leapsec_total, kernraddata.phat);
//		}
		break;

	case 2:	
		logger(RADLOG_ERR, "set_kernel_fixedpoint but kernel version 2!!");
		return (1);

	default:
		logger(RADLOG_ERR, "Unknown kernel version");
		return (1);
	}

	if (err < 0) 
		return (1);
	return (0);
}

#endif
