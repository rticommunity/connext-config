# Request Reply

This is the RTI Connext DDS example copied from the example directory in the RTI workspace: `rti_workspace/<VersionNumber>/examples/connext_dds/c/hello_world_request_reply`

This version uses the same source files (PrimesNumberReplier.c, PrimesNumberRequester.c, ...) but uses `cmake` to generate buid files for a target architecture.

With this approach cmake will invoke `connext-config` to retrieve the correct build flags and will use the results to set the compiler, linker, flags and libraries.

To build the project you need to have `cmake` installed in your system.
Make sure you have the environment variable `$NDDSHOME` correctly pointing to your home directory of RTI ConnextDDS, then create a build directory and invoke the cmake to create the required Makefiles:

```
$ mkdir build
$ cd build
$ cmake .. -DTARGET=x64Linux4gcc7.3.0
```

At this point you can build the example with:
```
$make 
```

You can specify the following cmake variables from command line:
* STATIC=1: set to link against the RTI Connext DDS static libraries (by default links against the dynamic libraries)
* DEBUG=1: set to build a DEBUG target that links against the RTI Connext DDS debug libraries

For example:

```
$ cmake .. -DSTATIC=1 -DDEBUG=1 -DTARGET=x64Linux4gcc7.3.0
```

