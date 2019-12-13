/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/

#ifndef __HELLO_H_INCLUDED__
#define __HELLO_H_INCLUDED__


/*****************************************************************************/
/* Unify some common functions and types between Unix and Windows            */
/*****************************************************************************/
#ifdef RTI_WIN32
#define RTI_STRCASECMP      _stricmp
#define RTI_STRNCASECMP     _strnicmp
#define RTI_STRTOULL        _strtoui64
#define RTI_STRTOUL         strtoul
#define RTI_STRNCMP         strncmp
#define RTI_SSCANF          sscanf_s
#else
#define RTI_STRCASECMP      strcasecmp
#define RTI_STRNCASECMP     strncasecmp
#define RTI_STRTOULL        strtoull
#define RTI_STRTOUL         strtoul
#define RTI_STRNCMP         strncmp
#define RTI_SSCANF          sscanf
#endif


#endif            /* !defined(__HELLO_H_INCLUDED__) */
