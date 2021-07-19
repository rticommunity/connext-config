/*****************************************************************************
 * Copyright (c) 2020 Real-Time Innovations, Inc.  All rights reserved.      *
 *                                                                           *
 * Permission to modify and use for internal purposes granted.               *
 * This software is provided "as is", without warranty, express or implied.  *
 *****************************************************************************/

/*
 * ---------------------------------------------------------------------------
 * This tool parses the rtiddsgen master project file:
 *      $NDDSHOME/resource/app/app_support/rtiddsgen/templates/projectfiles/platforms.vm
 * extracts the definition of project files (compilers, flags, linker, 
 * libs...), parses it and made it available through command line requests.
 *
 *
 * ---------------------------------------------------------------------------
 * This source file contains embedded fold tags around function declarations
 * to be interpreted by vim.
 * See sections containing {{{...}}}
 * If you change any code, please make sure those tags are maintained!
 *
 * To enable folding, enter:
 *  :set foldmethod=marker
 * Note that modelines are disabled by default on all the major Linux
 * distributions, so settings cannot be embedded in this file.
 * ---------------------------------------------------------------------------
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <libgen.h>     /* For dirname() */

#include <sys/types.h>
#include <sys/stat.h>   /* For stat() */

#include <unistd.h>
#include <limits.h>

#include <ndds/reda/reda_inlineList.h>

/* Define the following macro to use getcwd to retrieve the current
 * working directory using getcwd().
 * If unset, it will use genenv("CWD")
 * NOTE: getenv("CWD") might not work when running inside a containers
 *       or a reduced shell.
 */
#define USE_GETCWD

#define APPLICATION_NAME                        "connext-config"
#define APPLICATION_VERSION                     "1.0.3"

/* The location of the platform file to parse, from the NDDSHOME directory */
#define NDDS_PLATFORM_FILE      \
    "resource/app/app_support/rtiddsgen/templates/projectfiles/platforms.vm"

#define APPLICATION_EXIT_SUCCESS                0
#define APPLICATION_EXIT_INVALID_ARGS           1
#define APPLICATION_EXIT_NO_NDDSHOME            2
#define APPLICATION_EXIT_FAILURE                3
#define APPLICATION_EXIT_UNKNOWN                4

/* NULL-terminated array of valid <what> commands */
const char * VALID_WHAT[] = {
    "--ccomp", "--cflags", "--clink", "--ldflags", "--ldlibs",
    "--cxxcomp", "--cxxflags", "--cxxlink", "--ldxxflags", "--ldxxlibs",
    "--cxx03comp", "--cxx03flags", "--cxx03link", "--ldxx03flags", "--ldxx03libs",
    "--cxx11comp", "--cxx11flags", "--cxx11link", "--ldxx11flags", "--ldxx11libs",
    "--os", "--platform",
    NULL
};

/* State machine used when parsing the project file */
typedef enum {
    RSM_TOPLEVEL,           /* Parsing top level of the file */
    RSM_MACRO,              /* Parsing inside a #macro */
    RSM_ARCH                /* Parsing inside an #arch section */
} ReadStateMachine;

/***************************************************************************
 * Application Limits:
 * The following limits are used when allocating static buffers
 **************************************************************************/
/* Max length of a string token inside the parsed architecture list.
 * This value determines the max size of keys, single-string values, 
 * values of arrays, and architecture names
 */
#define MAX_STRING_SIZE     200

/* The maximum number of key-value pairs stored for each architecture */
#define MAX_ARRAY_SIZE      40

/* The maximum length of a command line or command line argument */
#define MAX_CMDLINEARG_SIZE 1024


/***************************************************************************
 * ArchParameter
 **************************************************************************/
/* Defines the object to store an individual key-value pair.
 * The value can be of 4 different types:
 *  - Boolean (represented as integer 1=true, 0=false)
 *  - string (max length MAX_STRING_SIZE)
 *  - Array of strings (each string has MAX_STRING_SIZE max length, and
 *    the array has up to MAX_ARRAY_SIZE values)
 *  - Env variables using the form $FOO.bar
 *
 * An ArchParameter object is a node of the list stored in the ArchParameter
 */
typedef enum {
    APVT_Invalid = 0,
    APVT_Boolean,
    APVT_String,
    APVT_ArrayOfStrings,
    APVT_EnvVariable
} ArchParamValueType;

struct ArchParameter {
    struct REDAInlineListNode   parent;
    char                        key[MAX_STRING_SIZE];
    union {
        /* 
         * Value is represented as a single string 
         * (APVT_String or APVT_EnvVariable) 
         */
        char as_string[MAX_STRING_SIZE];

        /* Value is represented as an array of strings */
        char as_arrayOfStrings[MAX_ARRAY_SIZE][MAX_STRING_SIZE];

        /* Value is a boolean */
        RTIBool as_bool;
    } value;
    ArchParamValueType valueType;
};

/* {{{ ArchParameter_init
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The initializer for the ArchParameter object
 */
void ArchParameter_init(struct ArchParameter *me) {
    REDAInlineListNode_init(&me->parent);
    memset(me->key, 0, sizeof(me->key));
    memset(&me->value, 0, sizeof(me->value));
    me->valueType = APVT_Invalid;
}

/* }}} */
/* {{{ ArchParameter_new
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The constructor for the ArchParameter object
 */
struct ArchParameter * ArchParameter_new() {
    struct ArchParameter *retVal = calloc(1, sizeof(*retVal));
    if (retVal == NULL) {
        return NULL;
    }
    ArchParameter_init(retVal);
    return retVal;
}

/* }}} */
/* {{{ ArchParameter_delete
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The destructor for the ArchParameter object
 */
void ArchParameter_delete(struct ArchParameter *me) {
    memset(me, 0, sizeof(*me));
    free(me);
}

/* }}} */


/***************************************************************************
 * Architecture
 **************************************************************************/
/* Defines the container of all the key-value pairs extracted from the
 * project file that are associated to a particular target architecture.
 *
 * It essentially contains the name of the target and a linked list
 * of ArchParameter objects.
 *
 * An Architecture is also a node of a linked list itself.
 */
struct Architecture {
    struct REDAInlineListNode   parent;
    char                        target[MAX_STRING_SIZE];
    struct REDAInlineList       paramList;
};

/* {{{ Architecture_init
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Architecture initializer
 */
void Architecture_init(struct Architecture *me) {
    REDAInlineListNode_init(&me->parent);
    memset(me->target, 0, sizeof(me->target));
    REDAInlineList_init(&me->paramList);
}

/* }}} */
/* {{{ Architecture_new
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Architecture constructor
 */
struct Architecture * Architecture_new() {
    struct Architecture *retVal = calloc(1, sizeof(*retVal));
    if (retVal == NULL) {
        return NULL;
    }
    Architecture_init(retVal);
    return retVal;
}

/* }}} */
/* {{{ Architecture_finalize
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Architecture finalizer
 */
void Architecture_finalize(struct Architecture *me) {
    /* Delete all the parameter nodes */
    struct REDAInlineListNode *next;
    struct REDAInlineListNode *node = REDAInlineList_getFirst(&me->paramList);
    while (node != NULL) {
        next = REDAInlineListNode_getNext(node);
        ArchParameter_delete((struct ArchParameter *)node);
        node = next;
    }

    memset(me, 0, sizeof(*me));
}

/* }}} */
/* {{{ Architecture_delete
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Architecture destructor
 */
void Architecture_delete(struct Architecture *me) {
    Architecture_finalize(me);
    free(me);
}

/* }}} */


/***************************************************************************
 * Local Utility Functions
 **************************************************************************/
/* {{{ calcNDDSHOME
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Determines the root directory where RTI Connext DDS is installed (NDDEHOME)
 * The logic is the following:
 * - Look up the environment variable NDDSHOME. If is defined, good, make
 *   a copy and return
 *
 * - If is not defined, retrieve the absolute path where connext-config is
 *   invoked by looking at argv[0] and the current working directory
 *
 * - Then go one level up (assumes the connext-config is installed under
 *   NDDSHOME/bin) and use that directory as NDDSHOME
 *
 *
 * \return a malloc-allocated string containing NDDSHOME or NULL if an
 *         error occurred
 *
 *
 */
