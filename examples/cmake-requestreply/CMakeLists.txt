# *****************************************************************************
# * Copyright (c) 2020 Real-Time Innovations, Inc.  All rights reserved.      *
# *                                                                           *
# * Permission to modify and use for internal purposes granted.               *
# * This software is provided "as is", without warranty, express or implied.  *
# *****************************************************************************

# When invoking cmake, you must:
# - Have the environment variable NDDSHOME defined
# - Set the cmake variable TARGET to the target architecture. To see a list of
#   target supported use `connext-config --list-all`. 
#   To see a list of targets installed on your computer

# To link against static libraries, use:
#       cmake -DSTATIC=1
#
# To use DEBUG libraries, set the variable 'DEBUG' to 1:
#       cmake -DDEBUG=1
#

cmake_minimum_required(VERSION 3.7)
project(connext-c-requestreply)

# Just a prefix string to be printed in every message
set(MSG_PREFIX "RTI ConnextDDS -")

# List of type support files
set(TYPE_SUPPORT_SRC
        src/Primes.c
        src/PrimesPlugin.c
        src/PrimesSupport.c)

# Validate NDDSHOME
if( NOT DEFINED ENV{NDDSHOME} )
    message(FATAL_ERROR "${MSG_PREFIX} Environment variable NDDSHOME is not defined")
endif()
message(STATUS "${MSG_PREFIX} Using install directory: $ENV{NDDSHOME}")

# Validate target
if( NOT DEFINED TARGET )
    message(FATAL_ERROR "${MSG_PREFIX} You must specify the target architecture using: -DTARGET=<arch>")
else()
    execute_process(COMMAND connext-config --list-installed OUTPUT_VARIABLE INSTALLED_TARGETS)
    string(FIND ${INSTALLED_TARGETS} "${TARGET}\n" POS)

    if( POS EQUAL -1)
        # Target not installed? Is it even supported at all?
        execute_process(COMMAND connext-config --list-all OUTPUT_VARIABLE ALL_TARGETS)
        string(FIND ${ALL_TARGETS} "${TARGET}\n" POS)
        if( POS EQUAL -1)
            message(FATAL_ERROR "${MSG_PREFIX} Target architecture: ${TARGET} not valid")
        else()
            message(WARNING "${MSG_PREFIX} Warning: target architecture: ${TARGET} is not installed")
        endif()
    endif()
endif()
message(STATUS "${MSG_PREFIX} Using target architecture: ${TARGET}")

# Static or Dynamic build?
if( STATIC )
    set(STATIC "--static")
    message(STATUS "${MSG_PREFIX} Using static libraries")
else()
    message(STATUS "${MSG_PREFIX} Using dynamic libraries")
endif()

# Debug or Release?
if( DEBUG )
    set(DEBUG "--debug")
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "${MSG_PREFIX} Using debug libraries")
else()
    message(STATUS "${MSG_PREFIX} Using release libraries")
endif()

# Retrieve the compiler to use
execute_process(COMMAND connext-config --ccomp ${TARGET} OUTPUT_VARIABLE APP_CCOMP)
# Strip the last newline of the compiler's name
string(REGEX REPLACE "\n$" "" APP_CCOMP "${APP_CCOMP}")

# Retrieve the compiler's flags
execute_process(COMMAND connext-config ${STATIC} ${DEBUG} --cflags ${TARGET} OUTPUT_VARIABLE APP_CFLAGS)
string(REGEX REPLACE "\n$" "" APP_CFLAGS "${APP_CFLAGS}")

# Retrieve the linker executable and strip out the last newline
execute_process(COMMAND connext-config --clink ${TARGET} OUTPUT_VARIABLE APP_CLINKER)
string(REGEX REPLACE "\n$" "" APP_CLINKER "${APP_CLINKER}")

# Retrieve the linker flags
execute_process(COMMAND connext-config ${STATIC} ${DEBUG} --ldflags ${TARGET} OUTPUT_VARIABLE APP_LDFLAGS)
string(REGEX REPLACE "\n$" "" APP_LDFLAGS "${APP_LDFLAGS}")

# Retrieve libraries
execute_process(COMMAND connext-config ${STATIC} ${DEBUG} --libmsg --ldlibs ${TARGET} OUTPUT_VARIABLE APP_LDLIBS)
string(REGEX REPLACE "\n$" "" APP_LDLIBS "${APP_LDLIBS}")

# Apply the retrieved parameters to the CMAKE variables
SET(CMAKE_C_COMPILER  ${APP_CCOMP})
SET(CMAKE_C_FLAGS  "${APP_CFLAGS} ${CMAKE_C_FLAGS}")
SET(CMAKE_LINKER  ${APP_CLINKER})
SET(CMAKE_EXE_LINKER_FLAGS  "${APP_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS}")

# Specify the two executables
add_executable(PrimeNumberReplier
        src/PrimeNumberReplier.c
        ${TYPE_SUPPORT_SRC})

add_executable(PrimeNumberRequester
        src/PrimeNumberRequester.c
        ${TYPE_SUPPORT_SRC})

# You can add the whole string returned by connext-config to target_link_libraries()
target_link_libraries(PrimeNumberReplier ${APP_LDLIBS})
target_link_libraries(PrimeNumberRequester ${APP_LDLIBS})

