/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define the first radclock specific ioctl number (freebsd specific) */
/* #undef FREEBSD_RADCLOCK_IOCTL */

/* Define if clock_gettime is available (rt library) */
/* #undef HAVE_CLOCK_GETTIME */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `nl' library (-lnl). */
/* #undef HAVE_LIBNL */

/* Define to 1 if you have the `pcap' library (-lpcap). */
#define HAVE_LIBPCAP 1

/* Define to 1 if you have the `rt' library (-lrt). */
/* #undef HAVE_LIBRT */

/* Define to 1 if you have the <linux/genetlink.h> header file. */
/* #undef HAVE_LINUX_GENETLINK_H */

/* Define to 1 if you have the <linux/netlink.h> header file. */
/* #undef HAVE_LINUX_NETLINK_H */

/* Define to 1 if you have the <machine/cpufunc.h> header file. */
/* #undef HAVE_MACHINE_CPUFUNC_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the `mkstemps' function. */
#define HAVE_MKSTEMPS 1

/* Define to 1 if you have the <netlink/netlink.h> header file. */
/* #undef HAVE_NETLINK_NETLINK_H */

/* Define to 1 if you have the `pcap_activate' function. */
#define HAVE_PCAP_ACTIVATE 1

/* Define if POSIX timer available (rt library) */
/* #undef HAVE_POSIX_TIMER */

/* Define to 1 if you have rdtsc() */
/* #undef HAVE_RDTSC */

/* Define to 1 if rdtscll() in asm/msr.h */
/* #undef HAVE_RDTSCLL_ASM */

/* Define to 1 if rdtscll() in asm-x86/msr.h */
/* #undef HAVE_RDTSCLL_ASM_X86 */

/* Define to 1 if rdtscll() in asm-x86_64/msr.h */
/* #undef HAVE_RDTSCLL_ASM_X86_64 */

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/timeffc.h> header file. */
/* #undef HAVE_SYS_TIMEFFC_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define get_vcounter_syscall number (Linux specific) */
/* #undef LINUX_SYSCALL_GET_VCOUNTER */

/* Define get_vcounter_latency_syscall number (Linux specific) */
/* #undef LINUX_SYSCALL_GET_VCOUNTER_LATENCY */

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "radclock"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "julien@synclab.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "radclock"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "radclock v0.4.x-298-g2726181"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "radclock"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.synclab.org/radclock/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "v0.4.x-298-g2726181"

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long long int', as computed by sizeof. */
#define SIZEOF_LONG_LONG_INT 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "v0.4.x-298-g2726181"

/* Define to 1 if detected patched radclock kernel */
/* #undef WITH_FFKERNEL_FBSD */

/* Define to 1 if detected patched radclock kernel */
/* #undef WITH_FFKERNEL_LINUX */

/* Define to 1 if NOT detected patched radclock kernel */
#define WITH_FFKERNEL_NONE 1

/* Define to 1 if with extra debug output */
/* #undef WITH_JDEBUG */

/* nl_recv takes 4 params */
/* #undef WITH_NL_RECV_FOUR_PARM */

/* Define to 1 if compiling with xenstore */
/* #undef WITH_XENSTORE */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
