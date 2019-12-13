dnl **********************************************************************************
dnl (c) Copyright 2018, Real-Time Innovations, Inc. All rights reserved.
dnl **********************************************************************************

AC_INIT(Hello, 1.0, fabrizio@rti.com)
AC_CONFIG_SRCDIR([src/Hello.c])

dnl Uncomment the next line if you are building a shared library
dnl LT_INIT([dlopen])


dnl subdir-objects tells Makefile to place object files in the same dir as sources.
dnl See: https://www.gnu.org/software/automake/manual/html_node/List-of-Automake-options.html
AM_INIT_AUTOMAKE([subdir-objects tar-ustar foreign])

AC_PROG_CC
AM_PROG_CC_C_O

AC_CONFIG_MACRO_DIRS([m4])

RTICFG_STATIC=""
dnl **********************************************************************************************
dnl * Static/Dynamic                                                                             *
dnl **********************************************************************************************
AC_ARG_ENABLE(dynamic,
[  --enable-dynamic        uses RTI dynamic libraries instead of static lib],
[],[
        RTICFG_STATIC="--static"
])

dnl **********************************************************************************************
dnl * Debug/Release                                                                              *
dnl **********************************************************************************************
RTICFG_DEBUG=""
AC_ARG_ENABLE(debug,
[  --enable-debug          enable full debugging information (and disable optimization)],
[
        CFLAGS="-g -O0 -DDEBUG"
        RTICFG_DEBUG="--debug"
],[
        CFLAGS="-O2 -DNDEBUG"
])


dnl **********************************************************************************************
dnl * Enable/disable compiler warnings                                                           *
dnl **********************************************************************************************
AC_ARG_ENABLE(warnings,
[  --enable-warnings       enable compiler warnings during build ],
[
        CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wwrite-strings -Wconversion -Wcast-qual -W -Wimplicit-fallthrough=0"
],[
        CFLAGS="$CFLAGS -W -Wimplicit-fallthrough=0"
])

dnl **********************************************************************************************
dnl * Connext DDS
dnl **********************************************************************************************
AC_ARG_WITH(connextdds,
[  --with-connextdds=ARG   specify the location of RTI Connext DDS (optional)],
[
        if test "a${withval}a" = "aa"; then
            AC_MSG_ERROR(missing argument for --with-connextdds parameter);
        elif test "${withval}" = "yes"; then
            AC_MSG_ERROR(you must specify a value for --with-connextdds parameter);
        else
            NDDSHOME="${withval}"
        fi
],[
        AC_MSG_RESULT(Checking for NDDSHOME...)
        if test "a${NDDSHOME}a" = "aa"; then
            AC_MSG_ERROR(NDDSHOME not defined. You must specify the --with-connextdds argument);
        else
            NDDSSHOME="${NDDSHOME}"
        fi
])

dnl Verify the provided location
AC_CHECK_FILE("${NDDSHOME}/include/ndds/ndds_c.h", 
[
        AC_MSG_RESULT(Found RTI Connext DDS at ${NDDSHOME})
],[
        AC_MSG_ERROR(RTI Connext DDS not found or NDDSHOME not defined)
])



dnl **********************************************************************************************
dnl * Target kind
dnl **********************************************************************************************
AC_ARG_ENABLE(target,
[  --enable-target=ARG     specify the target architecture (required)
                          Use 'rtipkg-config --list-all' to view the supported targets ],
[
        if test "${enableval}" = "yes"; then
            AC_MSG_ERROR(missing argument for --enable-target parameter);
        else
            AC_MSG_RESULT(Using rtipkg-config to validate target...)
            if test -z `rtipkg-config --list-all | grep ${enableval}`; then
                AC_MSG_ERROR(invalid or unsupported target type);
            fi
            if test $? -ne 0; then
                AC_MSG_ERROR(error invoking rtipkg-config, check log for details)
            fi
            AC_MSG_RESULT(Using rtipkg-config to retrieve build settings...)
            CC="`rtipkg-config --ccomp ${enableval}`"
            LD="`rtipkg-config --clink ${enableval}`"
            CFLAGS="$CFLAGS `rtipkg-config $RTICFG_STATIC $RTICFG_DEBUG --cflags ${enableval}`"
            LDFLAGS="`rtipkg-config $RTICFG_STATIC $RTICFG_DEBUG --ldflags ${enableval}`"
            LIBS="`rtipkg-config $RTICFG_STATIC $RTICFG_DEBUG --ldlibs ${enableval}`"
        fi
        AC_MSG_RESULT(Building for target ${enableval} system)
],[
        AC_MSG_ERROR(you must specify --enable-target with a valid target. See --help for more info)
])


AC_OUTPUT(Makefile src/Makefile)