static char *calcNDDSHOME(const char *argv0) {
    char *tmp;
    char *retVal = NULL;

    retVal = calloc(PATH_MAX+1, 1);
    if (retVal == NULL) {
        fprintf(stderr, "Out of memory allocating NDDSHOME path\n");
        goto err;
    }

    /* Check if $NDDSHOME is defined */
    tmp = getenv("NDDSHOME");
    if (tmp != NULL) {
        strncpy(retVal, tmp, PATH_MAX);
        return retVal;
    }

    /* Else, obtain NDDSHOME from argv0 */
    if (argv0[0] == '/') {
        /* argv0 is absolute path */
        strncpy(retVal, argv0, PATH_MAX);
    } else {
        char *appPath;
        /*
         * argv0 is a relative path, build the absolute path by looking at 
         * the current working directory
         */
#ifdef USE_GETCWD
        int wr;
        if (getcwd(retVal, PATH_MAX) == NULL) {
            fprintf(stderr,
                    "Error: failed to read current working directory: "
                    "%s (errno=%d)\n",
                    strerror(errno),
                    errno);
            goto err;
        }
        wr = strlen(retVal);
        retVal[wr++] = '/';
        strncpy(&retVal[wr], argv0, PATH_MAX-wr);
#else
        char *pwd = getenv("PWD");
        /* Build the absolute path to this scripb from the current working directory */
        if (pwd == NULL) {
            fprintf(stderr,
                    "Error: env variable PWD is not defined, "
                    "unable to calculate NDDSHOME\n");
            goto err;
        }
        snprintf(retVal, PATH_MAX, "%s/%s", pwd, argv0);
#endif
        appPath = realpath(retVal, NULL);
        if (appPath == NULL) {
            fprintf(stderr, 
                    "Unable to compute NDDSHOME: %s (errno=%d)\n", 
                    strerror(errno), 
                    errno);
            goto err;
        }
        strncpy(retVal, appPath, PATH_MAX);
        /* realpath allocates the output string, need to free after making a copy... */
        free(appPath);
    }
    /*
     * Now 'retVal' contains the absolute path of this app, call dirname twice to
     * retrieve the directory parent of where this app is installed
     *
     * Note that dirname() modify the input buffer and returns a "pointer to
     * null-terminated strings"... most likely it just locate the last path
     * separator and replace it with '\0', meaning the returned string is
     * the same as the input string...
     */
    tmp = dirname(retVal);
    if (tmp != retVal) {
        strncpy(retVal, tmp, PATH_MAX);
    }
    tmp = dirname(retVal);
    if (tmp != retVal) {
        strncpy(retVal, tmp, PATH_MAX);
    }
    return retVal;

err:
    if (retVal != NULL) {
        free(retVal);
    }
    return NULL;
}

/* }}} */
/* {{{ validateNDDSHOME
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Validates that the calculated NDDSHOME is correct by looking at the 
 * directory 'resource' under NDDSHOME.
 *
 * \param NDDSHOME  pointer to the full path of NDDSHOME
 * \return          the boolean RTI_TRUE if the NDDSHOME was sucessfully
 *                  validated, RTI_FALSE if NDDSHOME is invalid
 *                  (a more detailed error is printed to stderr)
 */
static RTIBool validateNDDSHOME(const char *NDDSHOME) {
    struct stat info;
    char path[PATH_MAX+1];

    if (snprintf(path, PATH_MAX, "%s/resource", NDDSHOME) > PATH_MAX) {
        fprintf(stderr, "Path too long while testing NDDSHOME\n");
        return RTI_FALSE;
    }

    if (stat(path, &info) < 0) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Unable to identify NDDSHOME.\n");
            goto err;
        }
        /* else is another error */
        fprintf(stderr, 
                "Error retrieving information about resource dir: '%s': %s (errno=%d)\n",
                path,
                strerror(errno),
                errno);
        goto err;
    }
    if (!S_ISDIR(info.st_mode)) {
        fprintf(stderr,
                "Resource directory '%s' is not a valid directory\n",
                path);
        goto err;
    }
    return RTI_TRUE;

err:
    fprintf(stderr,
            "Make sure the environment variable NDDSHOME is defined\n");
    fprintf(stderr,
            "or that %s is installed correctly under $NDDSHOME/bin\n",
            APPLICATION_NAME);
    return RTI_FALSE;
}

/* }}} */
#ifndef NDEBUG
/* {{{ dumpArch
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Prints to stdout a dump of the list of architectures.
 *
 * This function is used when user specify --dump-all parameter and is meant
 * for debugging/testing ony.
 *
 * \param list      The list of Architecture objects
 *
 */
static void dumpArch(struct REDAInlineList *list) {
    struct REDAInlineListNode *archNode = NULL;

    for (archNode = REDAInlineList_getFirst(list); 
            archNode; 
            archNode = REDAInlineListNode_getNext(archNode)) {
        struct REDAInlineListNode *paramNode;
        struct Architecture *arch = (struct Architecture *)archNode;
        printf("arch='%s':\n", arch->target);

        for (paramNode = REDAInlineList_getFirst(&arch->paramList); 
                paramNode; 
                paramNode = REDAInlineListNode_getNext(paramNode)) {
            struct ArchParameter *param = (struct ArchParameter *)paramNode;
            switch(param->valueType) {
                case APVT_Boolean:
                    printf("\t\t%s=%s\n",
                            param->key,
                            (param->value.as_bool ? "TRUE" : "FALSE"));
                    break;

                case APVT_String:
                    printf("\t\t%s=\"%s\"\n",
                            param->key,
                            param->value.as_string);
                    break;

                case APVT_EnvVariable:
                    printf("\t\t%s=%s\n",
                            param->key,
                            param->value.as_string);
                    break;

                case APVT_ArrayOfStrings: {
                    size_t i;
                    const char *sep = "";
                    printf("\t\t%s=[", param->key);
                    for (i = 0; i < MAX_ARRAY_SIZE; ++i) {
                        if (param->value.as_arrayOfStrings[i][0] == '\0') {
                            break;
                        }
                        printf("%s\"%s\"",
                                sep,
                                param->value.as_arrayOfStrings[i]);
                        sep = ", ";
                    }
                    printf("]\n");
                    break;
                }

                default:
                    printf("** Error: unknow how to display type=%d\n",
                            param->valueType);
            }
        }
    }
}

/* }}} */
#endif
/* {{{ archGetParam
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Search the list of key-value pairs inside an Architecture object for the 
 * requested key.
 *
 * \param arch      The list of Architectures
 * \param key       The key to search
 * \return          A pointer to ArchParameter object if found or NULL if the
 *                  requested key is not found
 */
static struct ArchParameter * archGetParam(struct Architecture *arch, const char *key) {
    struct REDAInlineListNode *paramNode;
    for (paramNode = REDAInlineList_getFirst(&arch->paramList);
            paramNode != NULL;
            paramNode = REDAInlineListNode_getNext(paramNode)) {
        struct ArchParameter *param = (struct ArchParameter *)paramNode;
        if (strcmp(param->key, key) == 0) {
            return param;
        }
    }
    return NULL;
}

/* }}} */
/* {{{ unescapeString
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Given a string (potentially) containing escaped characters like \n, \",...
 * returns a static string of original values with the escaped characters
 * unescaped.
 *
 * Warning: This function is NON REENTRANT
 *
 * Warning: The maximum length of the string processed is MAX_CMDLINEARG_SIZE
 *
 * \param in        The input string to unescape
 * \return          A pointer to a static string containing the unescaped 
 *                  string.
 */
static char * unescapeString(char *in) {
    static char retVal[MAX_CMDLINEARG_SIZE];
    size_t rd = 0, wr = 0;
    size_t len = strlen(in);
    char ch;

    memset(retVal, 0, MAX_CMDLINEARG_SIZE);

    /* 
     * Input string has at least 1 char: means there is the potential
     * for an escape character.
     */
    if (len > 1) {
        /* Stop at the second to the last char */
        for (rd=0, wr=0, --len; 
                rd < len; 
                ++rd,++wr) {
            ch = in[rd];
            if (ch == '\\') {
                switch(in[++rd]) {
                    case '"': ch = '"'; break;
                    case '\'':  ch = '\''; break;
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    case 'a': ch = '\a'; break; 
                    case 'b': ch = '\b'; break;
                    case 'f': ch = '\f'; break;
                    case 'v': ch = '\v'; break;
                    case '\\':  ch = '\\'; break;
                    case '\?':  ch = '\?'; break;
                    default:
                        /* Unknown escape sequence, leave it 'as is' */
                        --rd;
                }
            }
            retVal[wr] = ch;
        }
    } 
    if (len > 0) {
        retVal[wr] = in[rd];
    }
    return &retVal[0];
}

/* }}} */
/* {{{ arrayFind
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Given a NULL-terminated array of strings, find the position of the matching string
 * Returns -1 if not found.
 */
static int arrayFind(const char **arr, const char *val) {
    int i = 0;
    while (arr[i] != NULL) {
        if (strcmp(arr[i], val) == 0) {
            return i;
        }
        ++i;
    }
    return -1;
}

/* }}} */
/* {{{ expandEnvVar
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Expands all the env variables from the inStr
 * Function is NON REENTRANT, pointer returned is a static char *
 *
 * An environment variable in the inStr is represented as $(VARIABLE).
 * (parentheses are required).
 *
 * The maximum length of the expanded string is MAX_CMDLINEARG_SIZE
 * The maximum length of an environment variable is MAX_STRING_SIZE
 *
 * If an env variable is not defined, no errors are reported (the 
 * whole variable is expanded with an empty string).
 *
 * \param inStr     the input string
 * \return          a pointer to a static string buffer containing the result
 *                  of the expansion or NULL if an error occurred.
 */
