/*
 * Copyright (C) 2006-2011 Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 *
 * This file is part of the radclock program.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "../config.h"

#include <sys/shm.h>
#include <sys/time.h>

#include <errno.h>		// manage failed shmget calls 
#include <fcntl.h>		// open()
#include <stdlib.h>
#include <stdio.h>
#include <string.h>		// manage failed shmget calls 
#include <unistd.h>		// for sleep()


#include "radclock.h"
#include "radclock-private.h"


int
read_raddata(struct radclock_data *data)
{
	
	if (!data) {
		fprintf(stdout, "ERROR: NULL data pointer\n");
		return (1);
	}

	fprintf(stdout, "  phat: %.10g\n", data->phat);
	fprintf(stdout, "  phat_err: %.6g\n", data->phat_err);
	fprintf(stdout, "  phat_local: %.10g\n", data->phat_local);
	fprintf(stdout, "  phat_local_err: %.6g\n", data->phat_local_err);
	fprintf(stdout, "  ca: %.9Lf\n", data->ca);
	fprintf(stdout, "  ca_err: %.6g\n", data->ca_err);
	fprintf(stdout, "  status: %u\n", data->status);
	fprintf(stdout, "  last_changed: %llu\n", (long long unsigned)data->last_changed);
	//fprintf(stdout, "  valid_till: %llu\n", (long long unsigned)data->valid_till);

	return (0);
}

/* Returns 1 on success */
int 
read_sms(struct radclock *clock)
{
	struct radclock_data *data;
	struct radclock_sms *sms;

	sms = (struct radclock_sms *)clock->ipc_sms;

	if (sms) {
		fprintf(stdout, "Reading SMS:\n");
		fprintf(stdout, " version: %d\n", sms->version);
		fprintf(stdout, " status: %d\n", sms->status);
		fprintf(stdout, " clockid: %d\n", sms->clockid);
		fprintf(stdout, " gen: %u\n", sms->gen);

		fprintf(stdout, "Current data:\n");
		data = clock->ipc_sms + sms->data_off;
		read_raddata(data);

		fprintf(stdout, "Old data:\n");
		data = clock->ipc_sms + sms->data_off_old;
		read_raddata(data);
		return (1);
	} else
		fprintf(stdout, "SMS is down.\n");

	return 0;
}


int
main(int argc, char *argv[])
{
	struct radclock *clock;
	//struct timeval tv;
	long double currtime;
	int smsok, err;

	clock = radclock_create();
	radclock_init(clock);

	smsok = read_sms(clock);

	if (smsok) {
		err = radclock_gettime(clock, &currtime);
		//fprintf(stdout, "UNIX time: %lld.%ld with error code: %d\n",tv.tv_sec, tv.tv_usec, err);
		fprintf(stdout, "UNIX time: %Lf with error code: %d\n", currtime, err);
	}
	
	radclock_destroy(clock);

	return (0);
}

