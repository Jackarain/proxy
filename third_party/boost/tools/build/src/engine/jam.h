/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/* This file is ALSO:
 * Copyright 2001-2004 David Abrahams.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE.txt or copy at
 * https://www.bfgroup.xyz/b2/LICENSE.txt)
 */

/*
 * jam.h - includes and globals for jam
 */

#ifndef JAM_H_VP_2003_08_01
#define JAM_H_VP_2003_08_01

#include "config.h"

/* Assume popen support is available unless known otherwise. */
#define HAVE_POPEN 1

/*
 * VMS, OPENVMS
 */

#ifdef VMS

#include <ctype.h>
#include <file.h>
#include <signal.h>
#include <stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <types.h>
#include <unistd.h>
#include <unixlib.h>

#define OSMINOR "OS=VMS"
#define OSMAJOR "VMS=true"
#define OS_VMS
#define MAXLINE 1024 /* longest 'together' actions */
#define PATH_DELIM '/' /* use CRTL POSIX-style handling */
#define SPLITPATH ','
#define EXITOK EXIT_SUCCESS
#define EXITBAD EXIT_FAILURE
#define DOWNSHIFT_PATHS

/* This may be inaccurate. */
#ifndef __DECC
#define OSPLAT "OSPLAT=VAX"
#endif

#define glob jam_glob /* use jam's glob, not CRTL's */

#endif

/*
 * Windows NT
 */

#ifdef NT

#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#ifndef __MWERKS__
#include <memory.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OSMAJOR "NT=true"
#define OSMINOR "OS=NT"
#define OS_NT
#define SPLITPATH ';'
#define MAXLINE (undefined__see_execnt_c) /* max chars per command line */
#define USE_EXECNT
#define USE_PATHNT
#define PATH_DELIM '\\'

/* AS400 cross-compile from NT. */

#ifdef AS400
#undef OSMINOR
#undef OSMAJOR
#define OSMAJOR "AS400=true"
#define OSMINOR "OS=AS400"
#define OS_AS400
#endif

/* Metrowerks Standard Library on Windows. */

#ifdef __MSL__
#undef HAVE_POPEN
#endif

#endif /* #ifdef NT */

/*
 * Windows MingW32
 */

#ifdef MINGW

#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OSMAJOR "MINGW=true"
#define OSMINOR "OS=MINGW"
#define OS_NT
#define SPLITPATH ';'
#define MAXLINE 996 /* max chars per command line */
#define USE_EXECUNIX
#define USE_PATHNT
#define PATH_DELIM '\\'

#endif /* #ifdef MINGW */

/*
 * God fearing UNIX.
 */

#ifndef OSMINOR

#define OSMAJOR "UNIX=true"
#define USE_EXECUNIX
#define USE_FILEUNIX
#define USE_PATHUNIX
#define PATH_DELIM '/'

#ifdef _AIX
#define unix
#define MAXLINE 23552 /* 24k - 1k, max chars per command line */
#define OSMINOR "OS=AIX"
#define OS_AIX
#define NO_VFORK
#endif
#ifdef AMIGA
#define OSMINOR "OS=AMIGA"
#define OS_AMIGA
#endif
#ifdef __BEOS__
#define unix
#define OSMINOR "OS=BEOS"
#define OS_BEOS
#define NO_VFORK
#endif
#ifdef __bsdi__
#define OSMINOR "OS=BSDI"
#define OS_BSDI
#endif
#if defined(COHERENT) && defined(_I386)
#define OSMINOR "OS=COHERENT"
#define OS_COHERENT
#define NO_VFORK
#endif
#if defined(__cygwin__) || defined(__CYGWIN__)
#define OSMINOR "OS=CYGWIN"
#define OS_CYGWIN
#endif
#if defined(__FreeBSD__) && !defined(__DragonFly__)
#define OSMINOR "OS=FREEBSD"
#define OS_FREEBSD
#endif
#ifdef __DragonFly__
#define OSMINOR "OS=DRAGONFLYBSD"
#define OS_DRAGONFLYBSD
#endif
#ifdef __DGUX__
#define OSMINOR "OS=DGUX"
#define OS_DGUX
#endif
#ifdef __GNU__
#define OSMINOR "OS=HURD"
#define OS_HURD
#endif
#ifdef __hpux
#define OSMINOR "OS=HPUX"
#define OS_HPUX
#endif
#ifdef __HAIKU__
#define unix
#define OSMINOR "OS=HAIKU"
#define OS_HAIKU
#endif
#ifdef __OPENNT
#define unix
#define OSMINOR "OS=INTERIX"
#define OS_INTERIX
#define NO_VFORK
#endif
#ifdef __sgi
#define OSMINOR "OS=IRIX"
#define OS_IRIX
#define NO_VFORK
#endif
#ifdef __ISC
#define OSMINOR "OS=ISC"
#define OS_ISC
#define NO_VFORK
#endif
#if defined(linux) || defined(__linux) || defined(__linux__) \
	|| defined(__gnu_linux__)
