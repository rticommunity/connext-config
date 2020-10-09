dnl *****************************************************************************
dnl * Copyright (c) 2020 Real-Time Innovations, Inc.  All rights reserved.      *
dnl *                                                                           *
dnl * Permission to modify and use for internal purposes granted.               *
dnl * This software is provided "as is", without warranty, express or implied.  *
dnl *****************************************************************************

AC_INIT(connext-config, 1.0.3, fabrizio@rti.com)
AC_CONFIG_SRCDIR([src/connext-config.c])

AM_INIT_AUTOMAKE([subdir-objects tar-ustar foreign])

AC_PROG_CC

AC_CONFIG_MACRO_DIRS([m4])


dnl **********************************************************************************************
dnl * Debug/Release                                                                              *
dnl **********************************************************************************************
AC_ARG_ENABLE(debug,
[  --enable-debug          enable full debugging information (and disable optimization)],
[
        CFLAGS="-g -O0 -DDEBUG"
],[
        CFLAGS="-O2 -DNDEBUG"
])


dnl **********************************************************************************************
dnl * Enable/disable compiler warnings                                                           *
dnl **********************************************************************************************
AC_ARG_ENABLE(warnings,
[  --enable-warnings      enable compiler warnings during build ],
[
        CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wwrite-strings -Wconversion -Wcast-qual -W -Wimplicit-fallthrough -Wmissing-field-initializers -Wunused-parameter"
],[
        CFLAGS="$CFLAGS -W -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-unused-parameter"
])

dnl ****************************************************************************
dnl --with-rticonnextdds
dnl ****************************************************************************
AC_ARG_WITH(rticonnextdds,
[AC_HELP_STRING([--with-rticonnextdds],[specify the location of RTI Connext DDS (optional)])],
[
        if test "${withval}" = "yes"; then
            AC_MSG_ERROR(missing argument for --with-rticonnextdds parameter);
        else
            NDDSHOME="${withval}"
            [[[ ${NDDSHOME} != /* ]]] && NDDSHOME="`pwd`/${NDDSHOME#./}"
            dnl Realpath is not available on Mac, otherwise this would do the job:
            dnl NDDSHOME=`realpath "${withval}"`
        fi
],[
        AC_MSG_RESULT(Checking for NDDSHOME...)
        if test "a${NDDSHOME}a" = "aa"; then
            AC_MSG_ERROR(NDDSHOME not defined. You must specify the --with-rticonnextdds argument);
        else
            NDDSSHOME="${NDDSHOME}"
        fi
])

dnl Verify the provided location
AC_CHECK_FILE("${NDDSHOME}/include/ndds/ndds_c.h", 
[
        AC_MSG_RESULT(Using RTI Connext DDS at ${NDDSHOME})
],[
        AC_MSG_ERROR(RTI Connext DDS not found or NDDSHOME not defined)
])

CFLAGS="$CFLAGS -I${NDDSHOME}/include -I${NDDSHOME}/include/ndds"

AC_OUTPUT(Makefile src/Makefile)

