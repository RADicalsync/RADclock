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

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"

#include "radclock_daemon.h"
#include "sync_history.h"
#include "proto_ntp.h"
#include "sync_algo.h"
#include "config_mgr.h"
#include "verbose.h"
#include "create_stamp.h"
#include "stampinput.h"
#include "stampinput_int.h"
#include "jdebug.h"


#define ASCII_DATA(x) ((struct ascii_data *)(x->priv_data))


/* Parse commented header lines (comment char selectable)
 * Used to skip comment block headings in ascii input files, and to parse it
 * for tuple type information to determine if a RADstamp, PERFstamp etc.
 * TODO: check for all types and throw error if not recognised, currently
 *       only actively searching for NTP_PERF
 */
int
parse_commentblock(FILE *fd, const char commentchar, stamp_type_t *stamptype)
{
	int c;
	/* line extraction */
	ssize_t linelen;
	char *line = NULL;
	size_t llen = 0;  // will be ignored
	/* line parsing */
	char typeline[] = " type: NTP_PERF";  // includes null termination
	int tlen = strlen(typeline);          // excludes null termination

	*stamptype = STAMP_NTP;  // default
	/* If first char of line is a comment, skip it */
	while ((c=fgetc(fd)) != EOF && c == commentchar) {
		linelen = getline(&line, &llen, fd);  // read line post-commentchar incl EoL
		// fprintf(stdout,"%s", line);        // echo it
		if ( !strncmp(typeline, line, tlen) )
			*stamptype = STAMP_NTP_PERF;

		free(line); line = NULL;             // clean slate for next line
//		fprintf(stdout,"c line with total %ld (%d) chars after c-char\n", linelen, tlen);
	}

	/* This first char wasn't a comment, push it back, is from the first data line */
	if (c != EOF) {
		c = ungetc(c,fd);
	}
	return (c);    // returns EOF if no data
}


/* Open the stamp file and obtain its type */
static FILE *
open_stampfile(char* in, stamp_type_t *stamptype)
{
	FILE* stamp_fd = NULL;
	int ch;
	if ((stamp_fd = fopen(in,"r")) == NULL) {
		verbose(LOG_ERR, "Open failed on preprocessed stamp input file- %s", in);
		return (NULL);
	}

	if ((ch = parse_commentblock(stamp_fd,'%', stamptype)) == EOF)
		verbose(LOG_WARNING,"Stored ascii stamp file %s seems to be data free", in);
	else
		if (*stamptype == STAMP_NTP_PERF)
			verbose(LOG_NOTICE, "Reading from ascii file %s (detected as NTP_PERF 8tuple)", in);
		else
			verbose(LOG_NOTICE, "Reading from ascii file %s (detected as NTP 4tuple)", in);

	return (stamp_fd);
}

/* Prepare ascii input file for reading of data lines */
static int
asciistamp_init(struct radclock_handle *handle, struct stampsource *source)
{
	source->priv_data = (struct ascii_data *) malloc(sizeof(struct ascii_data));
	JDEBUG_MEMORY(JDBG_MALLOC, source->priv_data);
	if (!ASCII_DATA(source)) {
		verbose(LOG_ERR, "Couldn't allocate memory");
		return (-1);
	}

	stamp_type_t type = STAMP_UNKNOWN;
	if (strlen(handle->conf->sync_in_ascii) > 0) {
		ASCII_DATA(source)->fd = open_stampfile(handle->conf->sync_in_ascii, &type);
		if (!ASCII_DATA(source)->fd)
			return (-1);
	}
	ASCII_DATA(source)->stamptype = type;  // record detected type

	return (0);
}


/* Reads in a line of input from an ascii input file holding the timestamp tuple.
 * Different versions of the ascii input format are supported:
 *  NTP_PERF stamps:   [ auto-detected ]
 *   Version 1:   fields are Ta Tb Te Tf RefIn RefOut [sID]  **no longer supported**
 *   Version 2:   fields are Ta Tb Te Tf IntRefOut IntRefIn ExtRefOut ExtRefin [sID] (8tuple)
 *    Here the Ref inputs are read into a NTP_PERF stamp_t
 *  NTP stamps:   [ auto-detected ]
 *
 *   Unversioned: fields are Ta Tb Te Tf [RefIn [RefOut]]  ("4col 5col 6col" file)
 *   Version 3:   fields are Ta Tb Te Tf nonce
 *   Version 4:   fields are Ta Tb Te Tf nonce [sID]
 *    Here [RefIn RefOut] if present will be ignored by RADclock, and "nonce" was
 *    the key used to perform packet matching to assemble the RADstamp 4tuple.
 *
 * For each type sID only appears when there are multiple servers.
 *
 * REQUIRE: the number of servers (ns) in the conf file must be at least as
 * great as those appearing in the input, otherwise the additional server lines
 * will be dropped subsequently.
 *
 * NOTE: even if the same conf file (hence the same servers in the same order)
 * is used on dead replay as in ascii data generation, the replay sID's need not
 * match the input sID's, as these are allocated in stamp arrival order
 * (see serverIPtoID). This is unimportant as the sID<-->IP mapping is arbitrary,
 * but can be confusing when externally visualising the output. In particular
 * any originally unavailable servers implicitly end up corresponding to the
 * largest new sIDs, and so never appear in the new processing/output, not even
 * as starved servers (meaningless when dead).
 *
 * TODO:  increase robustness to possible variations in import format
 *  Currently traditional 6col (and 5col) are "working" by fluke, sID read fails, so get
 *  sID=0, which is correct, and buggy stamp->id doesn't matter as matching done already.
 *  Could potentially get the #cols from asciistamp_init and use it here, then
 *  remove the static, but still want the stamp check verb.
 */
