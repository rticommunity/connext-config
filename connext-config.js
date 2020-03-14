#!/usr/bin/env node

/******************************************************************************
 *
 * Copyright 2019 Fabrizio Bertocci (fabriziobertocci@gmail.com)
 * 
 * connext-config is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * connext-config is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with connext-config.  If not, see <http://www.gnu.org/licenses/>.
 * 
/******************************************************************************

/* Takes the rtiddsgen master template file containing the definition of
 * the project files (compilers, flags, linker, libs...), parses it and
 * made it available internally into a Javascript array of architecture
 * definitions.
 *
 * This object contains properties like the following:
 *      OS
 *      PLATFORMS
 *      C_COMPILER
 *      C_COMPILER_FLAGS
 *      C_LINKER
 *      C_LINKER_FLAGS
 *      CXX_COMPILER
 *      CXX_COMPILER_FLAGS
 *      CXX_LINKER
 *      CXX_LINKER_FLAGS
 *      SYSLIBS
 *      INCLUDES
 *      DEFINES
 *      HIDDEN
 *
 *
 * How this script works:
 * - Relies heavily on splitting the file into chunks using String.split() function
 * - Some massaging of each obtained line is necessary.
 * - Understanding regex is needed
 *
 */


"use strict;"

const fs=require('fs');
const path=require('path');

const APPLICATION_NAME = "connext-config";
const APPLICATION_VERSION = "0.9.9";        // Because why not?

const EXIT_SUCCESS = 0;
const EXIT_INVALID_ARGS = 1;
const EXIT_NO_NDDSHOME = 2;
const EXIT_FAILURE = 3;

// Define global $OS and $PLATFORM
const $OS = {};
["UNIX", "ANDROID", "IOS", "INTEGRITY", "VXWORKS", "WIN"].forEach( k => {
    $OS[k] = k;
});

const $PLATFORM = {};
["i86", "armv7a", "x64", "ppc", "mips64"].forEach( k => {
    $PLATFORM[k] = k;
});

const $CPU = {};
["i86", "armv7a", "x64", "ppcbe"].forEach( k => {
    $CPU[k] = k;
});

const VALID_WHAT = [
    "--list-all",
    "--ccomp", "--clink", "--cxxcomp", "--cxxlink", "--cflags", "--cxxflags",
    "--ldflags", "--ldxxflags", "--ldlibs", "--ldxxlibs",
    "--os", "--platform"
];

// Will be populated from processing the platform file
var theArchDef = {};    // Key is the target architecture, value is an object with all 
                        // the properties

// Command line parameters
let argOp;
let argTarget;
let argStatic = false;
let argDebug = false;
let argShell = false;
let argNoExpand = false;


// {{{ usage
// -----------------------------------------------------------------------------
function usage() {
    console.log(`
RTI Connext DDS Config version ${APPLICATION_VERSION}
------------------------------------------------------------------------------
Usage: 
    ${APPLICATION_NAME} [-h|--help|-V|--version]
    ${APPLICATION_NAME} --list-all
    ${APPLICATION_NAME} [modifiers] <what> <targetArch>

Where [modifiers] are:
    --static    use static linking against RTI Connext DDS
    --debug     use debug version of the RTI Connext DDS libraries
    --sh        use shell-like variable expansion (vs. make-like variables)
    --noexpand  do not expand environment variables in output

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
`);
}

// }}}
// {{{ processArch
// -----------------------------------------------------------------------------
// This is the function invoked from the input target definition
// def is an object like this:
//  
// processArch("mpc8572Inty5.1.0.whitefin","", {
//	$OS : $OS.INTEGRITY,
//	$CPU_TYPE : "mpc8572",
//	$MAJOR_VERSION : 10,
//	$HIDDEN : true,
//    $SHM_AREA_TEMPLATE : "shm_area.c.vm",
//    $POSIX_SHM_MANAGER_GPJ_TEMPLATE : "posix_shm_manager.gpj.vm",
//    $POSIX_SHM_MANAGER_INT_TEMPLATE : "posix_shm_manager.int.vm",
//    ...
// })
//
function processArch(os, comp, def) {
    if (os == "i86Win32" || os == "x64Win64") {
        // Skip all windows targets...
        return;
    }
    let targetArch = `${os}${comp}`;
    let obj = {};
    Object.keys(def).forEach( key => {
        // key is '$OS', '$BLAH'...
        let goodKey = key.substr(1);        // remove the "$" from the key name
        let val = def[key];
        if (Array.isArray(val)) {
            // Option array, unwrap it and prepend a '-'
            let flag='-';
            if (goodKey == 'INCLUDES') {
                // if the key is 'INCLUDES' we need to compose the array with '-I' and not with just '-'
                flag = '-I';
            }
            let newval = "";
            let sep = "";
            for (let i in val) {
                newval += `${sep}${flag}${val[i]}`
                sep = ' ';
            }
            val = newval;
        }
        if (argShell && (typeof(val) == 'string')) {
            // Some of the values reference an environment variable in the 
            // makefile-compatible form: $(name). If --sh is specified, we need
            // to transform them in shell-compatible form.
            // 
            // In this case replace the '(' -> '{' and ')' -> '}' for shell flags
            // Ok this regex took me a while to compose it... 
            // essentially works in this way: 
            //  - creates 3 capture groups: 
            //      $1: contains only the '$(' 
            //      $2: contains all the words (\w) repeated 1 or more
            //      $3: contains the ')'
            //  - Replace the matching expression with:
            //      - The characters '${'
            //      - The content of the 2nd capture group (the name of the variable)
            //      - the character '}'
            //  - The 'g' at the end means 'global', allow replacing ALL the variables
            val = val.replace(/(\$\()(\w+)(\))/g, '${$2}');
        }
        obj[goodKey] = val;
    })
    if (obj['HIDDEN'] === true) {
        // Skip hidden architectures
        return;
    }
    // Else, append this arch
    theArchDef[targetArch] = obj;
}