#define OSMINOR "OS=LINUX"
#define OS_LINUX
#endif
#ifdef __Lynx__
#define OSMINOR "OS=LYNX"
#define OS_LYNX
#define NO_VFORK
#define unix
#endif
#ifdef __MACHTEN__
#define OSMINOR "OS=MACHTEN"
#define OS_MACHTEN
#endif
#ifdef mpeix
#define unix
#define OSMINOR "OS=MPEIX"
#define OS_MPEIX
#define NO_VFORK
#endif
#ifdef __MVS__
#define unix
#define OSMINOR "OS=MVS"
#define OS_MVS
#endif
#ifdef _ATT4
#define OSMINOR "OS=NCR"
#define OS_NCR
#endif
#ifdef __NetBSD__
#define unix
#define OSMINOR "OS=NETBSD"
#define OS_NETBSD
#define NO_VFORK
#endif
#ifdef __QNX__
#define unix
#ifdef __QNXNTO__
#define OSMINOR "OS=QNXNTO"
#define OS_QNXNTO
#else
#define OSMINOR "OS=QNX"
#define OS_QNX
#define NO_VFORK
#define MAXLINE 996 /* max chars per command line */
#endif
#endif
#ifdef NeXT
#ifdef __APPLE__
#define OSMINOR "OS=RHAPSODY"
#define OS_RHAPSODY
#else
#define OSMINOR "OS=NEXT"
#define OS_NEXT
#endif
#endif
#ifdef __APPLE__
#define unix
#define OSMINOR "OS=MACOSX"
#define OS_MACOSX
#endif
#ifdef __osf__
#ifndef unix
#define unix
#endif
#define OSMINOR "OS=OSF"
#define OS_OSF
#endif
#ifdef _SEQUENT_
#define OSMINOR "OS=PTX"
#define OS_PTX
#endif
#ifdef M_XENIX
#define OSMINOR "OS=SCO"
#define OS_SCO
#define NO_VFORK
#endif
#ifdef sinix
#define unix
#define OSMINOR "OS=SINIX"
#define OS_SINIX
#endif
#if defined(__svr4__) || defined(__SVR4)
#define OSMINOR "OS=SOLARIS"
#define OS_SOLARIS
#elif defined(__sun__) || defined(__sun) || defined(sun)
#define OSMINOR "OS=SUNOS"
#define OS_SUNOS
#endif
#ifdef ultrix
#define OSMINOR "OS=ULTRIX"
#define OS_ULTRIX
#endif
#ifdef _UNICOS
#define OSMINOR "OS=UNICOS"
#define OS_UNICOS
#endif
#if defined(__USLC__) && !defined(M_XENIX)
#define OSMINOR "OS=UNIXWARE"
#define OS_UNIXWARE
#endif
#ifdef __OpenBSD__
#define OSMINOR "OS=OPENBSD"
#define OS_OPENBSD
#ifndef unix
#define unix
#endif
#endif
#if defined(__FreeBSD_kernel__) && !defined(__FreeBSD__)
#define OSMINOR "OS=KFREEBSD"
#define OS_KFREEBSD
#endif
#ifndef OSMINOR
#define OSMINOR "OS=UNKNOWN"
#endif

/* All the UNIX includes */

#include <sys/types.h>

#ifndef OS_MPEIX
#include <sys/file.h>
#endif

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef OS_QNX
#include <memory.h>
#endif

#ifndef OS_ULTRIX
#include <stdlib.h>
#endif

#if !defined(OS_BSDI) && !defined(OS_FREEBSD) && !defined(OS_DRAGONFLYBSD) \
	&& !defined(OS_NEXT) && !defined(OS_MACHTEN) && !defined(OS_MACOSX) \
	&& !defined(OS_RHAPSODY) && !defined(OS_MVS) && !defined(OS_OPENBSD)
#include <malloc.h>
#endif

#endif /* #ifndef OSMINOR */

/*
 * OSPLAT definitions - suppressed when it is a one-of-a-kind.
 */

#if defined(_M_PPC) || defined(PPC) || defined(ppc) || defined(__powerpc__) \
	|| defined(__ppc__)
#define OSPLAT "OSPLAT=PPC"
#endif

#if defined(_ALPHA_) || defined(__alpha__)
#define OSPLAT "OSPLAT=AXP"
#endif

#if defined(_i386_) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define OSPLAT "OSPLAT=X86"
#endif

#if defined(__ia64__) || defined(__IA64__) || defined(__ia64)
#define OSPLAT "OSPLAT=IA64"
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#define OSPLAT "OSPLAT=X86_64"
#endif