static char * expandEnvVar(const char *inStr) {
    static char retVal[MAX_CMDLINEARG_SIZE+1];
    char varName[MAX_STRING_SIZE+1];
    size_t wr = 0;     /* write pos */
    size_t rd;         /* read pos */

    memset(retVal, 0, sizeof(retVal));

    for (rd = 0; inStr[rd] != '\0'; ++rd) {
        if ((inStr[rd] == '$') && inStr[rd+1]=='(') {
            /* Found the beginning of an nev variable, extract the name */
            size_t i = 0;
            char *varValue = NULL;
            size_t varValueLen;

            memset(varName, 0, sizeof(varName));
            rd += 2;        /* Skip '$(' */
            while (inStr[rd] != ')') {    /* Read var name until ')' */
                if (inStr[rd] == '\0') {
                    fprintf(stderr, 
                            "Cannot find end of env variable in string: '%s'\n", 
                            inStr);
                    return NULL;
                }
                if (i == MAX_STRING_SIZE-1) {
                    fprintf(stderr, 
                            "Env variable name too large in input string: '%s'\n", 
                            inStr);
                    return NULL;
                }
                varName[i++] = inStr[rd++];
            }
            varValue = getenv(varName);
            if (varValue != NULL) {
                varValueLen = strlen(varValue);
                if (varValueLen + wr >= MAX_CMDLINEARG_SIZE-1) {
                    fprintf(stderr, 
                            "Expanded string too large when processing env variable '%s'", 
                            varName);
                    return NULL;
                }

                memcpy(&retVal[wr], varValue, varValueLen);
                wr += varValueLen;
            }
            continue;
        } 

        /* Else copy the character */
        retVal[wr++] = inStr[rd];
    }
    return retVal;
}

/* }}} */
/* {{{ printStringProperty
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Prints to stdout the value of the given string property for the 
 * specified architecture, optionally expanding the environment variables.
 *
 * \param arch      a pointer to the Architecture object
 * \param propName  the name of the property to lookup
 * \param expandVar the boolean that tells whether to expand the env 
 *                  variables or not.
 * \return          RTI_TRUE if success, RTI_FALSE if failed
 */
RTIBool printStringProperty(struct Architecture *arch,
        const char *propName,
        RTIBool expandVar) {
    char *toPrint;
    struct ArchParameter *ap = archGetParam(arch, propName);
    if (ap == NULL) {
        /* Variable not defined */
        return RTI_TRUE;
    }
    if (ap->valueType == APVT_String) {
        toPrint = &ap->value.as_string[0];
        if (expandVar == RTI_TRUE) {
            toPrint = expandEnvVar(toPrint);
            if (toPrint == NULL) {
                /* Error message has been already printed in expandEnvVar */
                return RTI_FALSE;
            }
        }

    } else if (ap->valueType == APVT_EnvVariable) {
        toPrint = &ap->value.as_string[0];

    } else {
        fprintf(stderr, 
                "Property '%s' is not a string or env variable for target %s\n", 
                propName, 
                arch->target);
        return RTI_FALSE;
    }
    puts(toPrint);
    return RTI_TRUE;
}

/* }}} */
/* {{{ getBooleanProperty
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Look for the given property and if set returns its boolean value.
 * Returns FALSE if the property is not defined.
 * If the property is not a boolean, prints an error and return FALSE (Release
 * mode), or abort (Debug mode).
 *
 * \param arch      a pointer to the Architecture object
 * \param propName  the name of the property to lookup
 * \return          RTI_TRUE or RTI_FALSE
 */
RTIBool getBooleanProperty(struct Architecture *arch,
        const char *propName) {
    char *toPrint;
    struct ArchParameter *ap = archGetParam(arch, propName);
    if (ap == NULL) {
        /* Variable not defined */
        return RTI_FALSE;
    }
    if (ap->valueType != APVT_Boolean) {
        fprintf(stderr,
                "Property '%s' is not a boolean (%d) for target '%s'\n",
                propName,
                ap->valueType,
                arch->target);
#ifndef NDEBUG
        abort();
#else
        return RTI_FALSE;
#endif
    }
    return ap->value.as_bool;
}

/* }}} */
/* {{{ ensureTargetSupportsCxx03
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * \param arch      a pointer to the Architecture object
 * \return          RTI_TRUE if the architecture supports Cxx03, FALSE otherwise
 */
RTIBool ensureTargetSupportsCxx03(struct Architecture *arch) {
    if (!getBooleanProperty(arch, "$SUPPORTS_CPP03")) {
        fprintf(stderr,
                "Error: target '%s' does not support C++03\n",
                arch->target);
        return RTI_FALSE;
    }
    return RTI_TRUE;
}

/* }}} */
/* {{{ ensureTargetSupportsCxx11
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * \param arch      a pointer to the Architecture object
 * \return          RTI_TRUE if the architecture supports Cxx03, FALSE otherwise
 */
RTIBool ensureTargetSupportsCxx11(struct Architecture *arch) {
    if (!getBooleanProperty(arch, "$SUPPORTS_CPP11")) {
        fprintf(stderr,
                "Error: target '%s' does not support C++11\n",
                arch->target);
        return RTI_FALSE;
    }
    return RTI_TRUE;
}

/* }}} */
/* {{{ joinStringArrayProperties
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Find a property 'propertyName' in the given architecture, then:
 * - Ensure the property exist
 * - Ensure the property is an array of string
 * - Unwrap the array (join) all the strings using a space as separator and
 *   a given prefix to be prepended before each string
 *
 * Note: the returned string is terminated with a space (so you can concatenate calls)
 *
 * Note: the prefix is required, use "" (empty string) if you don't want to
 *       have a prefix.
 *
 * \param arch      a pointer to the architecture where to look for the
 *                  given property
 * \param bufOut    a pointer to the string buffer where to write the result
 * \param bufSize   the maximum number of characters to write
 * \param propName  the name of the property to look in the key-value pairs
 * \param prefix    the string to be prepended to each array element
 * \return          the number of characters written or -1 if an error occurred
 *                  Returns 0 if the array is empty or the property is not 
 *                  defined.
 */
static int joinStringArrayProperties(struct Architecture *arch, 
        char *bufOut,
        int bufSize,
        const char *propName,
        const char *prefix) {
    struct ArchParameter *ap = archGetParam(arch, propName);
    int i = 0;
    int wr = 0;
    if (ap == NULL) {
        return 0;
    }
    if (ap->valueType != APVT_ArrayOfStrings) {
        fprintf(stderr, 
                "Property '%s' is not an array of strings for target %s\n", 
                propName, 
                arch->target);
        return -1;
    }

    while (ap->value.as_arrayOfStrings[i][0] != '\0') {
        wr += snprintf(&bufOut[wr], 
                bufSize-wr, 
                "%s%s ", 
                prefix, 
                ap->value.as_arrayOfStrings[i]);
        if (wr >= bufSize) {
            fprintf(stderr, 
                    "Cmd line too large expanding param '%s' for target %s\n", 
                    propName, 
                    arch->target);
            return -1;
        }
        ++i;
    }

    return wr;
}

/* }}} */
/* {{{ printCompositeFlagsProperties
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Given an architecture, prints the flags composed by getting the values of 
 * more than one properties.
 *
 * \param arch      Pointer to the architecture to use
 * \param props     a NULL terminated array of strings identifying either 
 *                  the name of the property to look up (if string starts with
 *                  '$'), or a verbatim string that need to be appended.
 *                  If the name starts with a '$', then the property value is 
 *                  looked up from the architecture.
 *                  Each property value is expected to be an array of strings 
 *                  identifying the flags. Each flag is prefixed with a "-" 
 *                  (or "-I" for $INCLUDES)
 * \param expandVar a boolean that tells whether to expand environment 
 *                  variables (RTI_TRUE) or not (RTI_FALSE).
 * \param envShell  a boolean that if expandVar == FALSE, what is the form of
 *                  env variable reference to use: shell (RTI_TRUE) means
 *                  that variables are using the form ${...} (with braces).
 *                  If envShell==RTI_FALSE, the variables are using a 
 *                  Makefile-style form: $(...) (with the parentheses)
 * \return return   RTI_FALSE if an error occurred, or RTI_TRUE if success
 */
