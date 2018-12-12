#ifndef CONFIG_H__
#define CONFIG_H__

#define _GNU_SOURCE
#define __USE_MINGW_ANSI_STDIO 1
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#include <WinSock2.h>


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#define HAVE_CONSTRUCTOR_ATTRIBUTE
#define HAVE_INTPTR_T
#define HAVE_STRING_H
#define STDC_HEADERS 1
#define HAVE_STRERROR
#define HAVE_ERRNO_DECL
#define HAVE_STRDUP
#define HAVE_MEMMOVE
#define HAVE_STRNLEN
#define HAVE_STRNDUP
#define HAVE_SETENV
#define HAVE_STRTOLL
#define HAVE_STDBOOL_H
#define HAVE_BOOL
#define HAVE_UINTPTR_T
#define HAVE_PTRDIFF_T
#define HAVE_USLEEP
#define HAVE_DECL_EWOULDBLOCK
#define HAVE_VA_COPY

#define TALLOC_BUILD_VERSION_MAJOR 2
#define TALLOC_BUILD_VERSION_MINOR 1
#define TALLOC_BUILD_VERSION_RELEASE 14

#define _PUBLIC_ __attribute__((visibility("default")))
#define _PRIVATE_ __attribute__((visibility("hidden")))

#ifndef MIN
#  define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#  define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef fprintf
#  undef fprintf
#endif
#ifdef vsnprintf
#  undef vsnprintf
#endif
#define fprintf   __mingw_fprintf
#define vsnprintf __mingw_vsnprintf

#endif