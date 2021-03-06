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
#include "ndds/ndds_cpp.h"


/* Returns false in case of error */
extern bool startSubscriber(DDSDomainParticipant *participant,
                       DDSTopic *topic,
                       DDS_Long verbose,
                       DDS_Long sampleCount);


#endif            /* !defined(__HELLO_SUBSCRIBER_H_INCLUDED__) */