RTIBool printCompositeFlagsProperties(struct Architecture *arch, 
        const char **props, 
        RTIBool expandVar,
        RTIBool envShell) {
    char line[MAX_CMDLINEARG_SIZE];
    int wr = 0;
    int rc = 0;
    int propIdx;
    char *toPrint;
    const char *prefix;

    memset(line, 0, sizeof(line));

    for (propIdx = 0; props[propIdx] != NULL; ++propIdx) {
        /* Just a convenience alias */
        const char *propName = &props[propIdx][0];

        /* Property or verbatim string? */
        if (propName[0] == '$') {
            /* In the template files, the "$INCLUDES" values need to be prefixed 
             * by '-I', all the other flags need to be prefixed by a "-"
             */
            prefix = (strcmp(propName, "$INCLUDES") == 0) ? "-I" : "-";
            rc = joinStringArrayProperties(arch, 
                    &line[wr], 
                    MAX_CMDLINEARG_SIZE-wr, 
                    propName, 
                    prefix);
            if (rc == -1) {
                return RTI_FALSE;
            }
        } else {
            /* Copy verbatim the propName */
            rc = snprintf(&line[wr], 
                    MAX_CMDLINEARG_SIZE-wr, 
                    "%s ",      /* Always add a space at the end */
                    propName);
            if (rc > MAX_CMDLINEARG_SIZE-wr) {
                fprintf(stderr, 
                        "Command line string too long while appending static flags\n");
                return RTI_FALSE;
            }
        }
        wr += rc;
    }
    
    toPrint = &line[0];
    if (expandVar == RTI_TRUE) {
        toPrint = expandEnvVar(toPrint);
        if (toPrint == NULL) {
            return RTI_FALSE;
        }
    } else {
        /* 
         * Do not expand env variable: make sure all the env
         * variabels are in the form $(...) or ${...}, depending
         * if the argument --shell is provided
         */
        char open_from = envShell ? '(' : '{';
        char open_to   = envShell ? '{' : '(';
        char close_from = envShell ? ')' : '}';
        char close_to   = envShell ? '}' : ')';
        char *ptr;
        for (ptr = toPrint; (*ptr != '\0'); ++ptr) {
            if ((*ptr == '$') && (*(ptr+1) == open_from)) {
                ++ptr;
                *ptr = open_to;
                ptr = strchr(ptr, close_from);
                if (ptr == NULL) {
                    fprintf(stderr, 
                            "Cannot find end of env variable in line=%s\n",
                            toPrint);
                    return RTI_FALSE;
                }
                *ptr = close_to;
            }
        }

    }
    /*
     * Now the toPrint string should always have a space at the end, remove it
     * (this is needed so we can use the dump-all.sh script to compare the results
     * with the javascript version)
     */
    wr = strlen(toPrint)-1;
    while (isspace(toPrint[wr])) {
        toPrint[wr--] = '\0';
    }
    puts(unescapeString(toPrint));
    return RTI_TRUE;
}

/* }}} */
/* {{{ parseStringInQuotes
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Given a char * identifying a string included in single or double quotes, 
 * extracts the content of the string.
 *
 * For example, if line = abc "def"zzz
 * then this function will search for the first quite (after "abc ") then
 * copy the string "def" inside bufOut (without the quotes).
 *
 * The copy operation ensures the max length of the bufOut does not exceed
 * the value given in bufOutLen.
 *
 * \param line      pointer to a string containing a string in quotes.
 * \param bufOut    pointer to the string where to write the content
 * \param bufOutLen max length of bufOut
 * \param errOut    A pointer to a constant string containing an error
 *                  message (to be used only if an error occurred).
 * \return          the pointer to the end quote (inside the original line) 
 *                  if success, or NULL if an error occurred.
 *                  In case of error *ptrErr contains the error message.
 */
char * parseStringInQuotes(char *line,
        char *bufOut,
        int bufOutLen,
        const char **errOut) {
    char endl = '"';        /* the char to identify the end of the string */
    char *tmp2;
    char *tmp1 = line;

    /* Scans the input line until we identify a single or double quote 
     * NOTE: the following code is NOT the same thing as performing
     * two strchr like this:
     *      tmp1 = strchr(line, '"')
     *      if (!tmp1) {
     *          tmp1 = strchr(line, '\'');
     *      }
     * because the above code will fail to correctly interpret
     * a string like this:
     *      'MyValue=\"hello\"'
     * (the first strchr will identify the double quote).
     */
    while (*tmp1 != '\0') {
        if (*tmp1 == '"') {
            break;
        }
        if (*tmp1 == '\'') {
            /* 
             * If we identify a string in single quote, look for the end
             * by searching the next single quote
             */
            endl='\''; 
            break;
        }
        ++tmp1;
    }
    if (tmp1 == NULL) {
        tmp1 = strchr(line, '\'');
        if (tmp1 == NULL) {
            *errOut = "Failed to find the beginning of string delimiter";
            return NULL;
        }
    }
    ++tmp1;

    tmp2 = strchr(tmp1, endl);
    if (tmp2 == NULL) {
        *errOut = "Failed to find the end of string delimiter";
        return NULL;
    }
    if (tmp2-tmp1 > bufOutLen) {
        *errOut = "String too long";
        return NULL;
    }
    strncpy(bufOut, tmp1, (tmp2-tmp1));
    return tmp2;;
}

/* }}} */
/* {{{ processArchDefinitionLine
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Parses the architecture definition line.
 *
 * The line has the following form:
 *      #arch("i86Sol2.9","gcc3.3.2", {
 * or (because of an error in the platform file):
 *      #arch("armv7Linux3.0","gcc4.6.1.cortex-a9" {
 *
 * The approach here is to search and identify the two strings and copy them 
 * inside 'target', and 'compiler', instead of looking for the commas.
 *
 * Each target and compiler must be MAX_STRING_SIZE in size.
 *
 * NOTE: This function assumes the target and compiler are enclosed
 *       in double quotes (not single quote).
 *
 * \param target    pointer to the string where the "target" platform is
 *                  copied. Must be at least MAX_STRING_SIZE.
 * \param compiler  pointer to the string where the compiler name is 
 *                  copied. Must be at least MAX_STRING_SIZE.
 * \return          RTI_TRUE if success, RTI_FALSE if an error occurred
 *                  (error message is printed to stderr)
 *
 */
RTIBool processArchDefinitionLine(const char *line, 
        char *target, 
        char *compiler) {
    char *tmp1;
    char *tmp2;

    memset(target, 0, MAX_STRING_SIZE);
    memset(compiler, 0, MAX_STRING_SIZE);
    tmp1 = strchr(line, '"');
    if (tmp1 == NULL) {
        fprintf(stderr, "Cannot find start target in arch definition: '%s'\n", 
                line);
        return RTI_FALSE;
    }
    ++tmp1;

    tmp2 = strchr(tmp1, '"');
    if (tmp2 == NULL) {
        fprintf(stderr, "Cannot find end target in arch definition: '%s'\n", 
                line);
        return RTI_FALSE;
    }
    if (tmp2 <= tmp1) {
        fprintf(stderr, "Unable to identify target arch in line: '%s'\n", 
                line);
        return RTI_FALSE;
    }

    if (tmp2-tmp1 > MAX_STRING_SIZE) {
        fprintf(stderr, "Target arch name '%s' too large: (%ld, max=%d)\n", 
                tmp1, (tmp2-tmp1), MAX_STRING_SIZE);
        return RTI_FALSE;
    }
    memcpy(&target[0], tmp1, (tmp2-tmp1));

    /* Now parse and copy the compiler arch */
    tmp1 = strchr(tmp2+1, '"');
    if (tmp1 == NULL) {
        fprintf(stderr, "Cannot find start compiler in arch definition: '%s'\n", 
                line);
        return RTI_FALSE;
    }
    ++tmp1;

    tmp2 = strchr(tmp1, '"');
    if (tmp2 == NULL) {
        fprintf(stderr, "Cannot find end compiler in arch definition: '%s'\n", 
                line);
        return RTI_FALSE;
    }

    /* Some targets have an empty compiler name */
    if (tmp2 > tmp1) {
        if (tmp2-tmp1 > MAX_STRING_SIZE) {
            fprintf(stderr, 
                    "Compiler name '%s' too large: (%ld, max=%d) for target='%s'\n", 
                    tmp1, 
                    (tmp2-tmp1), 
                    MAX_STRING_SIZE, 
                    target);
            return RTI_FALSE;
        }
        memcpy(&compiler[0], tmp1, (tmp2-tmp1));
    }
    return RTI_TRUE;
}

/* }}} */
/* {{{ processKeyValuePairLine
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Parses a key-value pair line with the following form:
 * "      KEY: <value> "
 * Notes:
 * - The number of heading space is not fixed
 * - Key _usually_ starts with '$', but there are some variables: "TR_VAR": "..."
 *
 * The approach is to split the line at the ':' point, then trim the
 * key and the value, finally copy it in the param argument.
 * 
 * NOTE: This function modify the 'line' buffer
 *
 * \param line      pointer to the line to parse
 * \param param     pointer to the ArchParameter object used to stored the
 *                  parsed key and value.
 * \return          RTI_TRUE if success, RTI_FALSE if an error occurred
 */
