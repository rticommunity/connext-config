/*****************************************************************************
 * Copyright 2020 Real-Time Innovations, Inc.                                *
 * Author: fabriziobertocci@gmail.com                                        *
 * Subject to Eclipse Public License v1.0; see LICENSE.md for details.       *
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
 * TODO / ISSUES:
 *  - if --noexpand and --shell is used, the env variables should all have 
 *    the form ${FOO}, but if the variable comes from the platform file,
 *    it retains the form $(FOO), getting mixed output.
 *    For example: 
 *      ./src/connext-config --noexpand --shell --cflags armv7aAndroid2.3gcc4.8
 *    produces the following output:
 *      -Wall -I$(ANDROID_NDK_PATH)/platforms/... -I${NDDSHOME}/include/ndds
 *               ^^^^^^^^^^^^^^^^^^
 *               Here we need to replace to ${...}
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <libgen.h>     // For dirname()

#include <unistd.h>
#include <limits.h>
#include <assert.h>

#include <linkedlist.h>

#define APPLICATION_NAME                        "connext-config"
#define APPLICATION_VERSION                     "1.0.0"

#define APPLICATION_EXIT_SUCCESS                0
#define APPLICATION_EXIT_INVALID_ARGS           1
#define APPLICATION_EXIT_NO_NDDSHOME            2
#define APPLICATION_EXIT_FAILURE                3
#define APPLICATION_EXIT_UNKNOWN                4

/* NULL-terminated array of valid <what> commands */
const char * VALID_WHAT[] = {
    "--ccomp", "--clink", "--cxxcomp", "--cxxlink", "--cflags", "--cxxflags",
    "--ldflags", "--ldxxflags", "--ldlibs", "--ldxxlibs",
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
#define MAX_ARRAY_SIZE      20

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
    struct SimpleListNode   parent;
    char                    key[MAX_STRING_SIZE];
    union {
        /* 
         * Value is represented as a single string 
         * (APVT_String or APVT_EnvVariable) 
         */
        char as_string[MAX_STRING_SIZE];

        /* Value is represented as an array of strings */
        char as_arrayOfStrings[MAX_ARRAY_SIZE][MAX_STRING_SIZE];

        /* Value is a boolean */
        int as_bool;
    } value;
    ArchParamValueType valueType;
};

/* {{{ ArchParameter_init
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The initializer for the ArchParameter object
 */
