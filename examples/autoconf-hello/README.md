# Hello Builtin

This is the RTI Connext DDS example that you can find in your RTI workspace 
directory: `rti_workspace/<version>/examples/connext_dds/c/hello_builtin`

This version uses autoconf/automake to generate a Makefile for the target
architecture selectable during the `./configure` phase.

The configure script make use of the `rtipkg-config` tool to determine
the correct build flags.

To generate configure script, run:

```
$ autoreconf -iv [-f to force]
```

Then, for example run configure and build for target `x64Linux4gcc7.3.0` with:

```
$ ./configure --enable-target=x64Linux4gcc7.3.0 \
              --with-connextdds=~/rti_connext_dds-6.0.0 \
              --enable-dynamic \
              --enable-debug
$ make
```