typedef enum {
    ProcessLineResult_Done,
    ProcessLineResult_Error,
    ProcessLineResult_Continue
} ProcessLineResult;

ProcessLineResult processKeyValuePairLine(char *line, struct ArchParameter *param) {
    char *key;
    char *val;
    char *tmp;

    memset(param, 0, sizeof(*param));
    tmp = strchr(line, ':');   /* find the ':' delimiter */
    if (tmp == NULL) {
        fprintf(stderr, 
                "Cannot find key-value pair delimiter ':' in line: '%s'\n", 
                line);
        return ProcessLineResult_Error;
    }
    val = tmp+1;    /* Skip the ':' */

    /* Trim the end of the key */
    do {
        *tmp = '\0'; --tmp;
    } while (isspace(*tmp) || (*tmp == '"'));

    /* Trim the beginning of the key */
    key = line;
    while (isspace(*key) || (*key == '"')) {
        ++key;
    }

    if (strlen(key) > sizeof(param->key)) {
        fprintf(stderr, "Key too long: '%s' (>%ld)\n",
                key, 
                sizeof(param->key));
        return ProcessLineResult_Error;
    }

    strcpy(param->key, key);

    /* Trim the beginning of the value */
    while (isspace(*val)) ++val;

    /* Identify the kind of value */
    if (strncmp(val, "true", 4) == 0) {
        /* Booleans are "true" ... */
        param->valueType = APVT_Boolean;
        param->value.as_bool = 1;
        return ProcessLineResult_Done;
    }
    if (strncmp(val, "false", 5) == 0) {
        /* ...or "false" */
        param->valueType = APVT_Boolean;
        param->value.as_bool = 0;
        return ProcessLineResult_Done;
    }
    if ((*val == '"') || (*val == '\'')) {
        /* Strings are in single or double quotes */
        const char *errMsg;
        param->valueType = APVT_String;
        val = parseStringInQuotes(val, 
                &param->value.as_string[0], 
                sizeof(param->value.as_string), 
                &errMsg);
        if (val == NULL) {
            fprintf(stderr,
                    "Error: %s while parsing key-value pair line: '%s'\n",
                    errMsg, 
                    line);
            return ProcessLineResult_Error;
        }
        return ProcessLineResult_Done;
    }

    if (*val == '[') {
        /* Value is an array of strings enclosed in single or double quotes */
        size_t idx = 0;
        const char *errMsg;
        /* Does the line contains the full array? */
        if (strchr(val, ']') == NULL) {
            /* No: array continues to the next line */
            return ProcessLineResult_Continue;
        }
        param->valueType = APVT_ArrayOfStrings;
        while ((strchr(val, '"') != NULL) || (strchr(val, '\'') != NULL)) {
            val = parseStringInQuotes(val, 
                    &param->value.as_arrayOfStrings[idx][0], 
                    MAX_STRING_SIZE,
                    &errMsg);
            if (val == NULL) {
                fprintf(stderr,
                        "Error: %s while parsing key-value pair line: '%s'\n",
                        errMsg, 
                        line);
                return ProcessLineResult_Error;
            }
            ++val;
            ++idx;
            if (idx == MAX_ARRAY_SIZE) {
                fprintf(stderr,
                        "Error: reached max number of elements in array: %ld, increase MAX_ARRAY_SIZE and rebuild\n",
                        idx);
                return ProcessLineResult_Error;
            }
        }

        return ProcessLineResult_Done;
    }
    if (*val == '$') {
        /* Values starting with a '$' are env variable */
        param->valueType = APVT_EnvVariable;
        tmp = strchr(val, ',');
        if (tmp == NULL) {
            fprintf(stderr,
                    "Unable to find end of env variable delimiter in line '%s'\n",
                    line);
            return ProcessLineResult_Error;
        }
        *tmp = '\0';

        /* When taking the env variable, skip the variable name.
         * For example, in the following:
         *      $CPU : $CPU.ppcbe
         * the value.as_string will get the value "ppcbe"
         */
        tmp = strchr(val, '.');
        if (tmp == NULL) {
            fprintf(stderr,
                    "Unable to find '.' delimiter of env variable in line '%s'\n",
                    line);
            return ProcessLineResult_Error;
        }

        strcpy(param->value.as_string, tmp+1);

        return ProcessLineResult_Done;
    }

    return ProcessLineResult_Done;
}

/* }}} */
/* {{{ appendNextLine
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
char *appendNextLine(FILE *fp, char *currLine, unsigned int *pLineCount) {
    char *line = NULL;
    size_t lineLen;
    size_t currLineLen = 0;
    char *tmp;
    RTIBool longComment = RTI_FALSE;
    char *trimLine;
    ssize_t nRead;

    if (currLine) currLineLen = strlen(currLine);

    for (;;) {
        if (line) {
            free(line);
            line = NULL;
        }
        lineLen = 0;

        /* Always allocate a new buffer */
        if (getline(&line, &lineLen, fp) == -1) {
            /* EOF */
            break;
        }
        ++*pLineCount;

        /* Identify multi-line comments */
        if ((tmp = strstr(line, "#*")) != NULL) {
            *tmp = '\0';
            longComment = RTI_TRUE;
        }

        trimLine = line;
        if ((longComment == RTI_TRUE) && ((tmp = strstr(trimLine, "*#")) != NULL)) {
            longComment = RTI_FALSE;
            trimLine = tmp+2;
            /* NOTE: Multi line comments are not handled correctly */
        }

        /*
         * HACK: In Connext 5.3.0 the first line is:
         *     "F## $Id$"
         * skip it...
         */
        if (strncmp(trimLine, "F## ", 4) == 0) {
            continue;
        }

        /*
         * Skip single-line comments
         * For example:
         *      ## (c) Copyright, Real-Time Innovations, Inc. 2001...
         *      ## ....
         */
        if ( (tmp = strstr(trimLine, "##")) != NULL) {
            *tmp = '\0';
        }

        /* 
         * HACK: In the code there are some lines like the following:
         *      #    $C_LINKER_FLAGS : ["std=libc++"],
         * It's unknown why they are there, but they need to be skipped...
         */
        if (strncmp(trimLine, "#    ", 5) == 0) {
            continue;
        }

        /* Trim read line and remove the newline at the end */
        nRead = strlen(trimLine);
        while (isspace(trimLine[nRead])) {
            trimLine[nRead--] = '\0';
        }
        while (isspace(*trimLine)) ++trimLine;

        /* Skip empty lines */
        if (trimLine[0] == '\0') {
            continue;
        }

        /* Append trimLine to currLine, reallocating its space */
        currLineLen += strlen(trimLine)+1;
        if (currLine) {
            currLine = realloc(currLine, currLineLen);
        } else {
            /* Note: realloc(NULL) behaves like malloc() not like calloc(), we
             *       still need to set the string to empty after allocation
             */
            currLine = calloc(1, currLineLen);
        }
        if (!currLine) {
            fprintf(stderr, "realloc failed, size=%lu\n", currLineLen);
            return NULL;
        }
        strncat(currLine, trimLine, currLineLen - 1);

        /* Free line */
        free(line);
        return currLine;
    }
    /* Reached the end of file */
    return currLine;
}

/* }}} */
/* {{{ readPlatformFile
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Top-level function that parses the platform file and populates the
 * archDef list
 *
 * This parser read and parse one line at the time. It uses a simple state
 * machine to track the state of the parser when processing a line.
 *
 * \param filePath      pointer to the full path of the platform file
 * \param archDef       pointer to the list where to store the parsed arches
 * \return              RTI_TRUE if success, RTI_FALSE if an error occurred
 */
