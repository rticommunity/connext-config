# RTI Package Config

A small command-line utiity, similar to pkg-config that help supply build information for a given architecture



## Usage

```sh
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
    --libs      output all linker flags (including syslibs)
    --rtilibs   output only RTI Connext libraries
    --syslibs   output only system libraries
    --os        output the OS (i.e. UNIX, ANDROID, IOS, ...)
    --platform  output the Platform (i.e. i86, x64, armv7a, ...)

<targetArch> is one of the supported target architectures. Use --list-all
to output a list of supported architectures
```



The tool can be invoked in two ways:

1. To list all the supported targets (including targets NOT installed). This is equivalent of using:
   `pkg-config --list-all`. No other arguments are required.
2. To obtain the right tool name, flags and libraries for a given architecture. 



### Usage examples

Show all the supported Linux 64-bit targets: 

```sh
$ ./rtipkg-config --list-all | grep x64Linux
x64Linux2.6gcc3.4.5
x64Linux2.6gcc4.1.1
x64Linux2.6gcc4.1.2
...
```



Show the C compiler to use for the target architecture `ppc4xxFPLinux2.6gcc4.5.1`:

```sh
$ ./rtipkg-config --ccomp ppc4xxFPLinux2.6gcc4.5.1
powerpc-ngc-linux-gcc
```



Shows the linker flags to use when statically linking (`--static`) C applications on `x64Linux2.6gcc4.5.1` with debugging information (`--debug`):

```sh
$ ./rtipkg-config --debug --static --ldflags x64Linux2.6gcc4.5.1
-L/Users/fabrizio/working/rti_connext_dds-6.0.0/lib/x64Linux2.6gcc4.5.1 -lnddsczd -lnddscorezd -ldl -lnsl -lm -lpthread -lrt
```



## Additional Notes

* The tool look at the target definition from the platform file used by rtiddsgen. This file is located under the following directory: `$NDDSHOME/resource/app/app_support/rtiddsgen/templates/projectfiles/platforms.vm`
* `rtipkg-config` will first use the environment variable `$NDDSHOME` to locate the platform file above. If `$NDDSHOME` is not defined, it will attempt to determine the location of the platform file from the location of the `rtipkg-config` application.
* To generate a stand-alone executable from the node.js application, you can use [pkg](https://www.npmjs.com/package/pkg) with node v10:
  * Linux: `pkg -t node10-linux-x64 rtipkg-config.js`
  * Windows:  `pkg -t node10-win-x64 rtipkg-config.js`
  * MacOS: `pkg -t node10-linux-x64 rtipkg-config.js`
* 