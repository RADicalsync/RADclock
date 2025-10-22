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
#include "proto_ntp.h"
#include "sync_algo.h"
#include "config_mgr.h"
#include "stampoutput.h"
#include "jdebug.h"


/* Open the ASCII sync stamp file for writing, and write the correct header
 * dependent on the passed stamp type.
 */
int
open_output_stamp(struct radclock_handle *handle, stamp_type_t type)
{
	char *backup;
	char *out;

	out = handle->conf->sync_out_ascii;
	if (strlen(out) == 0)
		return (0);

	/* Test if previous file exists. Rename it if so */
	handle->stampout_fd = fopen(out,"r");
	if (handle->stampout_fd) {
		fclose(handle->stampout_fd);
		backup = (char *) malloc(strlen(out)+ 5);
		JDEBUG_MEMORY(JDBG_MALLOC, backup);

		sprintf(backup, "%s.old", out);
		if (rename(out, backup) < 0) {
			verbose(LOG_ERR, "Cannot rename existing output file: %s", out);

			JDEBUG_MEMORY(JDBG_FREE, backup);
			free(backup);
			exit(EXIT_FAILURE);
		}
		verbose(LOG_NOTICE, "Backed up existing output file: %s", out);
		JDEBUG_MEMORY(JDBG_FREE, backup);
		free(backup);
		handle->stampout_fd = NULL;
	}

	/* Open output file to store input data in preprocessed stamp format */
	handle->stampout_fd = fopen(out,"w");
	if (handle->stampout_fd == NULL) {
		verbose(LOG_ERR, "Open failed on stamp output file- %s", out);
		exit(EXIT_FAILURE);
	} else {  // write out comment header describing data
		/* TODO: turn off buffering? */
		setvbuf(handle->stampout_fd, (char *)NULL, _IONBF, 0);
		fprintf(handle->stampout_fd, "%% BEGIN_HEADER\n");
		if (type == STAMP_NTP_PERF || type == STAMP_NTP_INT) {
			verbose(LOG_NOTICE, "Writing NTP_PERF (8tuple) stamp lines to file %s", out);
			fprintf(handle->stampout_fd, "%% description: radclock local vcounter "
			    "NTP server, and reference timestamps\n");
			fprintf(handle->stampout_fd, "%% type: NTP_PERF\n");
			fprintf(handle->stampout_fd, "%% version: 2\n");
			fprintf(handle->stampout_fd, "%% fields: Ta Tb Te Tf IntRefOut IntRefIn ExtRefOut ExtRefIn [sID]\n");
		} else {
			verbose(LOG_NOTICE, "Writing NTP (4tuple) stamp lines to file %s", out);
			fprintf(handle->stampout_fd, "%% description: radclock local vcounter "
			    "and NTP server timestamps\n");
			fprintf(handle->stampout_fd, "%% type: NTP\n");
			fprintf(handle->stampout_fd, "%% version: 4\n");
			fprintf(handle->stampout_fd, "%% fields: Ta Tb Te Tf nonce [sID]\n");
		}
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
	fprintf(handle->matout_fd, "%% columns 8--10 - RTThat, RTThat_new, RTThat_sh\n");
	fprintf(handle->matout_fd, "%% columns 11--17 - th_naive, minET, minET_last,"
	    " RADclockout, RADclockin, pDf, pDb\n");
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


/* Outputs the {RAD,PERF}stamp timestamp tuples to file (if open).
 *
 * RADstamp case  [ 4tuple input for RADclock, and nonce for offline matching ]
 *  4tuple + nonce [+ sID]
 * PERFstamp case [ 4tuple input for RADclock, and 2tuple Reference timestamps
 *                  required for EXTREF/PERF evaluation ]
 *  4tuple + {RefIn RefOut} [+ sID]  [ traditional order for backward compat ]
 *
 * Outputs one ascii line per stamp regardless of originating server, with
 * the serverID in the last column when applicable.
 */
void
print_out_syncline(struct radclock_handle *handle, struct stamp_t *stamp, int sID)
{
	int err;

	/* Generate fake DAG timestamps for one-off fake PERFstamp generation */
	//	struct radclock_error *rad_error = &handle->rad_error[sID];
	//	double DAGOWD = rad_error->min_RTT/2 - 0.3e-3;    // fake constant OWD from DAG
	//	long double fakeTout = BST(stamp)->Tb - DAGOWD;
	//	long double fakeTin  = BST(stamp)->Te + DAGOWD;

	if (handle->stampout_fd == NULL)
		return;

	if ((stamp->type != STAMP_NTP)     && (stamp->type != STAMP_NTP_PERF) &&
		 (stamp->type != STAMP_NTP_INT) && (stamp->type != STAMP_SPY))
		verbose(LOG_ERR, "Do not know how to print a stamp of type %d", stamp->type);

	if (stamp->type != STAMP_NTP)  // PERFstamp
		if (handle->nservers == 1)  // omit last column with serverID
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %.9Lf %.9Lf %.9Lf %.9Lf\n",
			  (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			  (long long unsigned)BST(stamp)->Tf, stamp->IntRef.Tout, stamp->IntRef.Tin,
			  stamp->ExtRef.Tout, stamp->ExtRef.Tin);
		else {                       // include serverID in last column
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %.9Lf %.9Lf %.9Lf %.9Lf %d\n",
			  (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			  (long long unsigned)BST(stamp)->Tf, stamp->IntRef.Tout, stamp->IntRef.Tin,
			  stamp->ExtRef.Tout, stamp->ExtRef.Tin,
			  sID);
		  // Troubleshooting hack to export Df,Db (for verification) and un-backtracked BL
//			struct bidir_caldata *caldata;
//			struct bidir_calibstate *calstate;
//			double Df, Db, BLf, BLb;
//			if (handle->calibrate) {
//				caldata = handle->caldata;
//				calstate = &caldata->state[sID];
//				Df = calstate->Df;
//				Db = calstate->Db;
//				BLf = calstate->Df_state.BL;
//				BLb = calstate->Db_state.BL;
//			} else
//				Df = Db = BLf = BLb = 0;
//			err = fprintf(handle->stampout_fd,"%.9lf %.9lf %.9lf %.9lf %d\n", Df, Db, BLf, BLb, sID);
		}
	else                           // RADstamp
		if (handle->nservers == 1)
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %llu\n",
			  (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			  (long long unsigned)BST(stamp)->Tf,
			  (long long unsigned)stamp->id);
		else
			/* Fake PERFstamp generation in multiple server case */
			//err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %.9Lf %.9Lf %d\n",
			//  (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			//  (long long unsigned)BST(stamp)->Tf, fakeTin, fakeTout,
			//  sID);
			err = fprintf(handle->stampout_fd,"%"VC_FMT" %.9Lf %.9Lf %"VC_FMT" %llu %d\n",
			  (long long unsigned)BST(stamp)->Ta, BST(stamp)->Tb, BST(stamp)->Te,
			  (long long unsigned)BST(stamp)->Tf,
			  (long long unsigned)stamp->id,
			  sID);

	if (err < 0)
		verbose(LOG_ERR, "Failed to write ascii data to timestamp file");
}



/* Outputs the algo-internals output structure to file (if open).
 * Output one line per stamp regardless of originating server, with
 * the serverID in the last column when applicable.
 */
void
print_out_clockline(struct radclock_handle *handle, struct stamp_t *stamp,
	struct bidir_algooutput *output, int sID)
{
	int err;

	if (handle->matout_fd == NULL)
		return;

	if ((stamp->type != STAMP_NTP)     && (stamp->type != STAMP_NTP_PERF) &&
	    (stamp->type != STAMP_NTP_INT) && (stamp->type != STAMP_SPY))
		verbose(LOG_ERR, "Do not know how to print a stamp of type %d", stamp->type);

	/* ld since must hold [s] since timescale origin, and at least 1mus precision */
	long double currtime_out, currtime_in;
	currtime_out = (long double)(BST(stamp)->Ta * output->phat) + output->K;
	currtime_in  = (long double)(BST(stamp)->Tf * output->phat) + output->K;

	char *buf;
	buf = (char *) malloc(500 * sizeof(char));  // must be big enuf for single disk access
	JDEBUG_MEMORY(JDBG_MALLOC, buf);

	sprintf(buf,
		"%.9Lf %"VC_FMT" %"VC_FMT" %.10lg %.10lg %.11Lf %.10lf "
		"%"VC_FMT" %"VC_FMT" %"VC_FMT" %.9lg %.9lg %.9lg %.11Lf "
		"%.11Lf %.10lf %.10lf %.6lg %.6lg %.6lg %"VC_FMT" %u "
		"%.9lg %.9lg %.9lg",    // pathpenalty metrics
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
		output->pDf,
		output->pDb,
		output->perr,
		output->plocalerr,
		output->wsum,
		output->best_Tf,
		output->status,
		// pathpenalty metrics
		output->pathpenalty,
		output->Pchange,
		output->Pquality);
		
	if (handle->nservers > 1)  // add last column with serverID
		sprintf(buf,"%s %d", buf, sID);

	err = fprintf(handle->matout_fd, "%s\n", buf);
	if (err < 0)
		verbose(LOG_ERR, "Failed to write data to matlab file");

	JDEBUG_MEMORY(JDBG_FREE, buf);
	free(buf);
}

