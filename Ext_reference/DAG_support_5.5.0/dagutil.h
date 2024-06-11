/*
 * LEGAL NOTICE: This source code: 
 * 
 * a) is a proprietary to Endace Technology Limited, a New Zealand company,
 *    and its suppliers and licensors ("Endace"). You must not copy, modify,
 *    disclose or distribute any part of it to anyone else, except as expressly
 *    permitted in the software license agreement provided with this
 *    software, or with the prior written authorisation of Endace Technology
 *    Limited; 
 *   
 * b) may also be part of inventions that are protected by patents and 
 *    patent applications; and   
 * 
 * c) is (C) copyright to Endace, 2013. All rights reserved, except as
 *    expressly granted under the software license agreement referred to
 *    in clause (a) above.
 *
 */


/*
 * Copyright (c) 2002-2005 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: 67b0dde  from: Fri Nov 6 16:36:31 2015 +1300  author: sfd $
 */

#ifndef DAGUTIL_H
#define DAGUTIL_H

/* DAG headers. */
#include "dag_platform.h"
#include "dagnew.h"
#include "dagreg.h"
#include "dagpci.h"
/** \defgroup grpUtility  Utility API
 * DAG Utility API
 */
/*@{*/


/* Macros defined on some platforms but not others. */
#ifndef LINE_MAX
#define LINE_MAX 1024
#endif /* LINE_MAX */

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif /* PATH_MAX */

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif /* O_LARGEFILE */

#ifndef O_DIRECT
#define O_DIRECT 0
#endif /* O_DIRECT */

#ifndef UINT8_MAX
#define UINT8_MAX 255
#endif /* UINT8_MAX */

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif /* UINT16_MAX */


/* SI units. */
#ifndef ONE_KIBI
#define ONE_KIBI (1024)
#endif /* ONE_KIBI */

#ifndef ONE_MEBI
#define ONE_MEBI (1048576)
#endif /* ONE_MEBI */

#ifndef ONE_GIBI
#define ONE_GIBI (1073741824)
#endif /* ONE_GIBI */

#ifndef ONE_TEBI
#define ONE_TEBI (1099511627776ULL)
#endif /* ONE_TEBI */


/** 
 * Useful cross-platform macros. 
 * Prefixed with "dagutil_" to avoid conflicts with client code.
 */
#ifndef dagutil_min
#define dagutil_min(X,Y) ((X)>(Y))?(Y):(X)
#endif /* dagutil_min */

#ifndef dagutil_max
#define dagutil_max(X,Y) ((X)>(Y))?(X):(Y)
#endif /* dagutil_max */

/* Macro to align value of x to a */
#ifndef dagutil_align
#define dagutil_align(x,a)              __dagutil_align_mask(x,(typeof(x))(a)-1)
#define __dagutil_align_mask(x,mask)    (((x)+(mask))&~(mask))
#define dagutil_ptr_align(p, a)         ((typeof(p))dagutil_align((unsigned long)(p), (a)))
#endif  

/**
 * Macro to calculate the number of items in an array
 */
#ifndef dagutil_arraysize
#define dagutil_arraysize(X) ((sizeof(X))/(sizeof(*X)))
#endif /* dagutil_arraysize */

/**
 * Sempahore type returned when the semaphore is created and should be used in
 * all subsequent semaphore accesses.
 *
 */
#if defined(__FreeBSD__) || defined(__linux__) || (defined(__SVR4) && defined(__sun)) || (defined(__APPLE__) && defined(__ppc__))

struct dagutil_sem_s {
	sem_t *sem_p;
	int    named;
	char  *name_p;
};
typedef struct dagutil_sem_s*  dagutil_sem_t;
#define DAGUTIL_INVALID_SEM_TYPE     NULL

#elif defined(_WIN32)

typedef HANDLE  dagutil_sem_t;
#define DAGUTIL_INVALID_SEM_TYPE     NULL

#endif


