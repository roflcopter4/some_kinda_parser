cmake_minimum_required (VERSION 3.15.0)
project (SomeKindaParser C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

option(SANITIZE "Enable sanitizers")

include_directories(
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/contrib/bstring"
)

###############################################################################
# Init

if (MSVC)
    message(FATAL_ERROR "MSVC is not and will probably never be supported. Use MinGW")
endif()
macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

###############################################################################
# Libraries

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

if (MINGW OR CMAKE_COMPILER_IS_MINGW)
    if (NOT MINGW)
        set (MINGW TRUE)
    endif()
    add_definitions(-D__MINGW__)
    string(CONCAT CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include _mingw.h")
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package (Threads REQUIRED)
find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)
if (NOT MINGW)
    find_package (Talloc REQUIRED)
endif()

set (CMAKE_REQUIRED_DEFINITIONS -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -DHAVE_TOPCONFIG_H)
add_definitions(-D_GNU_SOURCE -DHAVE_TOPCONFIG_H)

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
CHECK_INCLUDE_FILE("tgmath.h"      HAVE_TGMATH_H)

string(FIND ${CMAKE_C_COMPILER} "clang" CLANG_STR_SEARCH)

###############################################################################
# Flags

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if ((CMAKE_BUILD_TYPE STREQUAL "Debug") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set (DEBUG 1)
endif()

#####################################################################################################
if (MINGW)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    set (WARNS "-Wall -Werror=implicit -Werror=incompatible-pointer-types")
    string(CONCAT WARNS "-Werror=pointer-to-int-cast -Werror=int-conversion -Werror=implicit-function-declaration -Werror=incompatible-pointer-types")
else()
    string(CONCAT WARNS "-Wall"
        # " -Werror=implicit"
        " -Werror=incompatible-pointer-types"
        " -Werror=pointer-to-int-cast -Werror=int-conversion -Werror=format-extra-args")
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
message(WARNING "Sanitize set to \"${SANIT}\" -- \"${SANITIZE}\"")

set (BASE "${WARNS} ${MARCH_SETTING} ${SANIT} -std=gnu11 -fdiagnostics-color=always")

#####################################################################################################
if (CLANG_STR_SEARCH EQUAL -1)
    string(CONCAT WARNS "${WARNS}" 
        " -Wsuggest-attribute=pure -Wsuggest-attribute=cold -Wsuggest-attribute=malloc -Wsuggest-attribute=const"
        " -Wsuggest-attribute=noreturn -Wsuggest-attribute=format" " -Wold-style-definition -Wold-style-declaration")
    set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat -Wno-switch-unreachable")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Wextra -Wpedantic -Wno-switch-unreachable -Og -g3")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
else() # CLANG
    set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat -Wno-gnu-zero-variadic-macro-arguments")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Og -g3")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rtlib=compiler-rt")
endif()
#####################################################################################################

string (CONCAT CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${BASE} -Og -g3")
string (CONCAT CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE} ${BASE} -g3 -Ofast -ftree-vectorize")

if (MINGW)
    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
        " -Wl,--allow-multiple-definition"
    )
else()
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

configure_file(cmake-config.h.in topconfig.h)
add_subdirectory(src)

# vim: tw=0