// }}}
// {{{ expandEnvVar
// -----------------------------------------------------------------------------
function expandEnvVar(str) {
    return (argNoExpand) ? str : (str.replace(/\$\(([^%]+)\)/g, (_,n) => process.env[n]===undefined?"":process.env[n]) );
}

// }}}


// ----------------------------------------------------------------------------
// Main code starts here
// ----------------------------------------------------------------------------
// argv[0] = 'node'
// argv[1] = 'connext-config'
// argv[2] = fist argument
// ...
if (process.argv.length < 3) {
    usage();
    process.exit(EXIT_INVALID_ARGS);
}
if ((process.argv.length == 3) && 
        ((process.argv[2] == '-h') || (process.argv[2] == '--help'))) {
    usage();
    process.exit(EXIT_SUCCESS);
}
if ((process.argv.length == 3) && 
        ((process.argv[2] == '-V') || (process.argv[2] == '--version'))) {
    console.log(`${APPLICATION_NAME} v.${APPLICATION_VERSION}`);
    process.exit(EXIT_SUCCESS);
}
if ((process.argv.length == 3) && (process.argv[2] == '--list-all')) {
    argOp = process.argv[2];

} else {
    // Parse command line, expects at least 2 arguments
    if (process.argv.length < 4) {
        usage();
        process.exit(EXIT_INVALID_ARGS);
    }

    for (let i = 2; i < process.argv.length-1; ++i) {
        if (process.argv[i] == "--static") {
            argStatic = true;
            continue;
        }
        if (process.argv[i] == "--debug") {
            argDebug = true;
            continue;
        }
        if ((process.argv[i] == "--sh") || (process.argv[i] == "--shell")) {
            argShell = true;
            continue;
        }
        if (process.argv[i] == "--noexpand") {
            argNoExpand = true;
            continue;
        }
        if (process.argv[i] == "-h" || process.argv[i] == "--help") {
            usage();
            process.exit(EXIT_SUCCESS);
        }
        if (VALID_WHAT.indexOf(process.argv[i]) != -1) {
            argOp = process.argv[i];
            continue;
        }
        usage();
        console.log(`Error: invalid argument: ${process.argv[i]}`);
        process.exit(EXIT_INVALID_ARGS);
    }
    argTarget = process.argv[process.argv.length-1];
    if (argTarget == "-h" || argTarget == "--help") {
        usage();
        process.exit(EXIT_SUCCESS);
    }
    if (argTarget.startsWith('--')) {
        usage();
        console.log("Error: missing target architecture.");
        console.log("Use --list-all to print all the supported architectures.");
        process.exit(EXIT_INVALID_ARGS);
    }
}

// Check if NDDSHOME is defined
let NDDSHOME;
if (process.env.hasOwnProperty('NDDSHOME')) {
    NDDSHOME = process.env['NDDSHOME'];
} else {
    // __dirname + one level up
    NDDSHOME = path.dirname(__dirname);
}

// Attempt to locate the platform file
let platformFile = path.join(NDDSHOME, "resource", "app", "app_support", "rtiddsgen", "templates", 'projectfiles', "platforms.vm");
if (!fs.existsSync(platformFile)) {
    usage();
    console.log(`Error: cannot find platform file in ${platformFile}`);
    process.exit(EXIT_NO_NDDSHOME);
}


let platformAll = fs.readFileSync(platformFile).toString();


// Execution start here. platformAll is the full file. Split into array using '#arch' we get:
// all[0] = all the top comments of the file (to be ignored)
// all[1] = ("i86Sol2.9","gcc3.3.2", {\n     $OS : $OS.UNIX,...
// all[2] = ("i86Sol2.10","gcc3.4.2", {\n     $OS : $OS.UNIX,...
// ...
//
// The idea is to iterate through all the elements and build a string that looks like a function
// call that can be evaluated by this script

let all = platformAll.split("#arch")
// console.log(`Loaded ${all.length} architectures`);

