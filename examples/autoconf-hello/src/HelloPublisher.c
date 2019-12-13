/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/


#include "Hello.h"
#include "HelloPublisher.h"

/* Defines the maximum number of consecutive write errors. */
#define MAX_CONSECUTIVE_WRITE_ERROR         10


int RTI_SNPRINTF (
   char *buffer,
   size_t count,
   const char *format, ...)
{
    int length;
    va_list ap;
    va_start(ap, format);
#ifdef RTI_WIN32
    length = _vsnprintf_s(buffer, count, count, format, ap);
#else
    length = vsnprintf(buffer, count, format, ap);
#endif
     va_end(ap);
     return length;
}

/*****************************************************************************/
/* start_publisher                                                           */
/*                                                                           */
/* Creates the DDS publisher and data writers, then start sending the data.  */
/*                                                                           */
/* Input:                                                                    */
/*   participant: the DDS domain participant to use                          */
/*   topic: the DDS topic to use for publishing                              */
/*   dataSise: size of the payload buffer to send                            */
/*                                                                           */
/* Returns:                                                                  */
/*   The boolean value DDS_BOOLEAN_TRUE if an error occurred.                */
/*                                                                           */
/*****************************************************************************/
DDS_Boolean start_publisher(
                        DDS_DomainParticipant *participant,
                        DDS_Topic *topic,
                        DDS_Boolean verbose,
                        DDS_Long data_size,
                        DDS_Long sampleCount) {
    DDS_DataWriter *data_writer          = NULL;
    DDS_StringDataWriter *hello_writer   = NULL;
    char *instance                       = NULL;
    DDS_Boolean return_value             = DDS_BOOLEAN_FALSE;
    DDS_Long i;
    DDS_Long count;
    DDS_ReturnCode_t rc;
    struct DDS_Duration_t disc_period      = { 1, 0 };
    
    /* Creates the DDS Data writer. 
     * Just like before, if you want to customize the writer QoS,
     * use DDS_Publisher_get_default_datawriter_qos() to 
     * initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_DATAWRITER_QOS_DEFAULT.
     * For more data writer API info, see:
     *     $NDDSHOME/doc/html/api_c/group__DDSWriterModule.html
     */
    if (verbose) {
        printf("Creating the data writer...\n");
    }
    data_writer = DDS_DomainParticipant_create_datawriter(
                        participant, 
                        topic,
                        &DDS_DATAWRITER_QOS_DEFAULT,
                        NULL,           /* listener */
                        DDS_STATUS_MASK_NONE);
    if (data_writer == NULL) {
        fprintf(stderr, "! Unable to create DDS data writer\n");
        goto exitFn;
    }

    /* wait for discovery */
    NDDS_Utility_sleep(&disc_period);

    /* The following narrow function should never fail, as it performs 
     * only a safe cast of the generic data writer into a specific
     * DDS_StringDataWriter.
     * In our case the following cast would do the same job:
     *     hello_writer = (DDS_StringDataWriter *)data_writer;
     */
    hello_writer = DDS_StringDataWriter_narrow(data_writer);
    if (hello_writer == NULL) {
        fprintf(stderr, "! Unable to narrow data writer into "
                        "DDS_String writer\n");
        goto exitFn;
    }
    
    /* The string we are sending will have the following form:
     * - 10 characters (padded with spaces) will contain a sequence number,
     *   in ASCII character
     * - the rest of the string (data_size - 10) will contain just 'ABCDEF...'
     * The string buffer is managed by the application.
     */
    instance = DDS_String_alloc(data_size);
    if (instance == NULL) {
        fprintf(stderr, "! Unable to create an instance of the data\n");
        fprintf(stderr, "! This problem most likely is caused by "
                        "out of memory\n");
        goto exitFn;
    }
    

    /* Fill up the buffer with some valid data:
     * The content of the payload buffer is a progressive series of incremental
     * numbers, starting from 0.
     */
    for (i = 0; i < data_size-1; ++i) { 
        instance[i] = (i < 11) ? ' ' : ('A' + (i % 26));
    }
    instance[i] = '\0'; /* write() will use strlen to determine the length */
    
    /* Send the data! */
    printf("Sending data...\n");
    i = 0; /* Use 'i' to keep track of consecutive write errors */
    for (count = 0;; ++count ) {
        /* Conversion of an integer to a string in snprintf is relatively
         * expensive and could impact the throughput. But since this is
         * just an example, let's keep it simple.
         */
        RTI_SNPRINTF(instance, 10, "%9d", count);
        instance[9] = ' '; /* Remove the '\0' introduced by snprintf */
        rc = DDS_StringDataWriter_write(
                        hello_writer, 
                        instance, 
                        &DDS_HANDLE_NIL);
        if (rc != DDS_RETCODE_OK) {
            fprintf(stderr, "! Write error %d\n", rc);
            if (++i > MAX_CONSECUTIVE_WRITE_ERROR) {
                fprintf(stderr, "! Reached maximum number of failure, "
                       "stopping writer...\n");
                goto exitFn;
            }
        } else {
            i = 0;  /* Always clear the error count in case of successful write */
        }
        if (verbose && (count % 10000) == 0) {
            printf("Sent %d samples...\n", count);
            fflush(stdout);
        }
        /* Stop after sampleCount sent */
        if (sampleCount != 0 && (count >= sampleCount)) {
            printf("\nSent %d samples.\n", count);
            break;
        }
    }
    
    /* This code is reached only if sampleCount is set */
    return_value = DDS_BOOLEAN_TRUE;

exitFn:
    if (instance != NULL) {
        DDS_String_free(instance);
        instance = NULL;
    }
    
    return return_value;
}