#if defined(__sparc__) || defined(__sparc)
#define OSPLAT "OSPLAT=SPARC"
#endif

#ifdef __mips__
#if _MIPS_SIM == _MIPS_SIM_ABI64
#define OSPLAT "OSPLAT=MIPS64"
#elif _MIPS_SIM == _MIPS_SIM_ABI32
#define OSPLAT "OSPLAT=MIPS32"
#endif
#endif

#if defined(__arm__) || defined(_M_ARM)
#define OSPLAT "OSPLAT=ARM"
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define OSPLAT "OSPLAT=ARM64"
#endif

#ifdef __s390__
#define OSPLAT "OSPLAT=390"
#endif

#ifdef __hppa
#define OSPLAT "OSPLAT=PARISC"
#endif

#if defined(__riscv) || defined(__riscv__)
#if __riscv_xlen == 64
#define OSPLAT "OSPLAT=RISCV64"
#elif __riscv_xlen == 32
#define OSPLAT "OSPLAT=RISCV32"
#endif
#endif

#ifndef OSPLAT
#define OSPLAT ""
#endif

/*
 * Jam implementation misc.
 */

#ifndef MAXLINE
#define MAXLINE 102400 /* max chars per command line */
#endif

#ifndef EXITOK
#define EXITOK 0
#define EXITBAD 1
#endif

#ifndef SPLITPATH
#define SPLITPATH ':'
#endif

/* You probably do not need to muck with these. */

#define MAXSYM 1024 /* longest symbol in the environment */
#define MAXJPATH 1024 /* longest filename */

#define MAXARGC 32 /* words in $(JAMSHELL) */

/* Jam private definitions below. */

#define DEBUG_MAX 14

struct global_config
{
	// Do not execute actions.
	bool noexec = false;
	// Build all targets.
	bool anyhow = false;
	// Max number of parallel jobs/actions.
	int jobs = 1;
	// Abort building on first fail occurrence.
	bool quitquick = false;
	// Build newest sources first.
	bool newestfirst = false;
	// Where to send the output streams.
	int pipe_action = 0;
	// If a debug level flag was given.
	bool debug_flag_used = false;
	// Debug level flags.
	bool debug[DEBUG_MAX] = { false, true };
	// Mirror output here
	FILE * out = nullptr;
	// Number of seconds to limit actions to, default 0 for no limit.
	long timeout = 0;
	// Maximum amount of output saved from target (kb)
	int max_buf = 0;
	// Is behaving as a debugger.
	bool is_debugger = false;
	// Debugger interface.
	enum : char
	{
		debug_interface_no = 0,
		debug_interface_console,
		debug_interface_mi,
		debug_interface_child
	} debug_interface = debug_interface_no;
	// Print out information on configuration actions.
	bool debug_configuration = false;
	// Show help output instead of building.
	bool display_help = false;

	void out_print() const;
};

extern global_config globs;

/* show actions when executed */
inline bool is_debug_make() { return globs.debug[1]; }
/* show even quiet actions */
inline bool is_debug_makeq() { return globs.debug[2]; }
/* show text of actions */
inline bool is_debug_exec() { return globs.debug[2]; }
/* show make0 progress */
inline bool is_debug_makeprog() { return globs.debug[3]; }
/* show when files bound */
inline bool is_debug_bind() { return globs.debug[3]; }

/* show execcmds()'s work */
inline bool is_debug_execcmd() { return globs.debug[4]; }

/* show rule invocations */
inline bool is_debug_compile() { return globs.debug[5]; }

/* show result of header scan */
inline bool is_debug_header() { return globs.debug[6]; }
/* show result of dir scan */
inline bool is_debug_bindscan() { return globs.debug[6]; }
/* show binding attempts */
inline bool is_debug_search() { return globs.debug[6]; }

/* show variable settings */
inline bool is_debug_varset() { return globs.debug[7]; }
/* show variable fetches */
inline bool is_debug_varget() { return globs.debug[8]; }
/* show variable expansions */
inline bool is_debug_varexp() { return globs.debug[8]; }
/* show 'if' calculations */
inline bool is_debug_if() { return globs.debug[8]; }
/* show list manipulation */
inline bool is_debug_lists() { return globs.debug[9]; }
/* show scanner tokens */
inline bool is_debug_scan() { return globs.debug[9]; }
/* show memory use */
inline bool is_debug_mem() { return globs.debug[9]; }

/* dump rule execution times */
inline bool is_debug_profile() { return globs.debug[10]; }
/* debug parsing */
inline bool is_debug_parse() { return globs.debug[11]; }
/* debug dependencies */
inline bool is_debug_graph() { return globs.debug[12]; }
/* show fate changes in make0() */
inline bool is_debug_fate() { return globs.debug[13]; }

/* Everyone gets the memory definitions. */
#include "mem.h"

/* They also get the profile functions. */
#include "debug.h"

#endif
