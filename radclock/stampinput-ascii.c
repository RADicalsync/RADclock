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


struct ascii_data
{
	FILE *fd;
};


/* Function to skip contiguous comment lines (comment char selectable)
 * Used to skip comment block headings in ascii input files.
 */
int
skip_commentblock(FILE *fd, const char commentchar)
{
	int c;
	// see if first char of line is a comment
	while ((c=fgetc(fd))!=EOF && c==commentchar ){
		while ((fgetc(fd))!= '\n'){}    // skip to start of next line
		//fprintf(stdout,"c line: \n");
	}
	if (c!=EOF) {
		// oops, first char wasn't a comment, push it back, is the first data element
		c= ungetc(c,fd);
	}
	return (c);   // returns EOF if no data
}


/* Open an ascii stamp input file. */
static FILE *
open_timestamp(char* in)
{
	FILE* stamp_fd = NULL;
	int ch;
	if ((stamp_fd = fopen(in,"r")) == NULL) {
		verbose(LOG_ERR, "Open failed on preprocessed stamp input file- %s", in);
		return (NULL);
	}

	if ((ch=skip_commentblock(stamp_fd,'%'))==EOF)
		verbose(LOG_WARNING,"Stored ascii stamp file %s seems to be data free", in);
	else
		verbose(LOG_NOTICE, "Reading from stored ascii stamp file %s", in);

	return (stamp_fd);
}

static int
asciistamp_init(struct radclock_handle *handle, struct stampsource *source)
{
	source->priv_data = (struct ascii_data *) malloc(sizeof(struct ascii_data));
	JDEBUG_MEMORY(JDBG_MALLOC, source->priv_data);
	if (!ASCII_DATA(source)) {
		verbose(LOG_ERR, "Couldn't allocate memory");
		return (-1);
	}

	// Timestamp file input
	if (strlen(handle->conf->sync_in_ascii) > 0) {
		// preprocessed ascii TS input available, assumed 4 column
		ASCII_DATA(source)->fd = open_timestamp(handle->conf->sync_in_ascii);
		if (!ASCII_DATA(source)->fd)
			return (-1);
	}
	return (0);
}


/* Reads in a line of input from an ascii input file containing the 4tuple.
 * Different versions of the ascii input format are supported:
 *  Unversioned: fields are Ta Tb Te Tf [RefIn [RefOut]]  ("4col 5col 6col" file)
 *  Version 3:   fields are Ta Tb Te Tf nonce
 *  Version 4:   fields are Ta Tb Te Tf nonce [sID]
 * where [RefIn RefOut] is an augmented version of the input used for evaluation
 * (the Ref fields are ignored by RADclock), "nonce" was the key used to perform
 * packet matching to assemble the RADstamp 4tuple, and sID only appears for
 * multiple servers.
 *
 * REQUIRE: the number of servers (ns) in the conf file must be at least as
 * great as those appearing in the input.
 *
 * NOTE: even if the same conf file (hence the same servers in the same order)
 * is used on dead replay as in ascii data generation, the replay sID's need not
 * match the input sID's, as these are allocated in stamp arrival order
 * (see serverIPtoID). This is unimportant as the sID<-->IP mapping is arbitrary,
 * but can be confusing when externally visualising the output.
 *
 * TODO:  increase robustness to possible variations in import format
 *  Currently traditional 6col (and 5col) are "working" by fluke, sID read fails, so get
 *  sID=0, which is correct, and buggy stamp->id doesn't matter as matching done already.
 */
static int
asciistamp_get_next(struct radclock_handle *handle, struct stampsource *source,
    struct stamp_t *stamp)
{
	FILE *stamp_fd = ASCII_DATA(source)->fd;
	int ncols;
	int input_sID = 0;
	static int firstpass = 1;

	if ((ncols = fscanf(stamp_fd, "%"VC_FMT" %Lf %Lf %"VC_FMT" %"VC_FMT" %d",
	    &(BST(stamp)->Ta), &(BST(stamp)->Tb),
	    &(BST(stamp)->Te), &(BST(stamp)->Tf), &stamp->id, &input_sID )) == EOF )
	{
		verbose(LOG_NOTICE, "Got EOF on ascii input file.");
		return (-1);
	}
	else {
		// skip to start of next line (robust to additional unexpected columns)
		while((fgetc(stamp_fd))!= '\n'){}

		/* Assign distinct fake IP address matching serverID */
		if (firstpass) {
			verbose(LOG_NOTICE, "Found ≥ %d columns in ascii input file.", ncols);
			if (ncols == 6)
				verbose(LOG_NOTICE, "Assuming multiple servers, will assign fake "
				    "IP's as 10.0.0.input_sID in first-seen order");
			firstpass = 0;
		}

		// Assign IP to server (for each input stamp, even in single server case)
		sprintf(stamp->server_ipaddr, "10.0.0.%d", input_sID);
//		verbose(VERB_DEBUG, "input serverID value %d assigned to fake IP address %s",
//		    input_sID, stamp->server_ipaddr);

		// TODO: need to detect stamp type, ie, get a better input format
		stamp->type = STAMP_NTP;
		source->ntp_stats.ref_count += 2;
	}
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