static int
asciistamp_get_next(struct radclock_handle *handle, struct stampsource *source,
    struct stamp_t *stamp)
{
	FILE *stamp_fd = ASCII_DATA(source)->fd;
	int ncols;
	int input_sID = 0;
	static int firstpass = 1;  // input-check verbosity control

	/* line extraction */
	char *line = NULL;
	size_t llen = 0;  // will be ignored

	getline(&line, &llen, stamp_fd);  // read line post-commentchar incl EoL
	if (ASCII_DATA(source)->stamptype == STAMP_NTP_PERF) {
		stamp->type = STAMP_NTP_PERF;
		// verbose(LOG_NOTICE, "Getting next NTP_PERF 6tuple");
		ncols = sscanf(line, "%"VC_FMT" %Lf %Lf %"VC_FMT" %Lf %Lf %Lf %Lf %d",
		    &(BST(stamp)->Ta), &(BST(stamp)->Tb),
		    &(BST(stamp)->Te), &(BST(stamp)->Tf),
		    &(stamp->IntRef.Tout), &(stamp->IntRef.Tin),
		    &(stamp->ExtRef.Tout), &(stamp->ExtRef.Tin), &input_sID);
// Version 1 input --> Version 2  hack setting IntRef = ExtRef ------------
//		ncols = sscanf(line, "%"VC_FMT" %Lf %Lf %"VC_FMT" %Lf %Lf %d",
//		    &(BST(stamp)->Ta), &(BST(stamp)->Tb),
//		    &(BST(stamp)->Te), &(BST(stamp)->Tf),
//		    &(stamp->ExtRef.Tin), &(stamp->ExtRef.Tout), &input_sID);
//		stamp->IntRef.Tout = stamp->ExtRef.Tout;
//		stamp->IntRef.Tin  = stamp->ExtRef.Tin;
//		if (ncols != EOF)
//			ncols = 9;
// ---------------- end V1 hack --------------------------------------------
	} else {
		stamp->type = STAMP_NTP;
		// verbose(LOG_NOTICE, "Getting next NTP 4tuple");
		ncols = sscanf(line, "%"VC_FMT" %Lf %Lf %"VC_FMT" %"VC_FMT" %d",
		    &(BST(stamp)->Ta), &(BST(stamp)->Tb),
		    &(BST(stamp)->Te), &(BST(stamp)->Tf),
		    &stamp->id, &input_sID);
	}
	free(line); line = NULL;  // clean slate for next line

	/* Needed fields absent from ascii input */
	stamp->refid = "FAKE";

	if (ncols == EOF) {
		verbose(LOG_NOTICE, "Got EOF on ascii input file.");
		return (-1);
	}

	/* Assign distinct fake IP address matching serverID */
	if (firstpass) {
		verbose(LOG_NOTICE, "Found %d columns in ascii input file", ncols);
		/* Stamp read check */
		if (stamp->type == STAMP_NTP_PERF) {
			verbose(VERB_DEBUG,"read check: %"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %.9Lf %.9Lf %.9Lf %.9Lf %d",
			    (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			    (long long unsigned)BST(stamp)->Tf,
			    stamp->IntRef.Tout, stamp->IntRef.Tin,
			    stamp->ExtRef.Tout, stamp->ExtRef.Tin, input_sID);
			if (ncols > 8)
				verbose(LOG_NOTICE, "Assuming multiple servers, will assign fake "
				    "IP's as 10.0.0.input_sID in first-seen order");
		} else {
			verbose(VERB_DEBUG,"read check: %"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %llu %d",
			    (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			    (long long unsigned)BST(stamp)->Tf,
			    (long long unsigned)stamp->id, input_sID);
			if (ncols > 5)
				verbose(LOG_NOTICE, "Assuming multiple servers, will assign fake "
				    "IP's as 10.0.0.input_sID in first-seen order");
		}
		firstpass--;
	}

	// Assign IP to server (for each input stamp, even in single server case)
	sprintf(stamp->server_ipaddr, "10.0.0.%d", input_sID);
	verbose(VERB_DEBUG, "input serverID value %d assigned to fake IP address %s",
	    input_sID, stamp->server_ipaddr);

	source->ntp_stats.ref_count += 2;

	return (0);
}


static void
asciistamp_breakloop(struct radclock_handle *handle, struct stampsource *source)
{
	verbose(LOG_WARNING, "Call to breakloop in ascii replay has no effect");
	return;
}


static void
asciistamp_finish(struct radclock_handle *handle, struct stampsource *source)
{
	fclose(ASCII_DATA(source)->fd);
	JDEBUG_MEMORY(JDBG_FREE, ASCII_DATA(source));
	free(ASCII_DATA(source));
}

static int
asciistamp_update_filter(struct radclock_handle *handle, struct stampsource *source)
{
	/* So far this does nothing ...  */
	return (0);
}

static int
asciistamp_update_dumpout(struct radclock_handle *handle, struct stampsource *source)
{
	/* So far this does nothing ...  */
	return (0);
}

//This is externed elsehere
struct stampsource_def ascii_source =
{
	.init             = asciistamp_init,
	.get_next_stamp   = asciistamp_get_next,
	.source_breakloop = asciistamp_breakloop,
	.destroy          = asciistamp_finish,
	.update_filter    = asciistamp_update_filter,
	.update_dumpout   = asciistamp_update_dumpout,
};