/**
 * Constant used for the dagutil set of synchronisation functions, as a timeout
 * value that indicates that the function should block until the condition is
 * signaled or there is an error.
 */
#define DAGUTIL_WAIT_FOREVER     (uint32_t)-1




/**
 * Mutex type returned when the mutex object is created and should
 * be used when locking and unlocking a mutex.
 *
 */
#if defined(__FreeBSD__) || defined(__linux__) || (defined(__SVR4) && defined(__sun)) || (defined(__APPLE__) && defined(__ppc__))

typedef pthread_mutex_t* dagutil_mutex_t;
#define DAGUTIL_INVALID_MUTEX_TYPE     NULL

#elif defined(_WIN32)

typedef HANDLE           dagutil_mutex_t;
#define DAGUTIL_INVALID_MUTEX_TYPE     NULL

#endif



/**
 * The function pointer type past to the dagutil_create_thread function as
 * an entry point for the thread.
 */
#if defined(__FreeBSD__) || defined(__linux__) || (defined(__SVR4) && defined(__sun)) || (defined(__APPLE__) && defined(__ppc__))

typedef void*              dagutil_thread_return_t;
typedef pthread_t        dagutil_thread_id_t;
typedef sigset_t         dagutil_sigset_t;
typedef pthread_attr_t   dagutil_thread_attr_t;
#elif defined(_WIN32)

typedef DWORD            dagutil_thread_return_t;
typedef DWORD            dagutil_thread_id_t;

#endif

typedef dagutil_thread_return_t (*dagutil_thread_proc_t)(void*);





/**
 * This type defines dagutil time. It is structure almost identical to the
 * timeval defined by IEEE, except the field names are different and rather
 * than a micosecond field there is a nanosecond field. It has two fields
 * sec and nsec. The actual time value is computed by: sec + (nsec * 10^9)
 *
 */
typedef struct dagutil_time_s
{
	uint32_t  sec;          /**< seconds */
	int32_t   nsec;         /**< nanoseconds */
} dagutil_time_t;


/**
 * Compares two dagutil_time_t values and returns true if they are equal
 * otherwise false is returned.
 *
 * @param[in] x    First dagutil_time_t type to compare.
 * @param[in] y    Second dagutil_time_t type to compare.
 */
#define DAGUTIL_TIME_EQUAL(x, y)     \
	( ((x).sec == (y).sec) && ((x).nsec == (y).nsec) )

/**
 * Compares two dagutil_time_t values and returns true if x is greater than y
 * otherwise false is returned.
 *
 * @param[in] x    First dagutil_time_t type to compare.
 * @param[in] y    Second dagutil_time_t type to compare.
 */
#define DAGUTIL_TIME_GT(x, y)        \
	( ((x).sec > (y).sec) || (((x).sec == (y).sec) && ((x).nsec > (y).nsec)) )

/**
 * Compares two dagutil_time_t values and returns true if x is less than y
 * otherwise false is returned.
 *
 * @param[in] x    First dagutil_time_t type to compare.
 * @param[in] y    Second dagutil_time_t type to compare.
 */
#define DAGUTIL_TIME_LT(x, y)        \
	( ((x).sec < (y).sec) || (((x).sec == (y).sec) && ((x).nsec < (y).nsec)) )

/**
 * Performs a x += y operation.
 *
 * @param[in] x    dagutil_time_t type.
 * @param[in] y    dagutil_time_t type.
 */
#define DAGUTIL_TIME_ADD(x, y)                   \
	do {                                     \
		(x).sec  += (y).sec;             \
		(x).nsec += (y).nsec;            \
		DAGUTIL_TIME_NORMALIZE(x);       \
	} while (0)

/**
 * Performs a x -= y operation.
 *
 * @param[in] x    dagutil_time_t type.
 * @param[in] y    dagutil_time_t type.
 */
#define DAGUTIL_TIME_SUB(x, y)                   \
	do {                                     \
		(x).sec  -= (y).sec;             \
		(x).nsec -= (y).nsec;            \
		DAGUTIL_TIME_NORMALIZE(x);       \
	} while (0)

