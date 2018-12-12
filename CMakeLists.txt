cmake_minimum_required (VERSION 3.10.0)
project (x4c C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

if (MINGW)
    include_directories("D:/msys64/mingw64/x86_64-w64-mingw32/include")
    add_definitions(-D__MINGW__)
endif()

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib")
set (CMAKE_REQUIRED_DEFINITIONS -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -DHAVE_CONFIG_H -D__USE_ISOC11)
option (USE_JEMALLOC "")

#####################################################################################################

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package (Threads REQUIRED)
find_package (EXPAT REQUIRED)
find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)
if (NOT MINGW)
    find_package (Talloc REQUIRED)
endif()
    
CHECK_SYMBOL_EXISTS (mkostemps      "stdlib.h"   HAVE_MKOSTEMPS)
CHECK_SYMBOL_EXISTS (reallocarray   "stdlib.h"   HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (asprintf       "stdio.h"    HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"    HAVE_OPEN_MEMSTREAM)
CHECK_SYMBOL_EXISTS (vasprintf      "stdio.h"    HAVE_VASPRINTF)
CHECK_SYMBOL_EXISTS (memrchr        "string.h"   HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (strchrnul      "string.h"   HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (strdupa        "string.h"   HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (strlcat        "string.h"   HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strlcpy        "string.h"   HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strsep         "string.h"   HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strtonum       "string.h"   HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (fork           "unistd.h"   HAVE_FORK)
CHECK_SYMBOL_EXISTS (pause          "unistd.h"   HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2          "unistd.h"   HAVE_PIPE2)
CHECK_SYMBOL_EXISTS (err            "err.h"      HAVE_ERR)
CHECK_SYMBOL_EXISTS (clock_gettime  "time.h"     HAVE_CLOCK_GETTIME)
CHECK_SYMBOL_EXISTS (gettimeofday   "sys/time.h" HAVE_GETTIMEOFDAY)
CHECK_SYMBOL_EXISTS (posix_spawnp   "spawn.h"    HAVE_POSIX_SPAWNP)

CHECK_INCLUDE_FILE("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE("threads.h"     HAVE_THREADS_H)

#####################################################################################################

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set (DEBUG 1)
endif ()

string(FIND ${CMAKE_C_COMPILER} "clang" CLANG_STR_SEARCH)

if (MINGW)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    set (WARNS "-Wall -Werror=implicit -Werror=incompatible-pointer-types")
    string(CONCAT WARNS "-Werror=pointer-to-int-cast -Werror=int-conversion -Werror=implicit-function-declaration -Werror=incompatible-pointer-types")
else()
    string(CONCAT WARNS "-Wall -Werror=implicit -Werror=incompatible-pointer-types"
        " -Werror=pointer-to-int-cast -Werror=int-conversion -Werror=format-extra-args -Werror=implicit-function-declaration")
endif()

if (NOT BUILD_DIST) 
    set (MARCH_SETTING "-march=native")
endif()

if (SANITIZE)
    if ("${SANITIZE}" STREQUAL "thread")
        set (SANIT "-fsanitize=thread -fsanitize-address-use-after-scope -fsanitize=undefined")
    elseif ("${SANITIZE}" STREQUAL "address")
        set (SANIT "-fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined")
    elseif ("${SANITIZE}" STREQUAL "ON" OR "${SANITIZE}" STREQUAL "On")
        set (SANIT "-fsanitize-address-use-after-scope -fsanitize=undefined")
    else ()
        message(FATAL_ERROR "Unrecognized SANITIZE setting!")
    endif()
endif()

set (BASE "${WARNS} ${MARCH_SETTING} ${SANIT} -std=gnu11 -fdiagnostics-color=always")

#####################################################################################################
if (CLANG_STR_SEARCH EQUAL -1)
    string(CONCAT WARNS "${WARNS}" 
        " -Wsuggest-attribute=pure -Wsuggest-attribute=cold -Wsuggest-attribute=malloc -Wsuggest-attribute=const"
        " -Wsuggest-attribute=noreturn -Wsuggest-attribute=format" " -Wold-style-definition -Wold-style-declaration")
    set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat -Wno-switch-unreachable")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Wextra -Wpedantic -Wno-switch-unreachable -Og -g3")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
#----------------------------------------------------------------------------------------------------
else() # CLANG
#----------------------------------------------------------------------------------------------------
    string(CONCAT CMAKE_C_FLAGS_DEBUG "${BASE} ${SANIT} -O0 -g3 -Wextra -Wpedantic -Wformat" " -Wall" # " -Weverything"
          " -Wno-sign-conversion -Wno-documentation-unknown-command"
          " -Wno-cast-qual -Wno-shorten-64-to-32 -Wno-unused-macros -Wno-switch-enum"
          " -Wno-disabled-macro-expansion -Wno-covered-switch-default -Wno-padded -Werror=enum-conversion"
          " -Wno-vla -Wno-missing-variable-declarations -Wno-gnu" " -Wno-format-nonliteral"
          " -Wno-error=incompatible-pointer-types-discards-qualifiers"
    )
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Og -g3")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rtlib=compiler-rt")
endif()
#####################################################################################################

string (CONCAT CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${BASE} -Og -g3")
string (CONCAT CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE} ${BASE} -g3 -Ofast -ftree-vectorize")

if (MINGW)
    # string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
    #     " -Wl,--allow-multiple-definition"
    # )
else()
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

add_definitions(-D_GNU_SOURCE -DHAVE_CONFIG_H -DBSTRING_USE_P99)

configure_file(cmake-config.h.in config.h)
add_subdirectory(src)
