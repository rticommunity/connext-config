# RTI Package Config

A small command-line utiity, similar to pkg-config that help supply build information for a given architecture



## Usage

```
Usage:
    rtipkg-config [-h|--help]
    rtipkg-config --list-all
    rtipkg-config [modifiers] <what> <targetArch>

Where [modifiers] are:
    --static    use static linking against RTI Connext DDS
    --debug     use debug version of the RTI Connext DDS libraries
    --sh        use shell-like variable expansion (vs. make-like variables)
    --noexpand  do not expand NDDSHOME variable in output

And <what> (required) is one of:
    --ccomp     output the C compiler to use
    --clink     output the C linker to use
    --cxxcomp   output the C++ compiler to use
    --cxxlink   output the C++ linker to use
    --cflags    output all pre-processor and compiler flags for C compiler
    --cxxflags  output all pre-processor and compiler flags for C++ compiler
    --ldflags   output the linker flags to use when building C programs
    --ldxxflags output the linker flags to use when building C++ programs
    --ldlibs    output the required libraries for C programs
    --ldxxlibs  output the required libraries for C++ programs
    --os        output the OS (i.e. UNIX, ANDROID, IOS, ...)
    --platform  output the Platform (i.e. i86, x64, armv7a, ...)

<targetArch> is one of the supported target architectures. Use --list-all
to output a list of supported architectures
```



The tool can be invoked in two ways:

1. To list all the supported targets (including targets NOT installed). This is equivalent of using:
   `pkg-config --list-all`. No other arguments are required.
2. To obtain the right tool name, flags and libraries for a given architecture. 

In general, when determining the tools required to build a RTI Connext DDS application, you might want to use this tool 5 times. For example, to build a C program:

* Use the `--ccom` to determine the correct C compiler to use
* Use the `--cflags` to determine the right flags for the C compiler 
* Use the `--clink` to determine the correct linker to use
* Use the `--ldflags` and `--libs` to get the flags and libraries for the linker. Place the output of the `--ldflags` at the beginning of the argument list, and the output of `--libs` at the end.



### Examples

Show all the supported Linux 64-bit targets: 

```
$ ./rtipkg-config --list-all | grep x64Linux
x64Linux2.6gcc3.4.5
x64Linux2.6gcc4.1.1
x64Linux2.6gcc4.1.2
...
```



Show the C compiler to use for the target architecture `ppc4xxFPLinux2.6gcc4.5.1`:

```
$ ./rtipkg-config --ccomp ppc4xxFPLinux2.6gcc4.5.1
powerpc-ngc-linux-gcc
```



Shows the libraries to use when statically linking (`--static`) C applications on `x64Linux2.6gcc4.5.1` with debugging information (`--debug`):

```
$ ./rtipkg-config --debug --static --ldlibs x64Linux2.6gcc4.5.1
-L/Users/fabrizio/working/rti_connext_dds-6.0.0/lib/x64Linux2.6gcc4.5.1 -lnddsczd -lnddscorezd -ldl -lnsl -lm -lpthread -lrt
```



### Hello Builtin

Inside the `examples` directory you can find the `examples/connext_dds/c/hello_builtin` example that is part of the installed `rti_workspace`. This example uses the popular autotools (autoconf, automake) to invoke `rtipkg-config` to determine the correct build settings. 

Refer to the [README.md](examples/autoconf-hello/README.md) file for additional information on this example.





## Additional Notes

* The tool look at the target definition from the platform file used by rtiddsgen. This file is located under the following directory: `$NDDSHOME/resource/app/app_support/rtiddsgen/templates/projectfiles/platforms.vm`
* `rtipkg-config` will first use the environment variable `$NDDSHOME` to locate the platform file above. If `$NDDSHOME` is not defined, it will attempt to determine the location of the platform file from the location of the `rtipkg-config` application.
* The `--debug` option affects only the name of the required libraries and NOT the C or C++ compiler flags (for example, it does not include `-g` or disable any optimization). 
* All the Windows architectures are **not supported**. 
* To generate a stand-alone executable from the node.js application, you can use [pkg](https://www.npmjs.com/package/pkg) with node v10:
  * Linux: `pkg -t node10-linux-x64 rtipkg-config.js`
  * Windows:  `pkg -t node10-win-x64 rtipkg-config.js`
  * MacOS: `pkg -t node10-linux-x64 rtipkg-config.js`
* To install this application on your RTI Connext DDS Install (so it stays together with your installation):
  * Copy the script: `bin/rtipkg-config` under `$NDDSHOME/bin`
  * Copy the compiled version of rtipkg-config under: `$NDDSHOME/resource/app/bin/<hostPlatform>` (where <hostPlatform> is the name of the directory containing platform-specific binaries).