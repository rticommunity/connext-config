# Hello Builtin

This is the RTI Connext DDS example copied from the example directory in the RTI workspace: `rti_workspace/<VersionNumber>/examples/connext_dds/c/hello_builtin`

This version uses the same source files (Hello.c, Hello.h, ...) but uses the autotools (autoconf, automake) to generate a Makefile for the target architecture selectable during the `./configure` phase.

The configure script make use of the `connext-config` tool to determine
the correct build flags.

To generate the `configure` script, you need to have autoconf and automake installed in your system, then run:

```
$ autoreconf -ivf
```



Once you have the ./configure script you can use it, for example, to build for target `x64Linux4gcc7.3.0` with:

```
$ ./configure --enable-target=x64Linux4gcc7.3.0 \
              --with-connextdds=~/rti_connext_dds-6.0.0 \
              --enable-dynamic \
              --enable-debug
$ make
```



To easily repackage the project for distribution, use:

```
make dist
```

That will produce a ready-to-ship tar file (`hello-1.0.tar.gz` ) with all the necessary files so you won't need anymore the autotools installed to build the application.
