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
#include <string.h>
#include <syslog.h>
#include <pcap.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"

#include "radclock_daemon.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "config_mgr.h"
#include "stampoutput.h"
#include "jdebug.h"


int
open_output_stamp(struct radclock_handle *handle)
{
	char *backup;

	/* Sometimes, there is nothing to do */
	if (strlen(handle->conf->sync_out_ascii) == 0)
		return (0);

	/* Test if previous file exists. Rename it if so */
	handle->stampout_fd = fopen(handle->conf->sync_out_ascii, "r");
	if (handle->stampout_fd) {
		fclose(handle->stampout_fd);
		backup = (char *) malloc(strlen(handle->conf->sync_out_ascii)+ 5);
		JDEBUG_MEMORY(JDBG_MALLOC, backup);

		sprintf(backup, "%s.old", handle->conf->sync_out_ascii);
		if (rename(handle->conf->sync_out_ascii, backup) < 0) {
			verbose(LOG_ERR, "Cannot rename existing output file: %s",
					handle->conf->sync_out_ascii);

			JDEBUG_MEMORY(JDBG_FREE, backup);
			free(backup);
			exit(EXIT_FAILURE);
		}
		verbose(LOG_NOTICE, "Backed up existing output file: %s",
				handle->conf->sync_out_ascii);
		JDEBUG_MEMORY(JDBG_FREE, backup);
		free(backup);
		handle->stampout_fd = NULL;
	}

	/* Open output file to store input data in preprocessed stamp format */
	handle->stampout_fd = fopen(handle->conf->sync_out_ascii,"w");
	if (handle->stampout_fd == NULL) {
		verbose(LOG_ERR, "Open failed on stamp output file- %s",
				handle->conf->sync_out_ascii);
		exit(EXIT_FAILURE);
	/* write out comment header describing data saved */
	} else {
		/* TODO: turn off buffering? */
		setvbuf(handle->stampout_fd, (char *)NULL, _IONBF, 0);
		fprintf(handle->stampout_fd, "%% BEGIN_HEADER\n");
		fprintf(handle->stampout_fd, "%% description: radclock local vcounter "
				"and NTP server stamps\n");
		fprintf(handle->stampout_fd, "%% type: NTP_rad\n");
		fprintf(handle->stampout_fd, "%% version: 4\n");
		fprintf(handle->stampout_fd, "%% fields: Ta Tb Te Tf nonce [sID]\n");
		fprintf(handle->stampout_fd, "%% END_HEADER\n");
	}
	return (0);
}


void close_output_stamp(struct radclock_handle *handle)
{
	if (handle->stampout_fd != NULL) {
		fflush(handle->stampout_fd);
		fclose(handle->stampout_fd);
	}
}




int
open_output_matlab(struct radclock_handle *handle)
{
	char *backup;

	/* Sometimes, there is nothing to do */
	if (strlen(handle->conf->clock_out_ascii) == 0)
		return (0);

	/* Test if previous file exists. Rename it if so */
	handle->matout_fd = fopen(handle->conf->clock_out_ascii, "r");
	if (handle->matout_fd) {
		fclose(handle->matout_fd);
		backup = (char*) malloc(strlen(handle->conf->clock_out_ascii) + 5);
		JDEBUG_MEMORY(JDBG_MALLOC, backup);

		sprintf(backup, "%s.old", handle->conf->clock_out_ascii);
		if (rename(handle->conf->clock_out_ascii, backup) < 0) {
			verbose(LOG_ERR, "Cannot rename existing output file: %s",
					handle->conf->clock_out_ascii);
			JDEBUG_MEMORY(JDBG_FREE, backup);
			free(backup);
			exit(EXIT_FAILURE);
		}
		verbose(LOG_NOTICE, "Backed up existing output file: %s",
				handle->conf->clock_out_ascii);
		JDEBUG_MEMORY(JDBG_FREE, backup);
		free(backup);
		handle->matout_fd = NULL;
	}


	/* Open output file to store synchronisation algorithm output (for Matlab,
	 * written in RADalgo_bidir).
	 */
	handle->matout_fd = fopen(handle->conf->clock_out_ascii,"w");
	if (handle->matout_fd == NULL) {
		verbose(LOG_ERR, "Open failed on Matlab output file- %s",
				handle->conf->clock_out_ascii);
		exit(EXIT_FAILURE);
	} else
		/* TODO turn off buffering ? */
		setvbuf(handle->matout_fd, (char *)NULL, _IONBF, 0);

	fprintf(handle->matout_fd, "%% NTP packet filtering run with:\n");
	fprintf(handle->matout_fd, "%%\n");
	fprintf(handle->matout_fd, "%% column 1 - Tb \n");
	fprintf(handle->matout_fd, "%% column 2 - Tf \n");
	fprintf(handle->matout_fd, "%% column 3 - RTT\n");
	fprintf(handle->matout_fd, "%% column 4 - phat\n");
	fprintf(handle->matout_fd, "%% column 5 - plocal\n");
	fprintf(handle->matout_fd, "%% column 6 - K\n");
	fprintf(handle->matout_fd, "%% column 7 - thetahat\n");
	fprintf(handle->matout_fd, "%% columns 8--10 - RTThat, RTThat_new,"
			"RTThat_sh\n");
	fprintf(handle->matout_fd, "%% columns 11--17 - th_naive, minET, minET_last,"
			" RADclockout, RADclockin, errTa, errTf\n");
	fprintf(handle->matout_fd, "%% columns 18--22 - perr, plocalerr, wsum, "
			"best_Tf, clock status\n");
	fprintf(handle->matout_fd, "%%\n");

	return (0);
}