/**
 * Performs a z = x - y operation, that is it returns the difference between
 * time values.
 *
 * @param[in]  x   dagutil_time_t type.
 * @param[in]  y   dagutil_time_t type.
 * @param[out] z   dagutil_time_t type.
 */
#define DAGUTIL_TIME_DIFF(x, y, z)               \
	do {                                     \
		(z).sec  = (x).sec  - (y).sec;   \
		(z).nsec = (x).nsec - (y).nsec;  \
		DAGUTIL_TIME_NORMALIZE(z);       \
	} while (0)

/**
 * Performs a x = y operation, that is it equates the value of y to x.
 *
 * @param[in] x    dagutil_time_t type.
 * @param[in] y    dagutil_time_t type.
 */
#define DAGUTIL_TIME_SET(x, y)                   \
	do {                                     \
		(x).sec  = (y).sec;              \
		(x).nsec = (y).nsec;             \
	} while (0)

/**
 * Checks if the nsec value exceeds 1 billion (1 second) or is a negative value
 * and if so adds or subtracts 1 from the seconds value and adjusts the nanosecond
 * value accordingly.
 *
 * @param[in] x    dagutil_time_t type.
 */
#define DAGUTIL_TIME_NORMALIZE(x)                                     \
	while ( ((x).nsec >= 1000000000L) || ((x).nsec < 0) ) {       \
		if ((x).nsec >= 1000000000L)                          \
		{ (x).sec++; (x).nsec -= 1000000000L; }               \
		else if ((x).nsec < 0)                                \
		{ (x).sec--; (x).nsec += 1000000000L; }               \
	}




/**
 *  Useful bit constant macros. 
 */
enum
{
	BIT0  = (1<<0),
	BIT1  = (1<<1),
	BIT2  = (1<<2),
	BIT3  = (1<<3),
	BIT4  = (1<<4),
	BIT5  = (1<<5),
	BIT6  = (1<<6),
	BIT7  = (1<<7),
	BIT8  = (1<<8),
	BIT9  = (1<<9),
	BIT10 = (1<<10),
	BIT11 = (1<<11),
	BIT12 = (1<<12),
	BIT13 = (1<<13),
	BIT14 = (1<<14),
	BIT15 = (1<<15),
	BIT16 = (1<<16),
	BIT17 = (1<<17),
	BIT18 = (1<<18),
	BIT19 = (1<<19),
	BIT20 = (1<<20),
	BIT21 = (1<<21),
	BIT22 = (1<<22),
	BIT23 = (1<<23),
	BIT24 = (1<<24),
	BIT25 = (1<<25),
	BIT26 = (1<<26),
	BIT27 = (1<<27),
	BIT28 = (1<<28),
	BIT29 = (1<<29),
	BIT30 = (1<<30),
	BIT31 = (1<<31)
};


typedef struct pbm_offsets
{
	uint32_t globalstatus;  /*  Global status. */
	uint32_t streambase;    /*  Offset of first stream. */
	uint32_t streamsize;    /*  Size of each stream. */
	uint32_t streamstatus;  /*  Control / Status. */
	uint32_t mem_addr;      /*  Low Mem hole base address. */
	uint32_t mem_addr_h;    /*  High Mem hole base address */
	uint32_t mem_size;      /*  Low Mem hole size. */
	uint32_t mem_size_h;    /*  High Mem hole base address. */
	uint32_t record_ptr;    /*  Low Record pointer. */
	uint32_t record_ptr_h;   /*  High Record pointer. */
	uint32_t limit_ptr;     /*  Low Limit pointer. */
	uint32_t limit_ptr_h;   /*  High Limit pointer. */
	uint32_t safetynet_cnt; /*  At limit event pointer. */
	uint32_t drop_cnt;      /*  Drop counter. */
	
} pbm_offsets_t;

/**
Maximum memory that can be allocated to a single stream in MiB
 */