RTIBool readPlatformFile(const char *filePath, 
        struct REDAInlineList *archDef) {
    RTIBool ok = RTI_FALSE;
    FILE *fp;
    char *line = NULL;
    ReadStateMachine rsm = RSM_TOPLEVEL;
    struct Architecture *currentArch = NULL;
    struct ArchParameter *currentParam = NULL;
    char target[MAX_STRING_SIZE];
    char compiler[MAX_STRING_SIZE];
    RTIBool concatLines = RTI_FALSE;        /* Value spans multiple lines */
    unsigned int lineCount = 0;
    char *origLine;

    fp = fopen(filePath, "r");
    if (fp == NULL) {
        fprintf(stderr, "Platform file not found: %s\n", filePath);
        return RTI_FALSE;
    }

    for (;;) {
        if (!concatLines) {
            free(line);
            line = NULL;
            concatLines = RTI_FALSE;
        }
        line = appendNextLine(fp, line, &lineCount);
        if (!line) {
            /* Out of memory */
            return RTI_FALSE;
        }

        /* Skip empty lines */
        if (line[0] == '\0') {
            continue;
        }

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
         * FSM State: TOP LEVEL
         * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
        if (rsm == RSM_TOPLEVEL) {
            /* Identify directives (#macro, #arch) first */

            /*
             * Detect macro section
             * Example:
             *      #macro (arch $name $compiler $params)
             */
            if (strncmp(line, "#macro", 6) == 0) {
                rsm = RSM_MACRO;
                continue;
            }

            if (strncmp(line, "#set", 4) == 0) {
                // Skip the #set command
                // NOTE: currently we just skip the line since
                //       this command is not used.
                continue;
            }

            /* Detect arch section
             * Example:
             *      #arch("sparc64Sol2.10","gcc3.4.2", {
             */
            if (strncmp(line, "#arch(", 6) == 0) {
                currentArch = Architecture_new();
                if (currentArch == NULL) {
                    fprintf(stderr,
                            "Out of memory allocating currentArch\n");
                    goto done;
                }
                if (processArchDefinitionLine(line, 
                            &target[0], 
                            &compiler[0]) == RTI_FALSE) {
                    goto done;
                }
                if (snprintf(currentArch->target, 
                        MAX_STRING_SIZE, 
                        "%s%s", 
                        target, 
                        compiler) > MAX_STRING_SIZE) {
                    fprintf(stderr, 
                            "Composite target string too long for platform: "
                            "%s\n", 
                            line);
                    goto done;
                }
                rsm = RSM_ARCH;
                continue;
            }

            /* Unexpected */
            fprintf(stderr, 
                    "ERROR: unknown how to parse line: '%s'\n", 
                    line);
            goto done;
        }

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
         * FSM State: MACRO
         * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
        if (rsm == RSM_MACRO) {
            /* In macro state, ignore everything until the #end symbol */
            if (strncmp(line, "#end", 4) == 0) {
                rsm = RSM_TOPLEVEL;
            }
            continue;
        }

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
         * FSM State: ARCH (inside an Architecture definition)
         * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
        if (rsm == RSM_ARCH) {
            ProcessLineResult plres;
            /* Exit ARCH after we detect the '})\n' pattern */
            if (strncmp(line, "})\n", 3) == 0) {

                /* Filter out the architectures that need to be skipped:
                 *  - $HIDDEN=true
                 *  - All windows targets
                 *  - All the iOS targets
                 * In general remove all the architectures that don't define 
                 * the macros for the toolset (C_COMPILER, C_LINKER, ...)
                 */
                RTIBool skipArch = RTI_FALSE;
                struct ArchParameter * hidden = archGetParam(currentArch, 
                        "$HIDDEN");
                if ((hidden != NULL) && 
                        ((hidden->valueType == APVT_Boolean) && 
                            hidden->value.as_bool)) {
                    skipArch = RTI_TRUE;
                }
                if ((archGetParam(currentArch, "$C_COMPILER") == NULL) ||
                        (archGetParam(currentArch, "$C_LINKER") == NULL) ||
                        (archGetParam(currentArch, "$CXX_COMPILER") == NULL) ||
                        (archGetParam(currentArch, "$CXX_LINKER") == NULL)) {
                    skipArch = RTI_TRUE;
                }

                if (skipArch == RTI_TRUE) {
                    /* Drop skipped architectures */
                    Architecture_delete(currentArch);
                } else {
                    /* Valid, push the arch to the archDef */
                    REDAInlineList_addNodeToBackEA(archDef, 
                            &currentArch->parent);
                }

                currentArch = NULL;
                rsm = RSM_TOPLEVEL;
                continue;
            }

            /* process the arch line */
            if (currentParam == NULL) {
                /* Note: if not null, it was previously created for a multi-line array */
                currentParam = ArchParameter_new();
            }
            if (currentParam == NULL) {
                fprintf(stderr, "Out of memory allocating currentParam\n");
                goto done;
            }
            origLine = strdup(line);
            plres = processKeyValuePairLine(line, currentParam);
            if (plres == ProcessLineResult_Error) {
                free(origLine);
                goto done;
            }
            if (plres == ProcessLineResult_Continue) {
                /* Need to concatenate with the next line and try again */
                //ArchParameter_delete(currentParam);
                //currentParam = NULL;
                concatLines = RTI_TRUE;
                memcpy(line, origLine, strlen(origLine)+1);
                free(origLine);
                continue;
            }

            /* Got a valid line: */
            REDAInlineList_addNodeToBackEA(&currentArch->paramList, 
                    &currentParam->parent);
            currentParam = NULL;
            concatLines = RTI_FALSE;
            continue;
        }
    
        fprintf(stderr, "Error: unexpected state: %d\n", rsm);
        goto done;
    }
    ok = RTI_TRUE;

done:
    if (fp != NULL) {
        fclose(fp);
    }
    if (line != NULL) {
        free(line);
    }
    if (currentArch != NULL) {
        Architecture_delete(currentArch);
    }
    if (currentParam != NULL) {
        ArchParameter_delete(currentParam);
    }
    if (ok == RTI_FALSE) {
        fprintf(stderr, 
                "Error occurred while parsing line %u\n", 
                lineCount);
    }
    return ok;
}
/* }}} */
/* {{{ usage
 * -----------------------------------------------------------------------------
 */
void usage() {
    printf("RTI Connext DDS Config version %s\n", 
            APPLICATION_VERSION); 
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("Usage:\n");
    printf("    %s -h|--help        Show this help\n", 
            APPLICATION_NAME);
    printf("    %s -V|--version     Prints version number\n", 
            APPLICATION_NAME);
    printf("    %s --list-all       List all platform architectures supported\n", 
            APPLICATION_NAME);
    printf("    %s --list-installed List the installed architectures\n",
            APPLICATION_NAME);
#ifndef NDEBUG
    printf("    %s --dump-all       Dump all platforms and all settings (testing only)\n", 
            APPLICATION_NAME);
#endif
    printf("    %s [modifiers] <what> [targetArch]\n", 
            APPLICATION_NAME);
    printf("\n");
    /*      0        1         2         3         4         5         6         7         8 */
    /*      12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
    printf("Where [modifiers] are:\n");
    printf("    --static      use static linking against RTI Connext DDS\n");
    printf("    --debug       use debug version of the RTI Connext DDS libraries\n");
    printf("    --sh          use shell-like variable expansion (vs. make-like variables)\n");
    printf("    --noexpand    do not expand environment variables in output\n");
    printf("    --libmsg      include libraries for building request/reply apps\n");
/*    printf("    --librs       include libraries for building Routing Service apps/plugins\n"); */
/*    printf("    --libsecurity include libraries for building security applications\n"); */
    printf("\n");
    printf("Required argument <what> is one of:\n");
    printf("  C API:\n");
    printf("    --ccomp       output the C compiler to use\n");
    printf("    --cflags      output all pre-processor and compiler flags\n");
    printf("    --clink       output the C linker to use\n");
    printf("    --ldflags     output the linker flags\n");
    printf("    --ldlibs      output the required libraries\n");
    printf("  Traditional C++ API:\n");
    printf("    --cxxcomp     output the C++ compiler to use\n");
    printf("    --cxxflags    output all pre-processor and compiler flags\n");
    printf("    --cxxlink     output the C++ linker to use\n");
    printf("    --ldxxflags   output the linker flags\n");
    printf("    --ldxxlibs    output the required libraries\n");
    printf("  Modern C++ API (C++-03):\n");
    printf("    --cxx03comp   output the C++ compiler to use\n");
    printf("    --cxx03flags  output all pre-processor and compiler flags\n");
    printf("    --cxx03link   output the C++ linker to use\n");
    printf("    --ldxx03flags output the linker flags\n");
    printf("    --ldxx03libs  output the required libraries\n");
    printf("  Modern C++ API (C++-11):\n");
    printf("    --cxx11comp   output the C++ compiler to use\n");
    printf("    --cxx11flags  output all pre-processor and compiler flags\n");
    printf("    --cxx11link   output the C++ linker to use\n");
    printf("    --ldxx11flags output the linker flags\n");
    printf("    --ldxx11libs  output the required libraries\n");
    printf("  Miscellaneous:\n");
    printf("    --os          output the OS (i.e. UNIX, ANDROID, IOS, ...)\n");
    printf("    --platform    output the Platform (i.e. i86, x64, armv7a, ...)\n");
    printf("\n");
    printf("Optional argument [targetArch] is one of the supported target architectures.\n");
    printf("If not specified, uses environment variable NDDSARCH.\n");
    printf("Use `--list-all` or `--list-installed` to print a list of architectures\n");
}

