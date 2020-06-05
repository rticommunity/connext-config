dnl *****************************************************************************
dnl * Copyright (c) 2020 Real-Time Innovations, Inc.  All rights reserved.      *
dnl *                                                                           *
dnl * Permission to modify and use for internal purposes granted.               *
dnl * This software is provided "as is", without warranty, express or implied.  *
dnl *****************************************************************************

dnl
dnl Check for RTI ConnextDDS
dnl
dnl Requires the following environment variables:
dnl     DEBUG_ENABLED = [0|1] (optional)
dnl
dnl Modifies the following environment variables:
dnl     CC
dnl     LD
dnl     CFLAGS
dnl     LDFLAGS
dnl
dnl Defines the following environment variables:
dnl     NDDSHOME (if not already specified)
dnl     RTICFG_STATIC = "" | "--static"
dnl

dnl ****************************************************************************
dnl --with-rticonnextdds
dnl ****************************************************************************
AC_ARG_WITH(rticonnextdds,
[AC_HELP_STRING([--with-rticonnextdds],[specify the location of RTI Connext DDS (optional)])],
[
        if test "${withval}" = "yes"; then
            AC_MSG_ERROR(missing argument for --with-rticonnextdds parameter);
        else
            NDDSHOME=`realpath "${withval}"`
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
        AC_MSG_RESULT(Found RTI Connext DDS at ${NDDSHOME})
],[
        AC_MSG_ERROR(RTI Connext DDS not found or NDDSHOME not defined)
])

dnl ****************************************************************************
dnl --with-connext-config
dnl ****************************************************************************
dnl Allow user to define the connext-config to use, falling back to $NDDSHOME/bin if
dnl not specified
AC_ARG_WITH([connext-config],
    [AC_HELP_STRING([--with-connext-config],[defines the connext-config to use])],
    [
        if test "a${withval}a" = "aa"; then
            AC_MSG_ERROR(missing argument for --with-connext config);
        else
            CONNEXT_CONFIG="${withval}"
            export NDDSHOME
        fi
        AC_MSG_RESULT(Using connext-config from: ${CONNEXT_CONFIG})
    ], [ 
        AC_MSG_RESULT(Using connext-config from NDDSHOME)
        CONNEXT_CONFIG="${NDDSHOME}/bin/connext-config"
    ]
)

dnl Verify that connext-config is available
AC_CHECK_FILE("${CONNEXT_CONFIG}", 
[
        AC_MSG_RESULT(Successfully validated connext-config )
],[
   AC_MSG_RESULT()
   AC_MSG_RESULT(Cannot find the connext-config utility)
   AC_MSG_RESULT(For more info see: https://github.com/rticommunity/connext-config)
   AC_MSG_RESULT(To use a specific version of connext-config use: --with-connext-config=<path>)
   AC_MSG_ERROR(connext-config validation failed)
])


dnl ****************************************************************************
dnl --with-rticonnextdds-static                                                              *
dnl ****************************************************************************
RTICFG_STATIC=""
AC_ARG_WITH([rticonnextdds-static],
    [AC_HELP_STRING([--with-rticonnextdds-static],[uses RTI static libraries instead of dynamic libs])],
    [
        RTICFG_STATIC="--static"
        AC_MSG_RESULT(Using RTI ConnextDDS STATIC libraries)
    ], [ 
        AC_MSG_RESULT(Using RTI ConnextDDS DYNAMIC libraries)
    ]
)



dnl ****************************************************************************
dnl --enable-target
dnl ****************************************************************************
AC_ARG_ENABLE(target,
[AC_HELP_STRING([--enable-target],[specify the target architecture for RTI Connext DDS (required). Use 'connext-config --list-installed' to view the available targets])],
[
        if test "${enableval}" = "yes"; then
            AC_MSG_ERROR(missing argument for --enable-target parameter);
        else
            NDDSARCH=${enableval}
        fi
        AC_MSG_RESULT(Building for target ${enableval} system)
],[
        if test "a${NDDSARCH}a" = "aa"; then
            AC_MSG_ERROR(you must specify --enable-target with a valid target or set NDDSARCH env variable)
        else
            AC_MSG_RESULT(Using target architecture from NDDSARCH: ${NDDSARCH})
        fi
])


if test -z `${CONNEXT_CONFIG} --list-installed | grep ${NDDSARCH}`; then
    AC_MSG_ERROR(invalid or unsupported target type);
fi
if test $? -ne 0; then
    AC_MSG_ERROR(error invoking connext-config, check log for details)
fi
AC_MSG_RESULT(Using connext-config to get compiler and linker settings...)
if test "$DEBUG_ENABLED" = 1; then
    AC_MSG_RESULT(Using RTI ConnextDDS DEBUG libraries)
    RTICFG_DEBUG="--debug"
else
    AC_MSG_RESULT(Using RTI ConnextDDS RELEASE libraries)
    RTICFG_DEBUG=""
fi
CC="`${CONNEXT_CONFIG} --cxxcomp ${NDDSARCH}`"
LD="`${CONNEXT_CONFIG} --cxxlink ${NDDSARCH}`"
CFLAGS="$CFLAGS `${CONNEXT_CONFIG} $RTICFG_STATIC $RTICFG_DEBUG --cxxflags ${NDDSARCH}`"
LDFLAGS="`${CONNEXT_CONFIG} $RTICFG_STATIC $RTICFG_DEBUG --ldxxflags ${NDDSARCH}`"
LIBS="$LDFLAGS `${CONNEXT_CONFIG} $RTICFG_STATIC $RTICFG_DEBUG --ldxxlibs ${NDDSARCH}`"