// Skip first element since is everything up to the first arch definition
for (let i=1; i < all.length; ++i) {
    let arch = all[i];

    // Hack: fix a bug in the platform.vm shipped with RTI Connext DDS versions 6.0.1 and earlier
    //       where the template file have a missing comma for some architectures:
    //       For example, for Connext 6.0.1, the platform.vm contains:
    //              #arch("armv7Linux3.0","gcc4.6.1.cortex-a9" { 
    //       (there is a missing comma before the open brace).
    //       This command performs a search and replace inside each arch definition to fix that
    arch = arch.replace(/" {/g, "\", {");

    // arch is now something like this: ("i86Sol2.9","gcc3.3.2", {\n    $OS : $OS.UNIX,\n   ...
    // Remove all the extra comments that sometimes are placed after the definition by
    // splitting by '})\n' and taking the first element only (then re-append the '})' element)
    arch="processArch" + arch.split('})\n')[0] + "})";

    // What we got now is a something that resemble to a function call:
    //  processArch("plat", "compiler", { propertyName: propertyValue, ...})
    eval(arch);
}

// Done, now let's process the argOp requested operation
if (argOp == "--list-all") {
    console.log(Object.keys(theArchDef).join("\n"));
    process.exit(EXIT_SUCCESS);
}
let targetDef = theArchDef[argTarget];
if (!targetDef) {
    usage();
    console.log(`Error: requested target architecture '${argTarget}' is not supported.`);
    console.log("Use --list-all to print all the supported architectures.");
    process.exit(EXIT_INVALID_ARGS);
}

// Compose the NDDS-related includes and libraries
let nddsFlags;
let nddsCLibs;
let nddsCPPLibs;
let libSuffix = `${argStatic ? 'z' : ''}${argDebug ? 'd' : ''}`;
if (argNoExpand) {
    if (argShell) {
        // Use shell style
        nddsFlags = "-I${NDDSHOME}/include -I${NDDSHOME}/include/ndds";
        nddsCLibs = "-L${NDDSHOME}/lib/${argTarget} -lnddsc${libSuffix} -lnddscore${libSuffix}";
        nddsCPPLibs = "-L${NDDSHOME}/lib/${argTarget} -lnddscpp${libSuffix} -lnddsc${libSuffix} -lnddscore${libSuffix}";
    } else {
        // Use makefile style
        nddsFlags = "-I$(NDDSHOME)/include -I$(NDDSHOME)/include/ndds";
        nddsCLibs = "-L$(NDDSHOME)/lib/${argTarget} -lnddsc${libSuffix} -lnddscore${libSuffix}";
        nddsCPPLibs = "-L$(NDDSHOME)/lib/${argTarget} -lnddscpp${libSuffix} -lnddsc${libSuffix} -lnddscore${libSuffix}";
    }

} else {
    nddsFlags = `-I${NDDSHOME}/include -I${NDDSHOME}/include/ndds`;
    nddsCLibs = `-L${NDDSHOME}/lib/${argTarget} -lnddsc${libSuffix} -lnddscore${libSuffix}`;
    nddsCPPLibs = `-L${NDDSHOME}/lib/${argTarget} -lnddscpp${libSuffix} -lnddsc${libSuffix} -lnddscore${libSuffix}`;
}

switch(argOp) {
    case "--ccomp":
        console.log(expandEnvVar(targetDef.C_COMPILER));
        break;

    case "--clink":
        console.log(expandEnvVar(targetDef.C_LINKER));
        break;

    case "--cxxcomp":
        console.log(expandEnvVar(targetDef.CXX_COMPILER));
        break;

    case "--cxxlink":
        console.log(expandEnvVar(targetDef.CXX_LINKER));
        break;

    case "--cflags":
        console.log(expandEnvVar([targetDef.C_COMPILER_FLAGS, targetDef.DEFINES, targetDef.INCLUDES, nddsFlags].join(' ').trim()));
        break;

    case "--cxxflags":
        console.log(expandEnvVar([targetDef.CXX_COMPILER_FLAGS, targetDef.DEFINES, targetDef.INCLUDES, nddsFlags].join(' ').trim()));
        break;

    case "--ldflags":
        console.log(expandEnvVar(targetDef.C_LINKER_FLAGS));
        break;

    case "--ldxxflags":
        console.log(expandEnvVar(targetDef.CXX_LINKER_FLAGS));
        break;

    case "--ldlibs":
        console.log(expandEnvVar([nddsCLibs, targetDef.SYSLIBS].join(' ').trim()));
        break;

    case "--ldxxlibs":
        console.log(expandEnvVar([nddsCPPLibs, targetDef.SYSLIBS].join(' ').trim()));
        break;

    case "--os":
        console.log(expandEnvVar(targetDef.OS));
        break;

    case "--platform":
        console.log(expandEnvVar(targetDef.PLATFORMS));
        break;

    default:
        usage();
        console.log(`Error, unexpected <what>: ${argOp}`);
        process.exit(EXIT_FAILURE);
}
process.exit(EXIT_SUCCESS);