/* }}} */
/* {{{ main
 * -----------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    const char *argOp = NULL;
    const char *argTarget = NULL;
    RTIBool argStatic = RTI_FALSE;
    RTIBool argDebug = RTI_FALSE;
    RTIBool argShell = RTI_FALSE;
    RTIBool argExpandEnvVar = RTI_TRUE;
    RTIBool argMsg = RTI_FALSE;
    /*
    RTIBool argRs = RTI_FALSE;
    RTIBool argSec = RTI_FALSE;
    */
    char *NDDSHOME = NULL;
    char *platformFile = NULL;
    int retCode = APPLICATION_EXIT_UNKNOWN;
    struct REDAInlineList *archDef = NULL;
    struct Architecture *archTarget;
    char *nddsExtraLib = NULL;
    char *nddsExtraLibCPP = NULL;
    char *nddsExtraLibCPP03 = NULL;
    char *nddsFlags = NULL;
    char *nddsCPP03Flags = NULL;
    char *nddsCLibs = NULL;
    char *nddsCPPLibs = NULL;
    char *nddsCPP03Libs = NULL;
    const char *libSuffix;

    if (argc <= 1) {
        usage();
        retCode = APPLICATION_EXIT_INVALID_ARGS;
        goto done;
    }
    if ((argc == 2) && ((strcmp(argv[1], "-h") == 0) ||
                (strcmp(argv[1], "--help") == 0) )) {
        usage();
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
    if ((argc == 2) && ((strcmp(argv[1], "-V") == 0) ||
                (strcmp(argv[1], "--version") == 0))) {
        printf("%s v.%s\n", APPLICATION_NAME, APPLICATION_VERSION);
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
    if ((argc == 2) && (
#ifndef NDEBUG
                (strcmp(argv[1], "--dump-all") == 0) ||
#endif
                (strcmp(argv[1], "--list-installed") == 0) ||
                (strcmp(argv[1], "--list-all") == 0))) {
        argOp = argv[1];

    } else {
        int i;

        /* Parse command line with at least 1 arguments */
        if (argc < 2) {
            usage();
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }

        for (i = 1; i < argc; ++i) {
            if ((strcmp(argv[i], "--static") == 0)) {
                argStatic = RTI_TRUE;
                continue;
            }
            if ((strcmp(argv[i], "--debug") == 0)) {
                argDebug = RTI_TRUE;
                continue;
            }
            if ((strcmp(argv[i], "--shell") == 0) || 
                    (strcmp(argv[i], "--sh") == 0)) {
                argShell = RTI_TRUE;
                continue;
            }
            if ((strcmp(argv[i], "--noexpand") == 0)) {
                argExpandEnvVar = RTI_FALSE;
                continue;
            }
            if ((strcmp(argv[i], "--libmsg") == 0)) {
                argMsg = RTI_TRUE;
                continue;
            }
            if ((strcmp(argv[i], "-h") == 0) || 
                    (strcmp(argv[i], "--help") == 0)) {
                usage();
                retCode = APPLICATION_EXIT_SUCCESS;
                goto done;
            }
            if (arrayFind(&VALID_WHAT[0], argv[i]) != -1) {
                argOp = argv[i];
                continue;
            }
            if (i == argc-1) {
                /* Last unrecognized argument: assume target */
                argTarget = argv[i];
                continue;
            }
            fprintf(stderr, "Error: invalid argument: %s\n", argv[i]);
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }
        /* Make sure the last argument is as intended the target */
        if (argTarget && (argTarget[0] == '-')) {
            fprintf(stderr, 
                    "Error: unknown argument: %s\n"
                    "Use --list-all or --list-installed to print the architectures\n", 
                    argTarget);
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }
        if (argTarget == NULL) {
            /* 
             * argTarget is not defined, look at the environment variable
             * $NDDSARCH
             */
            char *tmp;
            tmp = getenv("NDDSARCH");
            if (tmp == NULL) {
                fprintf(stderr,
                        "Target architecture not specified and NDDSARCH not defined\n"
                        "Use --list-all or --list-installed to print the architectures\n");
                retCode = APPLICATION_EXIT_INVALID_ARGS;
                goto done;
            }
            argTarget = tmp;
        }
    }
    if (!argOp) {
        fprintf(stderr, "Missing operation. Use `%s --help` for usage information.\n", APPLICATION_NAME);
        retCode = APPLICATION_EXIT_INVALID_ARGS;
        goto done;
    }

    /* Determine NDDSHOME and platform file */
    NDDSHOME = calcNDDSHOME(argv[0]);
    if (NDDSHOME == NULL) {
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }
    if (validateNDDSHOME(NDDSHOME) == RTI_FALSE) {
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }

    /* Complete building the path to the platform.vm file: */
    platformFile = calloc(PATH_MAX+1, 1);
    if (platformFile == NULL) {
        fprintf(stderr, "Out of memory allocating platformFile path\n");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }
    snprintf(platformFile, PATH_MAX, "%s/%s", NDDSHOME, NDDS_PLATFORM_FILE);

    /* Allocate the archDef list */
    archDef = calloc(1, sizeof(*archDef));
    if (archDef == NULL) {
        fprintf(stderr, "Out of memory allocating archDef\n");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }
    REDAInlineList_init(archDef);

    /* Read and parse platform file */
    readPlatformFile(platformFile, archDef);
#ifndef NDEBUG
    if ((strcmp(argOp, "--dump-all") == 0)) {
        dumpArch(archDef);
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
#endif

    if ((strcmp(argOp, "--list-all") == 0)) {
        struct REDAInlineListNode *archNode;
        for (archNode = REDAInlineList_getFirst(archDef); 
                archNode != NULL; 
                archNode = REDAInlineListNode_getNext(archNode)) {
            struct Architecture *arch = (struct Architecture *)archNode;
            printf("%s\n", arch->target);
        }
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
    if ((strcmp(argOp, "--list-installed") == 0)) {
        /* Reuse the platformFile buffer (since is no longer required) to compose
         * the lib directory path
         */
        struct REDAInlineListNode *archNode;
        struct stat statbuf;
        int rc;
        for (archNode = REDAInlineList_getFirst(archDef); 
                archNode != NULL; 
                archNode = REDAInlineListNode_getNext(archNode)) {
            struct Architecture *arch = (struct Architecture *)archNode;
            snprintf(platformFile, PATH_MAX, "%s/lib/%s", NDDSHOME, arch->target);
            rc = stat(platformFile, &statbuf);
            if (rc != 0) {
                if (errno == ENOENT) {
                    // target is not installed
                    continue;
                }
                fprintf(stderr, "Error stat() failed: %s (errno=%d)\n", strerror(errno), errno);
                retCode = APPLICATION_EXIT_FAILURE;
                goto done;
            }
            if (S_ISDIR(statbuf.st_mode)) {
                printf("%s\n", arch->target);
            }
            // else it must be a file, ignore it
        }
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }


    /* Find target */
    archTarget = NULL;
    struct REDAInlineListNode *node;
    for (node = REDAInlineList_getFirst(archDef); 
            node != NULL; 
            node = REDAInlineListNode_getNext(node)) {
        struct Architecture *arch = (struct Architecture *)node;
        if ((strcmp(arch->target, argTarget) == 0)) {
            archTarget = arch;
            break;
        }
    }
    if (archTarget == NULL) {
        fprintf(stderr,
                "Error: requested architecture '%s' is not supported\n",
                argTarget);
        fprintf(stderr,
                "Use --list-all or --list-installed to print all the architectures\n");
        retCode = APPLICATION_EXIT_INVALID_ARGS;
        goto done;
    }

    /* Compose the NDDS-related includes and libraries */
    libSuffix = "";
    if ((argStatic == RTI_TRUE) && (argDebug == RTI_FALSE)) {
        libSuffix = "z";
    } else if ((argStatic == RTI_FALSE) && (argDebug == RTI_TRUE)) {
        libSuffix = "d";
    } else if ((argStatic == RTI_TRUE) && (argDebug == RTI_TRUE)) {
        libSuffix = "zd";
    }
    nddsFlags = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCPP03Flags = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCLibs = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCPPLibs = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCPP03Libs = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsExtraLib = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsExtraLibCPP = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsExtraLibCPP03 = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    if (    (nddsFlags == NULL) || 
            (nddsCPP03Flags == NULL) ||
            (nddsCLibs == NULL) || 
            (nddsCPPLibs == NULL) || 
            (nddsCPP03Libs == NULL) ||
            (nddsExtraLib == NULL) ||
            (nddsExtraLibCPP == NULL) ||
            (nddsExtraLibCPP03 == NULL) ) {
        fprintf(stderr, "Out of memory allocating command-line arguments");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }


    if (argMsg == RTI_TRUE) {
        snprintf(nddsExtraLib,
                MAX_CMDLINEARG_SIZE,
                "-lrticonnextmsgc%s ",
                libSuffix);
        snprintf(nddsExtraLibCPP,
                MAX_CMDLINEARG_SIZE,
                "-lrticonnextmsgcpp%s ",
                libSuffix);
        snprintf(nddsExtraLibCPP03,
                MAX_CMDLINEARG_SIZE,
                "-lrticonnextmsgcpp2%s ",
                libSuffix);
    } else {
        nddsExtraLib[0] = '\0';
        nddsExtraLibCPP[0] = '\0';
        nddsExtraLibCPP03[0] = '\0';
    }

    if (argExpandEnvVar == RTI_TRUE) {
        /* Expand NDDSHOME */
        snprintf(nddsFlags,
                MAX_CMDLINEARG_SIZE,
                "-I%s/include -I%s/include/ndds",
                NDDSHOME,
                NDDSHOME);
        snprintf(nddsCPP03Flags,
                MAX_CMDLINEARG_SIZE,
                "-I%s/include -I%s/include/ndds -I%s/include/ndds/hpp",
                NDDSHOME,
                NDDSHOME,
                NDDSHOME);
        snprintf(nddsCLibs,
                MAX_CMDLINEARG_SIZE,
                "-L%s/lib/%s %s-lnddsc%s -lnddscore%s",
                NDDSHOME,
                argTarget,
                nddsExtraLib,
                libSuffix,
                libSuffix);
        snprintf(nddsCPPLibs,
                MAX_CMDLINEARG_SIZE,
                "-L%s/lib/%s %s-lnddscpp%s -lnddsc%s -lnddscore%s",
                NDDSHOME,
                argTarget,
                nddsExtraLibCPP,
                libSuffix,
                libSuffix,
                libSuffix);
        snprintf(nddsCPP03Libs,
                MAX_CMDLINEARG_SIZE,
                "-L%s/lib/%s %s-lnddscpp2%s -lnddsc%s -lnddscore%s",
                NDDSHOME,
                argTarget,
                nddsExtraLibCPP03,
                libSuffix,
                libSuffix,
                libSuffix);
    } else {
        /* Do not expand variables */
        if (argShell == RTI_TRUE) {
            /* Use shell style */
            snprintf(nddsFlags,
                    MAX_CMDLINEARG_SIZE,
                    "-I${NDDSHOME}/include -I${NDDSHOME}/include/ndds");
            snprintf(nddsCPP03Flags,
                    MAX_CMDLINEARG_SIZE,
                    "-I${NDDSHOME}/include -I${NDDSHOME}/include/ndds -I${NDDSHOME}/include/ndds/hpp");
            snprintf(nddsCLibs,
                    MAX_CMDLINEARG_SIZE,
                    "-L${NDDSHOME}/lib/%s %s-lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLib,
                    libSuffix,
                    libSuffix);
            snprintf(nddsCPPLibs,
                    MAX_CMDLINEARG_SIZE,
                    "-L${NDDSHOME}/lib/%s %s-lnddscpp%s -lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLibCPP,
                    libSuffix,
                    libSuffix,
                    libSuffix);
            snprintf(nddsCPP03Libs,
                    MAX_CMDLINEARG_SIZE,
                    "-L${NDDSHOME}/lib/%s %s-lnddscpp2%s -lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLibCPP03,
                    libSuffix,
                    libSuffix,
                    libSuffix);
        } else {
            /* Use makefile style */
            snprintf(nddsFlags,
                    MAX_CMDLINEARG_SIZE,
                    "-I$(NDDSHOME)/include -I$(NDDSHOME)/include/ndds");
            snprintf(nddsCPP03Flags,
                    MAX_CMDLINEARG_SIZE,
                    "-I$(NDDSHOME)/include -I$(NDDSHOME)/include/ndds, -I$(NDDSHOME)/include/ndds/hpp");
            snprintf(nddsCLibs,
                    MAX_CMDLINEARG_SIZE,
                    "-L$(NDDSHOME)/lib/%s %s-lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLib,
                    libSuffix,
                    libSuffix);
            snprintf(nddsCPPLibs,
                    MAX_CMDLINEARG_SIZE,
                    "-L$(NDDSHOME)/lib/%s %s-lnddscpp%s -lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLibCPP,
                    libSuffix,
                    libSuffix,
                    libSuffix);
            snprintf(nddsCPP03Libs,
                    MAX_CMDLINEARG_SIZE,
                    "-L$(NDDSHOME)/lib/%s %s-lnddscpp2%s -lnddsc%s -lnddscore%s",
                    argTarget,
                    nddsExtraLibCPP03,
                    libSuffix,
                    libSuffix,
                    libSuffix);
        }
    } 

    /* Process request operation */
    retCode = APPLICATION_EXIT_SUCCESS;

    /******************* C API ***************************/
    if ((strcmp(argOp, "--ccomp") == 0)) {
        if (printStringProperty(archTarget, 
                    "$C_COMPILER", 
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cflags") == 0)) {
        const char *FLAGS[] = {
            "$C_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsFlags,
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--clink") == 0)) {
        if (printStringProperty(archTarget,
                    "$C_LINKER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldflags") == 0)) {
        const char *FLAGS[] = {
            "$C_LINKER_FLAGS",
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldlibs") == 0)) {
        const char *FLAGS[] = {
            nddsCLibs,
            "$SYSLIBS",
            "$C_SYSLIBS",
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }


    /*************** Traditional C++ API *****************/
    
    } else if ((strcmp(argOp, "--cxxcomp") == 0)) {
        if (printStringProperty(archTarget,
                    "$CXX_COMPILER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxxflags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsFlags,
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxxlink") == 0)) {
        if (printStringProperty(archTarget,
                    "$CXX_LINKER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxxflags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_LINKER_FLAGS",
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxxlibs") == 0)) {
        const char *FLAGS[] = {
            nddsCPPLibs,
            "$SYSLIBS",
            "$CXX_SYSLIBS",
            NULL
        };
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }


    /***************** Modern C++ API *******************/

    } else if ((strcmp(argOp, "--cxx03comp") == 0)) {
        if (!ensureTargetSupportsCxx03(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printStringProperty(archTarget,
                    "$CXX_COMPILER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxx03flags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_COMPILER_FLAGS",
            "$CPP03_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsCPP03Flags,
            NULL
        };
        if (!ensureTargetSupportsCxx03(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxx03link") == 0)) {
        if (!ensureTargetSupportsCxx03(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printStringProperty(archTarget,
                    "$CXX_LINKER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxx03flags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_LINKER_FLAGS",
            NULL
        };
        if (!ensureTargetSupportsCxx03(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxx03libs") == 0)) {
        const char *FLAGS[] = {
            nddsCPP03Libs,
            "$SYSLIBS",
            "$CXX_SYSLIBS",
            NULL
        };
        if (!ensureTargetSupportsCxx03(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }


    /************ Ultra Modern C++ API *****************/

    } else if ((strcmp(argOp, "--cxx11comp") == 0)) {
        if (!ensureTargetSupportsCxx11(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printStringProperty(archTarget,
                    "$CXX_COMPILER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxx11flags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_COMPILER_FLAGS",
            "$CPP11_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsCPP03Flags,     // Use same as CPP03 flags
            NULL
        };
        if (!ensureTargetSupportsCxx11(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--cxx11link") == 0)) {
        if (!ensureTargetSupportsCxx11(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printStringProperty(archTarget,
                    "$CXX_LINKER",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxx11flags") == 0)) {
        const char *FLAGS[] = {
            "$CXX_LINKER_FLAGS",
            NULL
        };
        if (!ensureTargetSupportsCxx11(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--ldxx11libs") == 0)) {
        const char *FLAGS[] = {
            nddsCPP03Libs,
            "$SYSLIBS",
            "$CXX_SYSLIBS",
            NULL
        };
        if (!ensureTargetSupportsCxx11(archTarget)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }
        if (printCompositeFlagsProperties(archTarget,
                    FLAGS,
                    argExpandEnvVar,
                    argShell) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }


    /****************** Miscellaneous *******************/

    } else if ((strcmp(argOp, "--os") == 0)) {
        if (printStringProperty(archTarget,
                    "$OS",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if ((strcmp(argOp, "--platform") == 0)) {
        if (printStringProperty(archTarget,
                    "$PLATFORM",
                    argExpandEnvVar) == RTI_FALSE) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else {
        fprintf(stderr, "Error: invalid operation: %s\n", argOp);
        retCode = APPLICATION_EXIT_INVALID_ARGS;
    }

done:
    if (NDDSHOME != NULL) {
        free(NDDSHOME);
    }
    if (platformFile != NULL) {
        free(platformFile);
    }
    if (archDef != NULL) {
        struct REDAInlineListNode *next;
        struct REDAInlineListNode *node = REDAInlineList_getFirst(archDef);
        while (node != NULL) {
            next = REDAInlineListNode_getNext(node);
            Architecture_delete((struct Architecture *)node);
            node = next;
        }
        free(archDef);
    }
    if (nddsFlags != NULL) {
        free(nddsFlags);
    }
    if (nddsCLibs != NULL) {
        free(nddsCLibs);
    }
    if (nddsCPPLibs != NULL) {
        free(nddsCPPLibs);
    }
    if (nddsCPP03Libs != NULL) {
        free(nddsCPP03Libs);
    }
    return retCode;
}
/* }}} */

