/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/

#ifndef __HELLO_SUBSCRIBER_H_INCLUDED__
#define __HELLO_SUBSCRIBER_H_INCLUDED__


/* Requires <ndds/ndds_c.h> for the data types */
#include "ndds/ndds_c.h"


/* Returns DDS_BOOLEAN_FALSE in case of error */
extern DDS_Boolean start_subscriber(DDS_DomainParticipant *participant,
                       DDS_Topic *topic,
                       DDS_Boolean verbose,
                       DDS_Long sample_count);


#endif      /* !defined(__HELLO_SUBSCRIBER_H_INCLUDED__) */


