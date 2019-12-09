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



/*
 * This program illustrate the use of functions related to time access and
 * defined in the RADclock API. These functions:
 * - retrieve the RADclock internal parameters
 * - access the RAW vcounter
 * - give access to absolute time based on the RADclock
 * - give access to difference time baed on the RADclock.
 *
 * The RADclock daemon should be running for this example to work correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

/* Needed for accessing the RADclock API */
#include <radclock.h>
/* needed to access types for printout_{rad,FF}data */
#include "radclock-private.h"		// needed for struct radclock_data 		defn
#include "kclock.h"              //            struct ffclock_estimate




int
main (int argc, char *argv[])
{
	/* RADclock */
	struct radclock *clock;

	/* Clock parameters */
	unsigned int status;
	double period;
	double period_error;
	long double offset;
	double offset_error;

	/* Time data structure */
	long double currtime;
	time_t currtime_t;

	/* Raw vcounter stamps */
	vcounter_t vcount1, vcount2, vcount3;   //

	int j;
	int err = 0;


	/* Initialize the clock handle */
	clock = radclock_create();
	if (!clock) {
		fprintf(stderr, "Could not create clock handle");
		return (-1);
	}
	printf("----------------------------------------------------------------\n");
	radclock_init(clock);	// initializes shm in here


	/* radclock_get_*
	 *
	 * Functions to get   some information regarding the radclock itself
	 */
	printf("----------------------------------------------------------------\n");
	err += radclock_get_status(clock, &status);
	err += radclock_get_last_changed(clock, &vcount1);
	err += radclock_get_next_expected(clock, &vcount3);
	err += radclock_get_period(clock, &period);
	err += radclock_get_bootoffset(clock, &offset);
	err += radclock_get_period_error(clock, &period_error);
	err += radclock_get_bootoffset_error(clock, &offset_error);

	if ( err != 0 ) {
		printf ("At least one of the calls to the radclock failed. Giving up.\n");
		return (-1);
	}

	printf("----------------------------------------------------------------\n");
	printf("Get some information about the clock parameters: \n");
	printf(" - Clock status: \t\t %u\n", status);
	printf(" - Clock last update (vcount value): \t\t%llu\n", (long long unsigned)vcount1);
	printf(" - Clock next expected update (vcount): \t%llu\n", (long long unsigned)vcount3);
	printf(" - Clock oscillator period: \t%15.9lg \n", period);
	printf(" - Clock oscillator period error: \t%15.9lg \n", period_error);
	printf(" - Clock boot offset (ca): \t%22.9Lf\n", offset);
	printf(" - Clock offset error: \t%15.9lf [ms]\n", 1000.0*offset_error);

	printf("----------------------------------------------------------------\n");


//	struct radclock_data rad_data;
	struct ffclock_estimate cest;
	struct radclock_shm *shm;

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		printout_raddata(SHM_DATA(shm));
	} else
		printf("SHM is down, can''t print daemon''s raddata\n\n");
	
	if (get_kernel_ffclock(clock, &cest))
		printf("kernel FFdata unreachable, can''t print it\n\n");
	else {
		// fill_radclock_data(&cest, &rad_data);
		printout_FFdata(&cest);
		printf("\n\n");
	}
	
	

	/* radclock_get_vcounter
	 *
	 * Quick test to check the routine can access the RAW vcounter
	 */
	err = radclock_get_vcounter(clock, &vcount1);
	printf("Initial vcounter reading is %llu   error = %d\n", (long long unsigned)vcount1, err);
	
	for ( j=0; j<3; j++ ) {
		err = radclock_get_vcounter(clock, &vcount2);
		radclock_duration(clock, &vcount1, &vcount2, &currtime);
		printf(" Delta(vcount) from previous vcount = %llu (%9.4Lg [ms]) error=%d\n",
				(long long unsigned)(vcount2 - vcount1), currtime * 1e3, err);
		vcount1 = vcount2;
	}
	printf("\n");


	/* radclock_gettimeofday
	 *
	 * This uses the absolute RADclock, and passes back a long double,
	 * the resolution depends on the selected oscillator frequency and
	 * the definition of a long double on your architecture
	 */
	printf("----------------------------------------------------------------\n");
	printf("Calling the RADclock equivalent to gettimeofday with possibly "
			"higher resolution'\n");
	err = radclock_gettime(clock, &currtime);
	currtime_t = (time_t) currtime;
	printf(" - radclock_gettimeofday now: %s   (UNIX time: %12.20Lf)\n",
			ctime(&currtime_t), currtime);


	/* radclock_vcount_to_abstime and radclock_vcount_to_abstime
	 *
	 * This allows to quickly read the counter, store the value and
	 * convert it to time information later on.
	 * Since the RADclock is updated every poll_period seconds, the conversion
	 * should be done within that interval.
	 */
	printf("----------------------------------------------------------------\n");
	err = radclock_get_vcounter(clock, &vcount1);
	printf("Reading a vcount value now: %llu\n", (long long unsigned)vcount1);

	err = radclock_vcount_to_abstime(clock, &vcount1, &currtime);
	currtime_t = (time_t) currtime;
	printf("   converted to long double: %s   (UNIX time: %12.20Lf)\n",
			ctime(&currtime_t), currtime);


	/* radclock_elapsed
	 *
	 * These take advantage of the stability of the difference RADclock. These
	 * are the function to use to measure time intervals over a short time scale
	 * between a past event and now.
	 */
	printf("----------------------------------------------------------------\n");
	err = radclock_get_vcounter(clock, &vcount1);
	printf("Reading a vcount value now: %llu ", (long long unsigned)vcount1);
	printf(" - have a little rest for .2 seconds...\n");
	sleep(.2);
	err = radclock_elapsed(clock, &vcount1, &currtime);
	printf("   radclock_elapsed says we have been sleeping for %12.20Lf [ms]\n", 1000*currtime);


	/* radclock_duration
	 *
	 * These take advantage of the stability of the difference RADclock. These
	 * are the function to use to measure time intervals over a short time scale
	 * between two events.
	 */
	printf("----------------------------------------------------------------\n");
	err = radclock_get_vcounter(clock, &vcount1);
	printf("Reading a vcount value now:  %llu ", (long long unsigned)vcount1);
	printf(" - have a little rest for .2 seconds...\n");
	sleep(.2);
	err = radclock_get_vcounter(clock, &vcount2);
	printf("Reading a second vcount now: %llu \n", (long long unsigned)vcount2);
	err = radclock_duration(clock, &vcount1, &vcount2, &currtime);
	printf(" - radclock_duration says we have been sleeping for %12.20Lf [ms]\n", 1000*currtime);

	return (0);
}