void ArchParameter_init(struct ArchParameter *me) {
    SimpleListNode_init(&me->parent);
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
/* {{{ ArchParameter_finalize
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The finalizer for the ArchParameter objec
 */
void ArchParameter_finalize(struct ArchParameter *me) {
    SimpleListNode_finalize(&me->parent);
}

/* }}} */
/* {{{ ArchParameter_delete
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The destructor for the ArchParameter object
 */
void ArchParameter_delete(struct ArchParameter *me) {
    ArchParameter_finalize(me);
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
    struct SimpleListNode   parent;
    char                    target[MAX_STRING_SIZE];
    struct SimpleList       paramList;
};

/* {{{ Architecture_init
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Architecture initializer
 */
void Architecture_init(struct Architecture *me) {
    SimpleListNode_init(&me->parent);
    memset(me->target, 0, sizeof(me->target));
    SimpleList_init(&me->paramList);

    /* Set the node destructor of the list of parameter so all the nodes
     * will be properly released when this list is finalized
     */
    SimpleList_setNodeDestructor(&me->paramList, (SimpleListNodeDestructorFn)ArchParameter_delete);
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
    SimpleListNode_finalize(&me->parent);
    SimpleList_finalize(&me->paramList);
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
static void dumpArch(struct SimpleList *list) {
    struct SimpleListNode *archNode = NULL;

    for (archNode = SimpleList_getBegin(list); 
            archNode; 
            archNode = SimpleListNode_getNext(archNode)) {
        struct SimpleListNode *paramNode;
        struct Architecture *arch = (struct Architecture *)archNode;
        printf("arch='%s':\n", arch->target);

        for (paramNode = SimpleList_getBegin(&arch->paramList); 
                paramNode; 
                paramNode = SimpleListNode_getNext(paramNode)) {
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
            }
        }
    }
}

/* }}} */
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
    struct SimpleListNode *paramNode;
    for (paramNode = SimpleList_getBegin(&arch->paramList);
            paramNode;
            paramNode = SimpleListNode_getNext(paramNode)) {
        struct ArchParameter *param = (struct ArchParameter *)paramNode;
        if (!strcmp(param->key, key)) {
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
    while (arr[i]) {
        if (!strcmp(arr[i], val)) {
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
    static char retVal[MAX_CMDLINEARG_SIZE];
    char varName[MAX_STRING_SIZE];
    size_t wr = 0;     // write pos
    size_t rd;         // read pos

    memset(retVal, 0, sizeof(retVal));

    for(rd = 0; inStr[rd] != '\0'; ++rd) {
        if ((inStr[rd] == '$') && inStr[rd+1]=='(') {
            /* Found the beginning of an nev variable, extract the name */
            size_t i = 0;
            char *varValue = NULL;
            size_t varValueLen;

            memset(varName, 0, sizeof(varName));
            rd += 2;        /* Skip '$(' */
            while(inStr[rd] != ')') {    /* Read var name until ')' */
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
 * \return          1 if success, 0 if failed
 */
int printStringProperty(struct Architecture *arch,
        const char *propName,
        int expandVar) {
    char *toPrint;
    struct ArchParameter *ap = archGetParam(arch, propName);
    if (ap == NULL) {
        /* Variable not defined */
        return 1;
    }
    if (ap->valueType == APVT_String) {
        toPrint = &ap->value.as_string[0];
        if (expandVar) {
            toPrint = expandEnvVar(toPrint);
            if (toPrint == NULL) {
                // Error message has been already printed in expandEnvVar
                return 0;
            }
        }

    } else if (ap->valueType == APVT_EnvVariable) {
        toPrint = &ap->value.as_string[0];

    } else {
        fprintf(stderr, 
                "Property '%s' is not a string or env variable for target %s\n", 
                propName, 
                arch->target);
        return 0;
    }
    puts(toPrint);
    return 1;
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

    while(ap->value.as_arrayOfStrings[i][0]) {
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
 * props is a NULL terminated array of strings identifying either the name 
 *      of the property to look up (if string starts with '$'), or a verbatim
 *      string that need to be appended.
 *      If the name starts with a '$', then the property value is looked up
 *      from the architecture.
 *      Each property value is expected to be an array of strings identifying
 *      the flags. Each flag is prefixed with a "-" (or "-I" for $INCLUDES)
 * expandVar is a boolean that tells whether to expand environment variables (1)
 *      or not (0).
 * Returns 0 if an error occurred, or 1 if success
 */
int printCompositeFlagsProperties(struct Architecture *arch, 
        const char **props, 
        int expandVar) {
    char line[MAX_CMDLINEARG_SIZE];
    int wr = 0;
    int rc = 0;
    int propIdx;
    char *toPrint;
    const char *prefix;

    memset(line, 0, sizeof(line));

    for (propIdx = 0; props[propIdx]; ++propIdx) {
        /* Just a convenience alias */
        const char *propName = &props[propIdx][0];

        /* Property or verbatim string? */
        if (propName[0] == '$') {
            /* In the template files, the "$INCLUDES" values need to be prefixed 
             * by '-I', all the other flags need to be prefixed by a "-"
             */
            prefix = (!strcmp(propName, "$INCLUDES")) ? "-I" : "-";
            rc = joinStringArrayProperties(arch, 
                    &line[wr], 
                    MAX_CMDLINEARG_SIZE-wr, 
                    propName, 
                    prefix);
            if (rc == -1) {
                return 0;
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
                return 0;
            }
        }
        wr += rc;
    }
    
    toPrint = &line[0];
    if (expandVar) {
        toPrint = expandEnvVar(toPrint);
        if (toPrint == NULL) {
            return 0;
        }
    }
    // Now the toPrint string should always have a space at the end, remove it
    // (this is needed so we can use the dump-all.sh script to compare the results
    // with the javascript version)
    wr = strlen(toPrint)-1;
    while(isspace(toPrint[wr])) {
        toPrint[wr--] = '\0';
    }
    puts(unescapeString(toPrint));
    return 1;
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
    while (*tmp1) {
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
    if (!tmp1) {
        tmp1 = strchr(line, '\'');
        if (!tmp1) {
            *errOut = "Failed to find the beginning of string delimiter";
            return NULL;
        }
    }
    ++tmp1;

    tmp2 = strchr(tmp1, endl);
    if (!tmp2) {
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

// {{{ processArchDefinitionLine
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns 0 if failed, 1 if success
// Line has the following form:
//      #arch("i86Sol2.9","gcc3.3.2", {
// or (because of a bug in the platform file):
//
//      #arch("armv7Linux3.0","gcc4.6.1.cortex-a9" {
// Identify the two strings and copy them inside 'target', and 'compiler' 
// (each one of max length MAX_STRING_SIZE)
int processArchDefinitionLine(const char *line, char *target, char *compiler) {
    char *tmp1;
    char *tmp2;

    memset(target, 0, MAX_STRING_SIZE);
    memset(compiler, 0, MAX_STRING_SIZE);
    tmp1 = strchr(line, '"');
    if (!tmp1) {
        fprintf(stderr, "Cannot find start target in arch definition: '%s'\n", 
                line);
        return 0;
    }
    ++tmp1;

    tmp2 = strchr(tmp1, '"');
    if (!tmp2) {
        fprintf(stderr, "Cannot find end target in arch definition: '%s'\n", 
                line);
        return 0;
    }
    if (tmp2 <= tmp1) {
        fprintf(stderr, "Unable to identify target arch in line: '%s'\n", 
                line);
        return 0;
    }

    if (tmp2-tmp1 > MAX_STRING_SIZE) {
        fprintf(stderr, "Target arch name '%s' too large: (%ld, max=%d)\n", 
                tmp1, (tmp2-tmp1), MAX_STRING_SIZE);
        return 0;
    }
    memcpy(&target[0], tmp1, (tmp2-tmp1));

    // Now parse and copy the compiler arch
    tmp1 = strchr(tmp2+1, '"');
    if (!tmp1) {
        fprintf(stderr, "Cannot find start compiler in arch definition: '%s'\n", 
                line);
        return 0;
    }
    ++tmp1;

    tmp2 = strchr(tmp1, '"');
    if (!tmp2) {
        fprintf(stderr, "Cannot find end compiler in arch definition: '%s'\n", 
                line);
        return 0;
    }

    // Some targets have an empty compiler name
    if (tmp2 > tmp1) {
        if (tmp2-tmp1 > MAX_STRING_SIZE) {
            fprintf(stderr, "Compiler name '%s' too large: (%ld, max=%d) for target='%s'\n", 
                    tmp1, (tmp2-tmp1), MAX_STRING_SIZE, target);
            return 0;
        }
        memcpy(&compiler[0], tmp1, (tmp2-tmp1));
    }
    return 1;
}

// }}}
// {{{ processKeyValuePairLine
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns 0 if failed, 1 if success
//
// Line has the following form:
// "      KEY: <value>
// Notes:
// - The number of heading space is not fixed
// - Key _usually_ starts with '$', but there are some variables: "TR_VAR": "..."
int processKeyValuePairLine(char *line, struct ArchParameter *param) {
    char *key;
    char *val;
    char *tmp;

    memset(param, 0, sizeof(*param));
    tmp = strchr(line, ':');   // find the ':' delimiter
    if (!tmp) {
        fprintf(stderr, "Cannot find key-value pair delimiter ':' in line: '%s'\n", 
                line);
        return 0;
    }
    val = tmp+1;

    // Trim the end of the key
    do {
        *tmp = '\0'; --tmp;
    } while (isspace(*tmp) || (*tmp == '"'));

    // Trim the beginning of the key
    key = line;
    while(isspace(*key) || (*key=='"')) {
        ++key;
    }

    if (strlen(key) > sizeof(param->key)) {
        fprintf(stderr, "Key too long: '%s' (>%ld)\n",
                key, sizeof(param->key));
        return 0;
    }

    strcpy(param->key, key);

    // Trim the beginning of the value
    while(isspace(*val)) ++val;

    // Identify the kind of value
    if (!strncmp(val, "true", 4)) {
        param->valueType = APVT_Boolean;
        param->value.as_bool = 1;
        return 1;
    }
    if (!strncmp(val, "false", 5)) {
        param->valueType = APVT_Boolean;
        param->value.as_bool = 0;
        return 1;
    }
    if ((*val == '"') || (*val == '\'')) {
        const char *errMsg;
        param->valueType = APVT_String;
        val = parseStringInQuotes(val, 
                &param->value.as_string[0], 
                sizeof(param->value.as_string), 
                &errMsg);
        if (!val) {
            fprintf(stderr, "Error: %s while parsing key-value pair line: '%s'\n",
                    errMsg, line);
            return 0;
        }
        return 1;
    }

    if (*val == '[') {
        // Value is an array
        size_t idx = 0;
        const char *errMsg;
        param->valueType = APVT_ArrayOfStrings;
        while ((strchr(val, '"') != NULL) || (strchr(val, '\'') != NULL)) {
            val = parseStringInQuotes(val, 
                    &param->value.as_arrayOfStrings[idx][0], 
                    MAX_STRING_SIZE,
                    &errMsg);
            if (val == NULL) {
                fprintf(stderr, "Error: %s while parsing key-value pair line: '%s'\n",
                        errMsg, line);
                return 0;
            }
            ++val;
            ++idx;
        }

        return 1;
    }
    if (*val == '$') {
        // Value is an env variable
        param->valueType = APVT_EnvVariable;
        tmp = strchr(val, ',');
        if (!tmp) {
            fprintf(stderr, "Unable to find end of env variable delimiter in line '%s'\n",
                    line);
            return 0;
        }
        *tmp = '\0';
        /* When taking the env variable, skip the variable name.
         * For example, in the following:
         *      $CPU : $CPU.ppcbe
         * the value.as_string will get the value "ppcbe"
         */
        tmp = strchr(val, '.');
        if (!tmp) {
            fprintf(stderr, "Unable to find '.' delimiter of env variable in line '%s'\n",
                    line);
            return 0;
        }

        strcpy(param->value.as_string, tmp+1);

        return 1;
    }


    return 1;
}

// }}}
// {{{ readPlatformFile
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns 0 if failed, 1 if success
int readPlatformFile(const char *filePath, struct SimpleList *archDef) {
    int ok = 0;
    FILE *fp;
    char *line = NULL;
    size_t lineLen = 0;
    ssize_t nRead;
    ReadStateMachine rsm = RSM_TOPLEVEL;
    struct Architecture *currentArch = NULL;
    struct ArchParameter *currentParam = NULL;
    char *trimLine;
    char target[MAX_STRING_SIZE];
    char compiler[MAX_STRING_SIZE];
    int longComment = 0;
    char *tmp;
    int lineCount = 0;

    fp = fopen(filePath, "r");
    if (!fp) {
        fprintf(stderr, "File not found: %s\n", filePath);
        goto done;
    }

    while (getline(&line, &lineLen, fp) != -1) {
        ++lineCount;

        // Identify multi-line comments
        if ((tmp = strstr(line, "#*")) != NULL) {
            *tmp = '\0';
            longComment = 1;
        }

        trimLine = line;
        if (longComment && ((tmp = strstr(trimLine, "*#")) != NULL)) {
            longComment = 0;
            trimLine = tmp+2;
        }

        // Trim read line and remove the newline at the end
        nRead = strlen(trimLine);
        while (isspace(trimLine[nRead])) {
            trimLine[nRead--] = '\0';
        }
        while (isspace(*trimLine)) ++trimLine;

        // Skip empty lines
        if (trimLine[0] == '\0') {
            continue;
        }


        // TOP LEVEL - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        if (rsm == RSM_TOPLEVEL) {
                
            // Identify directives (#macro, #arch) first

            // Detect macro section
            // Example:
            //      #macro (arch $name $compiler $params)
            if (!strncmp(trimLine, "#macro", 6)) {
                rsm = RSM_MACRO;
                continue;
            }

            // Detect arch section
            // Example:
            //      #arch("sparc64Sol2.10","gcc3.4.2", {
            if (!strncmp(trimLine, "#arch(", 6)) {
                assert(currentArch == NULL);

                currentArch = Architecture_new();
                if (currentArch == NULL) {
                    fprintf(stderr, "Out of memory allocating currentArch\n");
                    goto done;
                }
                if (processArchDefinitionLine(trimLine, &target[0], &compiler[0]) == FL_FALSE) {
                    goto done;
                }
                if (strlen(target) + strlen(compiler) > MAX_STRING_SIZE) {
                    fprintf(stderr, "Composite target string too long for platform: %s\n", trimLine);
                    goto done;
                }
                snprintf(currentArch->target, MAX_STRING_SIZE, "%s%s", target, compiler);
                rsm = RSM_ARCH;
                continue;
            }

            // Skip comments
            // From now on, comments are lines that starts with '#'. For example:
            //      ## (c) Copyright, Real-Time Innovations, Inc. 2001.  All rights reserved.
            //      #******************** LINUX ********************#
            //      #    $C_LINKER_FLAGS : ["std=libc++"],
            if (trimLine[0] == '#') {
                continue;
            }

            // Unexpected
            fprintf(stderr, "ERROR: unknown how to parse line: '%s'\n", trimLine);
            goto done;
        }

        // MACRO - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        if (rsm == RSM_MACRO) {
            // In macro state, we ignore everything until the #end symbol
            if (!strncmp(trimLine, "#end", 4)) {
                rsm = RSM_TOPLEVEL;
            }
            continue;
        }

        // ARCH  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        if (rsm == RSM_ARCH) {
            // Exit ARCH after we detect the '})\n' pattern
            if (!strncmp(trimLine, "})\n", 3)) {

                // Filter out the architectures that need to be skipped:
                //  - $HIDDEN=true
                //  - All windows targets
                //  - All the iOS targets
                // In general remove all the architectures that don't define 
                // the macros for the toolset (C_COMPILER, C_LINKER, ...)
                int skipArch = 0;
                struct ArchParameter * hidden = archGetParam(currentArch, "$HIDDEN");
                if (hidden && ((hidden->valueType == APVT_Boolean) && hidden->value.as_bool)) {
                    skipArch = 1;
                }
#if 1
                if (!archGetParam(currentArch, "$C_COMPILER") ||
                        !archGetParam(currentArch, "$C_LINKER") ||
                        !archGetParam(currentArch, "$CXX_COMPILER") ||
                        !archGetParam(currentArch, "$CXX_LINKER")) {
                    skipArch = 1;
                }
#else
                if ((strstr(currentArch->target, "Win") != NULL) || 
                    (strstr(currentArch->target, "INtime6") != NULL) ||
                    (strstr(currentArch->target, "iOS") != NULL)) {
                    skipArch = 1;
                }
#endif

                if (skipArch) {
                    // Drop skipped architectures
                    Architecture_delete(currentArch);
                } else {
                    // Valid, push the arch to the archDef
                    SimpleList_addToEnd(archDef, &currentArch->parent);
                }

                currentArch = NULL;
                rsm = RSM_TOPLEVEL;
                continue;
            }

            // process the arch line
            assert(currentParam == NULL);
            currentParam = ArchParameter_new();
            if (!currentParam) {
                fprintf(stderr, "Out of memory allocating currentParam\n");
                goto done;
            }
            if (!processKeyValuePairLine(trimLine, currentParam)) {
                goto done;
            }
            // Got a valid line:
            SimpleList_addToEnd(&currentArch->paramList, &currentParam->parent);
            currentParam = NULL;
            continue;
        }
    
        fprintf(stderr, "Error: unexpected state: %d\n", rsm);
        goto done;
    }
    ok = 1;

done:
    if (fp) {
        fclose(fp);
    }
    if (line) {
        free(line);
    }
    if (currentArch) {
        Architecture_delete(currentArch);
    }
    if (currentParam) {
        ArchParameter_delete(currentParam);
    }
    if (!ok) {
        fprintf(stderr, "Error occurred while parsing line %d\n", lineCount);
    }
    return ok;
}
// }}}

// {{{ usage
// -----------------------------------------------------------------------------
void usage() {
    printf("RTI Connext DDS Config version %s\n", APPLICATION_VERSION); 
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("Usage:\n");
    printf("    %s -h|--help    Show this help\n", APPLICATION_NAME);
    printf("    %s -V|--version Prints version number\n", APPLICATION_NAME);
    printf("    %s --list-all   List all platform architectures supported\n", APPLICATION_NAME);
    printf("    %s --dump-all   Dump all platforms and all settings (testing only)\n", APPLICATION_NAME);
    printf("    %s [modifiers] <what> <targetArch>\n", APPLICATION_NAME);
    printf("\n");
    printf("Where [modifiers] are:\n");
    printf("    --static    use static linking against RTI Connext DDS\n");
    printf("    --debug     use debug version of the RTI Connext DDS libraries\n");
    printf("    --sh        use shell-like variable expansion (vs. make-like variables)\n");
    printf("    --noexpand  do not expand environment variables in output\n");
    printf("\n");
    printf("Required argument <what> is one of:\n");
    printf("    --ccomp     output the C compiler to use\n");
    printf("    --clink     output the C linker to use\n");
    printf("    --cxxcomp   output the C++ compiler to use\n");
    printf("    --cxxlink   output the C++ linker to use\n");
    printf("    --cflags    output all pre-processor and compiler flags for C compiler\n");
    printf("    --cxxflags  output all pre-processor and compiler flags for C++ compiler\n");
    printf("    --ldflags   output the linker flags to use when building C programs\n");
    printf("    --ldxxflags output the linker flags to use when building C++ programs\n");
    printf("    --ldlibs    output the required libraries for C programs\n");
    printf("    --ldxxlibs  output the required libraries for C++ programs\n");
    printf("    --os        output the OS (i.e. UNIX, ANDROID, IOS, ...)\n");
    printf("    --platform  output the Platform (i.e. i86, x64, armv7a, ...)\n");
    printf("\n");
    printf("Required argument <targetArch> is one of the supported target architectures.\n");
    printf("Use `--list-all` to output a list of supported architectures\n");
}

// }}}
// {{{ main
// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    const char *argOp = NULL;
    const char *argTarget = NULL;
    int argStatic = 0;
    int argDebug = 0;
    int argShell = 0;
    int argExpandEnvVar = 1;
    char *tmp;
    char *NDDSHOME = NULL;
    char *platformFile = NULL;
    int retCode = APPLICATION_EXIT_UNKNOWN;
    struct SimpleList   archDef;
    struct Architecture *archTarget;
    char *nddsFlags = NULL;
    char *nddsCLibs = NULL;
    char *nddsCPPLibs = NULL;
    const char *libSuffix;

    SimpleList_init(&archDef);
    SimpleList_setNodeDestructor(&archDef, (SimpleListNodeDestructorFn)Architecture_delete);

    if (argc <= 1) {
        usage();
        retCode = APPLICATION_EXIT_INVALID_ARGS;
        goto done;
    }
    if ((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        usage();
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
    if ((argc == 2) && (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))) {
        printf("%s v.%s\n", APPLICATION_NAME, APPLICATION_VERSION);
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }
    if ((argc == 2) && (
                !strcmp(argv[1], "--list-all") || 
                !strcmp(argv[1], "--dump-all"))) {
        argOp = argv[1];

    } else {
        int i;

        // Parse command line with at least 2 arguments
        if (argc < 3) {
            usage();
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }

        for (i = 1; i < argc-1; ++i) {
            if (!strcmp(argv[i], "--static")) {
                argStatic = 1;
                continue;
            }
            if (!strcmp(argv[i], "--debug")) {
                argDebug = 1;
                continue;
            }
            if (!strcmp(argv[i], "--shell") || !strcmp(argv[i], "--sh")) {
                argShell = 1;
                continue;
            }
            if (!strcmp(argv[i], "--noexpand")) {
                argExpandEnvVar = 0;
                continue;
            }
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                retCode = APPLICATION_EXIT_SUCCESS;
                goto done;
            }
            if (arrayFind(&VALID_WHAT[0], argv[i]) != -1) {
                argOp = argv[i];
                continue;
            }
            usage();
            fprintf(stderr, "Error: invalid argument: %s\n", argv[i]);
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }
        argTarget = argv[argc-1];
        // Make sure the last argument is as intended the target
        if (!strcmp(argTarget, "-h") || !strcmp(argTarget, "--help")) {
            usage();
            retCode = APPLICATION_EXIT_SUCCESS;
            goto done;
        }
        if (!strncmp(argTarget, "--", 2)) {
            usage();
            fprintf(stderr, "Error: missing target architecture\n");
            fprintf(stderr, "Use --list-all to print all the supported architectures\n");
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }
    }

    // Check if $NDDSHOME is defined
    NDDSHOME = calloc(PATH_MAX+1, 1);
    if (!NDDSHOME) {
        fprintf(stderr, "Out of memory allocating NDDSHOME path\n");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }
    tmp = getenv("NDDSHOME");
    if (tmp) {
        strncpy(NDDSHOME, tmp, PATH_MAX);
    } else {
        // Obtain NDDSHOME from argv[0]
        if (argv[0][0] == '/') {
            // argv[0] is absolute path
            strncpy(NDDSHOME, argv[0], PATH_MAX);
        } else {
            char *appPath;
            // Build the absolute path to this scripb from the current working directory
            snprintf(NDDSHOME, PATH_MAX, "%s/%s", getenv("PWD"), argv[0]);
            /*
            if (!getcwd(cwd, PATH_MAX)) {
                printf("Error: failed to read current working directory while attempting to locate the platform.vm file\n");
                printf("Try re-running with NDDSHOME environment variable defined\n");
                retCode = APPLICATION_EXIT_FAILURE;
                goto done;
            }
            strncat(cwd, "/", PATH_MAX-strlen(cwd));
            strncat(cwd, argv[0], PATH_MAX-strlen(cwd));
            */
            appPath = realpath(NDDSHOME, NULL);
            if (!appPath) {
                fprintf(stderr, "Unable to compute NDDSHOME: %s (errno=%d)\n", strerror(errno), errno);
                retCode = APPLICATION_EXIT_NO_NDDSHOME;
                goto done;
            }
            strncpy(NDDSHOME, appPath, PATH_MAX);
            free(appPath);
        }
        // Go two level up
        tmp = dirname(NDDSHOME);
        strncpy(NDDSHOME, tmp, PATH_MAX);
        tmp = dirname(NDDSHOME);
        strncpy(NDDSHOME, tmp, PATH_MAX);
    }

    // Complete building the path to the platform.vm file:
    platformFile = calloc(PATH_MAX+1, 1);
    if (!platformFile) {
        printf("Out of memory allocating platformFile path\n");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }
    snprintf(platformFile, PATH_MAX, "%s/resource/app/app_support/rtiddsgen/templates/projectfiles/platforms.vm", NDDSHOME);

    // Read and parse platform file
    readPlatformFile(platformFile, &archDef);
    if (!strcmp(argOp, "--dump-all")) {
        dumpArch(&archDef);
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }

    if (!strcmp(argOp, "--list-all")) {
        struct SimpleListNode *archNode;
        for (archNode = SimpleList_getBegin(&archDef); archNode; archNode = SimpleListNode_getNext(archNode)) {
            struct Architecture *arch = (struct Architecture *)archNode;
            printf("%s\n", arch->target);
        }
        retCode = APPLICATION_EXIT_SUCCESS;
        goto done;
    }

    // Find target
    {
        archTarget = NULL;
        struct SimpleListNode *node;
        for (node = SimpleList_getBegin(&archDef); node; node = SimpleListNode_getNext(node)) {
            struct Architecture *arch = (struct Architecture *)node;
            if (!strcmp(arch->target, argTarget)) {
                archTarget = arch;
                break;
            }
        }
        if (!archTarget) {
            usage();
            fprintf(stderr, "Error: requested architecture '%s' is not supported\n", argTarget);
            fprintf(stderr, "Use --list-all to print all the supported architectures\n");
            retCode = APPLICATION_EXIT_INVALID_ARGS;
            goto done;
        }
    }

    // Compose the NDDS-related includes and libraries
    libSuffix = "";
    if (argStatic && !argDebug) {
        libSuffix = "z";
    } else if (!argStatic && argDebug) {
        libSuffix = "d";
    } else if (argStatic && argDebug) {
        libSuffix = "zd";
    }
    nddsFlags = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCLibs = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    nddsCPPLibs = calloc(MAX_CMDLINEARG_SIZE+1, 1);
    if (!nddsFlags || !nddsCLibs || !nddsCPPLibs) {
        fprintf(stderr, "Out of memory allocating command-line arguments");
        retCode = APPLICATION_EXIT_FAILURE;
        goto done;
    }

    if (argExpandEnvVar) {
        /* Expand NDDSHOME */
        snprintf(nddsFlags, MAX_CMDLINEARG_SIZE, "-I%s/include -I%s/include/ndds", NDDSHOME, NDDSHOME);
        snprintf(nddsCLibs, MAX_CMDLINEARG_SIZE, "-L%s/lib/%s -lnddsc%s -lnddscore%s", NDDSHOME, argTarget, libSuffix, libSuffix);
        snprintf(nddsCPPLibs, MAX_CMDLINEARG_SIZE, "-L%s/lib/%s -lnddscpp%s -lnddsc%s -lnddscore%s", NDDSHOME, argTarget, libSuffix, libSuffix, libSuffix);
    } else {
        /* Do not expand variables */
        if (argShell) {
            // Use shell style
            snprintf(nddsFlags, MAX_CMDLINEARG_SIZE, "-I${NDDSHOME}/include -I${NDDSHOME}/include/ndds");;
            snprintf(nddsCLibs, MAX_CMDLINEARG_SIZE, "-L${NDDSHOME}/lib/%s -lnddsc%s -lnddscore%s", argTarget, libSuffix, libSuffix);
            snprintf(nddsCPPLibs, MAX_CMDLINEARG_SIZE, "-L${NDDSHOME}/lib/%s -lnddscpp%s -lnddsc%s -lnddscore%s", argTarget, libSuffix, libSuffix, libSuffix);
        } else {
            // Use makefile style
            snprintf(nddsFlags, MAX_CMDLINEARG_SIZE, "-I$(NDDSHOME)/include -I$(NDDSHOME)/include/ndds");
            snprintf(nddsCLibs, MAX_CMDLINEARG_SIZE, "-L$(NDDSHOME)/lib/%s -lnddsc%s -lnddscore%s", argTarget, libSuffix, libSuffix);
            snprintf(nddsCPPLibs, MAX_CMDLINEARG_SIZE, "-L$(NDDSHOME)/lib/%s -lnddscpp%s -lnddsc%s -lnddscore%s", argTarget, libSuffix, libSuffix, libSuffix);
        }
    } 

    // Process request operation
    retCode = APPLICATION_EXIT_SUCCESS;
    if (!strcmp(argOp, "--ccomp")) {
        if (!printStringProperty(archTarget, "$C_COMPILER", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--clink")) {
        if (!printStringProperty(archTarget, "$C_LINKER", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--cxxcomp")) {
        if (!printStringProperty(archTarget, "$CXX_COMPILER", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--cxxlink")) {
        if (!printStringProperty(archTarget, "$CXX_LINKER", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--cflags")) {
        const char *FLAGS[] = {
            "$C_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsFlags,
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--cxxflags")) {
        const char *FLAGS[] = {
            "$CXX_COMPILER_FLAGS",
            "$DEFINES",
            "$INCLUDES",
            nddsFlags,
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--ldflags")) {
        const char *FLAGS[] = {
            "$C_LINKER_FLAGS",
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--ldxxflags")) {
        const char *FLAGS[] = {
            "$CXX_LINKER_FLAGS",
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--ldlibs")) {
        const char *FLAGS[] = {
            nddsCLibs,
            "$SYSLIBS",
            "$C_SYSLIBS",
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--ldxxlibs")) {
        const char *FLAGS[] = {
            nddsCPPLibs,
            "$SYSLIBS",
            "$CXX_SYSLIBS",
            NULL
        };
        if (!printCompositeFlagsProperties(archTarget, FLAGS, argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--os")) {
        if (!printStringProperty(archTarget, "$OS", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else if (!strcmp(argOp, "--platform")) {
        if (!printStringProperty(archTarget, "$PLATFORM", argExpandEnvVar)) {
            retCode = APPLICATION_EXIT_FAILURE;
            goto done;
        }

    } else {
        fprintf(stderr, "Error: invalid operation: %s\n", argOp);
        retCode = APPLICATION_EXIT_INVALID_ARGS;
    }


done:
    if (NDDSHOME) {
        free(NDDSHOME);
    }
    if (platformFile) {
        free(platformFile);
    }
    SimpleList_finalize(&archDef);
    if (nddsFlags) {
        free(nddsFlags);
    }
    if (nddsCLibs) {
        free(nddsCLibs);
    }
    if (nddsCPPLibs) {
        free(nddsCPPLibs);
    }
    return retCode;
}
// }}}