#define PBM_MAX_SMALLSTREAM_SIZE_MIB 2044
#define PBM_MAX_STREAM_SIZE_MIB 65532
/**
 * This part controls fan and thermal information
 */
enum
{
	LM63_TEMP_LOC   = 0x00,
	LM63_TEMP_REM_H = 0x01,
	LM63_CONF       = 0x03,
	LM63_LOC_HIGH   = 0x05,
	LM63_REM_HIGH   = 0x07,
	
	LM63_ALERT_MASK = 0x16,
	
	LM63_PWM_RPM    = 0x4a,
	LM63_SPINUP     = 0x4b,
	LM63_PWM_VALUE  = 0x4c,

	LM63_LOOKUP_T1  = 0x50,
	LM63_LOOKUP_P1  = 0x51,
	LM63_LOOKUP_T2  = 0x52,
	LM63_LOOKUP_P2  = 0x53,
	LM63_LOOKUP_T3  = 0x54,
	LM63_LOOKUP_P3  = 0x55,
	LM63_LOOKUP_T4  = 0x56,
	LM63_LOOKUP_P4  = 0x57,
	LM63_LOOKUP_T5  = 0x58,
	LM63_LOOKUP_P5  = 0x59,
	LM63_LOOKUP_T6  = 0x5a,
	LM63_LOOKUP_P6  = 0x5b,
	LM63_LOOKUP_T7  = 0x5c,
	LM63_LOOKUP_P7  = 0x5d,
	LM63_LOOKUP_T8  = 0x5e,
	LM63_LOOKUP_P8  = 0x5f,
	
	LM63_REM_TEMP_FILTER = 0xbf,
	
	LM63 = 0x98
};

typedef enum
{
   /*Represents the bit positions of ERF classes -As defined by stream feature ICD */
   kErfClassEthernet = 0,
   kErfClassHDLC = 1,
   kErfClassSonetChannelised = 2,
   kErfClassInfiniband = 3,
   kErfClassUndefined,
   kLastErfClassType = kErfClassUndefined/* undefined */
}erf_class_t;

/** Consistent release version string. */
extern const char* const kDagReleaseVersion;

/** Consistent release version string "debug", "production". */
extern const char* const kDagBuildType;

/** Compiler flags string. */
extern const char* const kDagCompilerFlags;


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * WARNING: routines in the dagutil module are provided for convenience
 *          and to promote code reuse among the dag* tools.
 *          They are subject to change without notice.
 */


/**\defgroup grpMiscellaneousFuncs Miscellaneous Functions. 
 */
/*@{*/
/**
 * Set the program name
 *
 * @param[in] program_name the string of program name
 *
 * @return		NONE
 */
void dagutil_set_progname(const char* program_name);
/** 
 * Get the name of program
 *
 * @return		the sting of program name
 *
 */
const char* dagutil_get_progname(void);

/**
 * Set up signal handler.
 * Catching most common signals: SIGHUP, SIGINT, SIGTERM, SIGPIPE and SIGQUIT on Unix like systems and;
 * SIGINT and SIGTERM on Windows systems to provide
 * for a graceful shutdown of the card or application, worst case the driver will have
 * to clean up after a crash.
 *
 * @param[in] handler	a specific signal handler
 *
 * @return		NONE
 */
void dagutil_set_signal_handler(void (*handler)(int));

/**
 * Set up time signal handler. Set up a timer to expire every seconds 
 * for reporting.
 *
 * @param[in] handler	a specific signal handler
 *
 * @return		NONE
 *
 */
void dagutil_set_timer_handler(void (*handler)(int), uint32_t seconds);
/*@}*/

/**\defgroup grpOutFuncs Output functions. 
 */
/*@{*/
/**
 * (DEPRECATED)
 * set the program verbosity
 */
void dagutil_set_verbosity(int v);
int dagutil_get_verbosity(void);
/**
 * Used to increase the verbosity level
 */
void dagutil_inc_verbosity(void);
/**
 * Used to decrease the verbosity level
 */
