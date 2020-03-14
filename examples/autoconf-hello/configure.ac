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


dnl **********************************************************************************************
dnl * Debug/Release                                                                              *
dnl **********************************************************************************************
AC_ARG_ENABLE(debug,
[  --enable-debug          enable full debugging information (and disable optimization)],
[
      CFLAGS="-g -O0"
      DEBUG_ENABLED=1
      AC_DEFINE([DEBUG],1,[])
],[
      CFLAGS="-O2 -DNDEBUG"
      DEBUG_ENABLED=0
      AC_DEFINE([NDEBUG],1,[])
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

m4_include([m4/rticonnextdds.m4])

AC_OUTPUT(Makefile src/Makefile)