void
close_output_matlab(struct radclock_handle *handle)
{
	if (handle->matout_fd != NULL) {
		fflush(handle->matout_fd);
		fclose(handle->matout_fd);
	}
}

/* This function covers the cases of the
 *   ascii stamp out (4tuple + nonce + sID)  [ input to RADclock offline ]
 *   ascii algo-internals (lots..    + sID)  [ input to matlab evaluation analysis ]
 * Data is output one line per stamp regardless of originating server, with
 * the serverID in the last column when applicable.
 */
void
print_out_files(struct radclock_handle *handle, struct stamp_t *stamp,
	struct bidir_algooutput *output, int sID)
{
	int err;
	// XXX What is the reason for me to do it that way? Cannot remember.
	/* A single buffer to have a single disk access, it has to be big enough */
	char *buf;

	/* long double since must hold [sec] since timescale origin, and at least
	 * 1mus precision
	 */
	long double currtime_out, currtime_in;

	if ((stamp->type != STAMP_NTP) && (stamp->type != STAMP_SPY))
		verbose(LOG_ERR, "Do not know how to print a stamp of type %d", stamp->type);

	currtime_out = (long double)(BST(stamp)->Ta * output->phat) + output->K;
	currtime_in  = (long double)(BST(stamp)->Tf * output->phat) + output->K;

	/* Deal with sync output */
	if (handle->stampout_fd != NULL) {
		if (handle->nservers == 1)	// omit last column with serverID
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %llu\n",
					(long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
					(long long unsigned)BST(stamp)->Tf,
					(long long unsigned)stamp->id);
		else	// include serverID in last column
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %llu %d\n",
					(long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
					(long long unsigned)BST(stamp)->Tf,
					(long long unsigned)stamp->id,
					sID);
				
		if (err < 0)
			verbose(LOG_ERR, "Failed to write ascii data to timestamp file");
	}
	
	
	/* Deal with internal algo output */
	if (handle->matout_fd == NULL)
		return;

	buf = (char *) malloc(500 * sizeof(char));
	JDEBUG_MEMORY(JDBG_MALLOC, buf);

	sprintf(buf,
		"%.9Lf %"VC_FMT" %"VC_FMT" %.10lg %.10lg %.11Lf %.10lf "
		"%"VC_FMT" %"VC_FMT" %"VC_FMT" %.9lg %.9lg %.9lg %.11Lf "
		"%.11Lf %.10lf %.10lf %.6lg %.6lg %.6lg %"VC_FMT" %u",
		BST(stamp)->Tb,
		(unsigned long long)BST(stamp)->Tf,
		(unsigned long long)output->RTT,
		output->phat,
		output->plocal,
		output->K,
		output->thetahat,
		(unsigned long long)output->RTThat,
		(unsigned long long)output->RTThat_new,
		(unsigned long long)output->RTThat_shift,
		output->th_naive,
		output->minET,
		output->minET_last,
		currtime_out,
		currtime_in,
		output->errTa,
		output->errTf,
		output->perr,
		output->plocalerr,
		output->wsum,
		output->best_Tf,
		output->status);
		
	if (handle->nservers > 1)	// add last column with serverID
		sprintf(buf,"%s %d", buf, sID);

	err = fprintf(handle->matout_fd, "%s\n", buf);
	if (err < 0)
		verbose(LOG_ERR, "Failed to write data to matlab file");

	JDEBUG_MEMORY(JDBG_FREE, buf);
	free(buf);
}