void dagutil_dec_verbosity(void);
/**
 * Generate a panic message to system.
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_panic(const char* fmt, ...) __attribute__((noreturn, format (printf, 1, 2)));
/**
 * Generate a error message to system
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_error(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
/**
 * Generate an error message to system if debugging enabled
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_debug_error(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
/**
 * Generate a message to system if debugging enabled
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_debug_msg(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
/**
 * Generate a warning message to system
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_warning(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
/**
 * Print out verbose message to system
 *
 * @param[in] fmt	the format is a character string
 *
 * @return 		NONE
 */
void dagutil_verbose(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
/**
 * Print out verbose message to system with specified verbosity level
 *
 * @param[in] fmt	the format is a character string
 * @param[in] level	the level of verbosity
 *
 * @return 		NONE
 */
void dagutil_verbose_level(uint32_t level, const char* fmt, ...) __attribute__((format (printf, 2, 3)));
/**
 * Print out user message to specified stream (or syslog) with specified verbosity level
 *
 * @param[in] fmt	the format is a character string
 * @param[in] stream	the stream to print out to
 * @param[in] level	the level of verbosity
 *
 * @return 		NONE
 */
void dagutil_msg_level(uint32_t level, FILE *stream, const char* fmt, ...) __attribute__((format (printf, 3, 4)));
/**
 * Open a connection to the system logger for a program.
 *
 * @param[in] option	specifies flags which control the operation
 * @param[in] facility	establishes a default to be used if none is 
 * 			specified in  subsequent  calls  to  syslog()
 *
 * @return		NONE
 */
void dagutil_daemon(int option, int facility);
/*@}*/

/**\defgroup grpSleepFuncs Sleeping functions. 
 */
/*@{*/
/** 
 * Get wake time. wake time is a tsc value.
 *
 * @param[in] wake_time	a tsc value
 * @param[in] microseconds
 *
 * @return		NONE 
 */
void dagutil_sleep_get_wake_time(unsigned long long* wake_time, int microseconds);
/**
 * Suspends execution of the calling process until reach specified 
 * value wake time.
 * 
 * @param[in] wake_time	a tsc value
 *
 * @return		NONE
 */
void dagutil_sleep_until(unsigned long long wake_time);
/**
 * Same as dagutil_sleep_until() but does not busywait.
 * CPU usage will be considerably lower, but accuracy MUCH worse!
 * usleep(1) should cause a sleep of at least 1usec,
 * generally equal to system scheduler time unit (typ 10ms).
 *
 * @param[in] wake_time	a tsc value
 *
 * @return 		NONE
 */
void dagutil_sleep_until_nobusy(unsigned long long wake_time);
/**
 * Supends execution for microsecond intervals
 *
 * @param[in] microseconds time interval in microsecond
 *
 * @return		NONE
 */
void dagutil_microsleep(uint32_t microseconds);
/**
 * Supends execution for nanosecond intervals
 *
 * @param[in] nanoseconds time interval in nanosecond
 *
 * @return		NONE
 */
void dagutil_nanosleep(uint32_t nanoseconds);
/**
 * read 64 bits tsc value 
 *
 * @param[in] tsc	the pointer to a 64 bits tsc value
 *
 * @return 		NONE
 */
void dagutil_tsc64_read(uint64_t* tsc);
/**
 * read 32 bits tsc value 
 *
 * @param[in] tsc	the pointer to a 32 bits tsc value
 *
 * @return 		NONE
 */
void dagutil_tsc32_read(uint32_t* tsc);
/*@}*/

/**\defgroup grpTimmingFuncs Timming Functions. 
 */
/*@{*/
int dagutil_get_time(dagutil_time_t *time_p);
/*@}*/

/**\defgroup grpSyncFuncs Synchronisation Functions 
 */
/*@{*/
int dagutil_semaphore_init (dagutil_sem_t *sem_p, int initial, const char *name);
int dagutil_semaphore_take (dagutil_sem_t sem, uint32_t timeout);
int dagutil_semaphore_give (dagutil_sem_t sem);
int dagutil_semaphore_delete (dagutil_sem_t sem);
int dagutil_mutex_init (dagutil_mutex_t *mutex_p, int initial);
int dagutil_mutex_lock (dagutil_mutex_t mutex, uint32_t timeout);
int dagutil_mutex_unlock (dagutil_mutex_t mutex);
int dagutil_mutex_delete (dagutil_mutex_t mutex);
/*@}*/

/**\defgroup grpThreadingFuncs Threading Functions
 */
/*@{*/
int dagutil_thread_attr_init(dagutil_thread_attr_t *attr);
int dagutil_thread_attr_destroy(dagutil_thread_attr_t *attr);
int dagutil_thread_attr_setschedpolicy(dagutil_thread_attr_t *attr,int policy);
int dagutil_thread_attr_setdetachstate(dagutil_thread_attr_t *attr,int detached_state);
int dagutil_thread_create (dagutil_thread_proc_t proc, void *arg, dagutil_thread_id_t *id_p,dagutil_thread_attr_t *attr);
void dagutil_thread_exit( void *retval);
int dagutil_thread_join(dagutil_thread_id_t thread_id,void **retval);
int dagutil_thread_kill (dagutil_thread_id_t id);
int dagutil_thread_get_id (dagutil_thread_id_t *id_p);
int dagutil_thread_mask_signal(dagutil_sigset_t *set,dagutil_sigset_t *oldset);
int dagutil_thread_unmask_signal(dagutil_sigset_t *set,dagutil_sigset_t *oldset);
int dagutil_sigemptyset(dagutil_sigset_t *set);
int dagutil_sigfillset(dagutil_sigset_t *set);
int dagutil_sigaddset(dagutil_sigset_t *set,int signum);
int dagutil_sigdelset(dagutil_sigset_t *set,int signum);
int dagutil_sigismember(dagutil_sigset_t *set,int signum);
/*@}*/


/**\defgroup grpIOFuncs I/O Functions */
/*@{*/
/**
 * Read a file
 *
 * @param[in] filename	Input file name
 * @param[in] flags	The mode of stream
 * @param[out] out_length The length of file
 *
 * @return		The pointer to the buffer
 */
void* dagutil_readfile(char *filename, char *flags, unsigned int* out_length);
/*@}*/

/**\defgroup grpMemFuncs Hooks for memory allocation/free Functions
 */
/*@{*/
/**
 * Wrappers for malloc function. This function is used to allocate memory
 * 
 * @param[in] size	allocates size bytes
 *
 * @return		return a pointer to the allocated memory.On error, 
 * 			these  functions  return  NULL.
 */
void* dagutil_malloc(size_t size);

/**
 * Wrappers for malloc function. This function is used to allocate memory uses posix memalign.
 * 
 * @param[in] size	allocates size bytes
 *
 * @return		return a pointer to the allocated memory.On error, 
 * 			these  functions  return  NULL.
 */
void* dagutil_malloc_aligned(size_t size);

/** Wrappers for free function. frees the memory space pointed to by ptr, 
 *  which must have been returned by a previous call to malloc(). Otherwise, 
 *  or if free(ptr) has already been called before, undefined behavior occurs. 
 *  
 *  @param[in] ptr	pointer to memory space
 *
 *  @return 		NONE
 */
void dagutil_free(void* ptr);

/** Wrappers for free function. frees the memory space pointed to by ptr, 
 *  which must have been returned by a previous call to malloc(). Otherwise, 
 *  or if free(ptr) has already been called before, undefined behavior occurs. 
 *  
 *  @param[in] ptr	pointer to memory space
 *
 *  @return 		NONE
 */
void dagutil_free_aligned(void *ptr);
/**
 * Wrappers for strdup for duplicate a string.
 *
 * @param[in] source	the string need to duplicate
 *
 * @return		a pointer to a new string
 */
const char* dagutil_strdup(const char* source);
/*@}*/


/**\defgroup grpdupFuncs Common Functions 
 *  Routines that are common to multiple applications
 */
/*@{*/
pbm_offsets_t* dagutil_get_pbm_offsets(void);
int dagutil_resetduck(int dagfd);
int dagutil_lockstream(int dagfd, int stream);
int dagutil_unlockstream(int dagfd, int stream);
int dagutil_lockallstreams(int dagfd);
void dagutil_unlockallstreams(int dagfd);
uint32_t dagutil_iomread(volatile uint8_t* iom, uint32_t addr);
void dagutil_iomwrite(volatile uint8_t* iom, uint32_t addr, uint32_t value);
void dagutil_steerstatus(int dagfd, volatile uint8_t* dagiom, uint32_t base, int ver);

/**
 * reset the PCI burst data path 
 *
 * @param[in] iom	The IOM for the card
 *
 * @return		NONE
 */
void dagutil_reset_datapath(volatile uint8_t* iom);

/**
 * Translate a physical line to a logical line number.  This was duplicated in several places 
 * too: dagbits, dagpartum (twice). It's still in the dag37t_api library.
 * 
 * @param[in] uline	The physical line this connection is to be made on
 *
 * @return		The number of logical interface
 */
uint32_t dagutil_37t_line_get_logical(uint32_t uline);
/*@}*/

/**\defgroup grpGenUtil General utilities Functions
 */
/*@{*/
/**
 * Utility function that performs the same functionality as snprintf, however it
 * concatenates the string in the printf format part onto the end of any existing
 * stext in the string. This is equivalent to this:
 * 
 * snprintf (new_string, (n - strlen(string)), fmt, ...);
 * strncat (string, new_string, n);
 * 
 * But all in one function and with more error checking.
 *     
 * @param[in,out]  str     The buffer that gets the new string concatenated onto it.
 * @param[in]      size    The total size of the buffer in characters.
 * @param[in]      fmt     The format string.
 * @param[in]      ...     Variable number of arguments.
 *
 * @returns                On success, returns the number of characters that would have
 * 			   been written had size been sufficiently large, not counting
 *                         the terminating nul character.
 */
int dagutil_snprintfcat(char *str, size_t size, const char *fmt, ...) __attribute__((format (printf, 3, 4)));

/** In dagutil_snprintf_scale_num() use binary power units, E.g. Mi instead of M
 */
#define DAGUTIL_PRINTF_BINARY 0x01
/** In dagutil_snprintf_scale_num() put a space between number and units
 */
#define DAGUTIL_PRINTF_SPACE  0x02

/**
 * Utility function tfor printing numbers with automatic scaling.
 * SI units are used from Atto to Exa. Micro is printed as 'u'.
 *
 * But all in one function and with more error checking.
 *
 * @param[in,out]  str     The buffer that gets the new string printed onto it.
 * @param[in]      size    The total size of the buffer in characters.
 * @param[in]      value   Number to scale.
 * @param[in]      width   Width of numeric field, see printf(3)
 * @param[in]      precision Precision of numeric field, see printf(3)
 * @param[in]      flags   DAGUTIL_PRINTF_* flags
 *
 * @returns                On success, returns the number of characters printed
 *
 */
int dagutil_snprintf_scale_num(char *str, size_t size, double value, int width, int precision, int flags);

/**
 * Utility function to subtract two struct timespec
 * 
 * @param[in]      a       First value
 * @param[in]      b       Second value
 * @param[in]      result  First value minus second value 
 *
 */
void
dagutil_timespec_sub(struct timespec *a, struct timespec *b, struct timespec *result);

/**
 * Utility function to return the number of seconds elasped between two timespec
 * 
 * @param[in]      a       First value
 * @param[in]      b       Second value
 *
 * @returns                Time a minus Time b in seconds
 *
 */
double
dagutil_timespec_difftime(struct timespec *a, struct timespec *b);

/**
 * Utility function to add two struct timespec
 * 
 * @param[in]      a       First value
 * @param[in]      b       Second value
 * @param[in]      result  First value plus second value 
 *
 */
void
dagutil_timespec_add(struct timespec *a, struct timespec *b, struct timespec *result);

/**
 * Utility function to test if a struct timespec is non-zero
 * 
 * @param[in]      a       Value to test
 * 
 * @returns                Returns true if value is non-zero
 */
int
dagutil_timespec_isset(struct timespec *a);

/**
 * Utility function to clear a timespec
 * 
 * @param[in]      a       Value to clear
 *
 */
void
dagutil_timespec_clear(struct timespec *a);

/**
 * Convert an ERF record timestamp into a struct timespec
 * @param erfts pointer to ERF timestamp
 * @param ts struct timespec result
 * @return 0 on success or -1 on failure
 */
int
dagutil_erfts_to_timespec(uint64_t * erfts, struct timespec *ts);

/**
 * Convert a struct timespec into an ERF timestamp
 * @param ts struct timespec
 * @param erfts pointer to ERF timestamp for result
 * @return 0 on success or -1 on failure
 */
int
dagutil_timespec_to_erfts(struct timespec *ts, uint64_t * erfts);

/*@}*/

erf_class_t erf_types_to_erf_class(uint32_t erf_types);

uint32_t erf_class_to_erf_types(erf_class_t erf_class);

/**
 * Trims a string 
 * " aaa a " -> "aaa a"
 * 
 * @param[str] the string to be trimed
 *
 * @return NONE
*/
void dagutil_trim_string(char* str);

/**
	Converts a broken-down UTC time to simple calendar time (epoch).
	/param	utc_tm	UTC time.

	@return epoch time.

	@sa timegm()
	@sa _mkgmtime() in Windows.
*/
time_t dagutil_mkutctime (struct tm *utc_tm); 

/**
 * portable flexitime implementation
 * for parsing and coverting user defined date/time strings to seconds since unix epoch utc
 *
 * Input date/time string can be anything like:
 *     [[YYYY-]MM-DD | MMM/DD[st|nd|th] | DD[st|nd|th]/MMM | MMM DD[st|nd|th] | DD[st|nd|th][MMM] | tomorrow] [@] hh[:mm[:ss]] [AM|PM] [UTC]
 * or, can be a delay like
 *     N Seconds|Minutes|Hours|Days|Weeks
 */
typedef const char* flexitime_dtstring_t;
typedef int flexitime_result_t;

flexitime_result_t dagutil_flexitime_to_timestamp(flexitime_dtstring_t datetimestring, uint32_t *timestamp_utc);
const char* dagutil_flexitime_error_to_text(flexitime_result_t result);

#define FLEXITIME_RESULT_SUCCESS 0
#define FLEXITIME_RESULT_FAILURE -1
#define FLEXITIME_RESULT_UNKNOWNDELAYFACTOR -2
#define FLEXITIME_RESULT_MISSINGDELAYFACTOR -3
#define FLEXITIME_RESULT_UNKNOWNRELATIVEDATE -4
#define FLEXITIME_RESULT_BADSTRUCTM -5
#define FLEXITIME_RESULT_TOOMANYTOKENS -6
#define FLEXITIME_RESULT_UNEXPECTEDDATETOKEN -7
#define FLEXITIME_RESULT_BADDATE -8
#define FLEXITIME_RESULT_BADYEAR -9
#define FLEXITIME_RESULT_BADMONTH -10
#define FLEXITIME_RESULT_BADDAY -11
#define FLEXITIME_RESULT_MISSINGTIME -12
#define FLEXITIME_RESULT_UNDEFINED -13
#define FLEXITIME_RESULT_FAILEDNORMALISATION -14
#define FLEXITIME_RESULT_INPUTTOOLONG -15

/**
 * Utility function to portably return the system physical memory size
 * 
 * @returns                Size of physical memory in bytes
 */
size_t dagutil_get_physmem_size(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAGUTIL_H */ 
/*@}*/
