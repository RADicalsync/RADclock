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

#ifndef _MISC_H
#define _MISC_H

// TODO: this file is poorly named

/* These don't exist in the standard math library
 * Careful:  the sign of arguments is not tested, so if given an unsigned
 * argument that rolls, they may return the wrong result.
 */
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/*
 * NTP timestamp fields in NTP timescale&format, need UTC <--> NTP conversions.
 *   epoch:      (NTP,UTC) = Jan 1 (1900,1970)        NTP = UTC + JAN_1970
 *   frac format:(NTP,tvalUTC) = (#(2^-32)s, #mus)    NTP = UTC*1e-6 *2^32
 */
static inline void
UTCld_to_NTPtime(long double *time, l_fp *NTPtime)
{
	NTPtime->l_int = (uint32_t) *time;
	uint64_t frac = (uint64_t)(4294967296.0L * (*time - NTPtime->l_int) + 0.5L);
	if (frac >= 4294967296ULL) {  // would overflow u32, so set result manually
		NTPtime->l_int += 1;
		frac = 0;
	}
	NTPtime->l_int += JAN_1970;
	NTPtime->l_fra = (uint32_t)frac;
}

static inline void
UTCtimeval_to_NTPtime(struct timeval *tv, l_fp *NTPtime)
{
	NTPtime->l_int = (uint32_t) tv->tv_sec + JAN_1970;
	// Can't overflow u32 even in worst case thanks to usec granularity
	NTPtime->l_fra = (uint32_t)(4294967296.0 * (double)tv->tv_usec / 1000000 + 0.5);
}

static inline long double
NTPtime_to_UTCld(l_fp ntp_ts)
{
	long double time;
	time  = (long double)(ntohl(ntp_ts.l_int) - JAN_1970);
	time += (long double)(ntohl(ntp_ts.l_fra)) / 4294967296.0;
	return (time);
}

static inline void
ld_to_timeval(long double *time, struct timeval *tv)
{
	tv->tv_sec  = (uint32_t) *time;
	tv->tv_usec = (uint32_t) (1000000*(*time - tv->tv_sec) + 0.5);
}

static inline void
timeval_to_ld(struct timeval *tv, long double *time)
{
	*time  = (long double) tv->tv_sec;
	*time += (long double)(tv->tv_usec) / 1000000.0;
}


/* Subtract two timeval */
static inline void
subtract_tv(struct timeval *delta, struct timeval tv1, struct timeval tv2)
{
	int nsec;

	/* Perform the carry */
	if (tv1.tv_usec < tv2.tv_usec) {
		nsec = (tv2.tv_usec - tv1.tv_usec) / 1000000 + 1;
		tv2.tv_usec -= 1000000 * nsec;
		tv2.tv_sec += nsec;
	}
	if (tv1.tv_usec - tv2.tv_usec > 1000000) {
		nsec = (tv1.tv_usec - tv2.tv_usec) / 1000000;
		tv2.tv_usec += 1000000 * nsec;
		tv2.tv_sec -= nsec;
	}

	/* Subtract */
	delta->tv_sec = tv1.tv_sec - tv2.tv_sec;
	delta->tv_usec = tv1.tv_usec - tv2.tv_usec;
}


#endif
