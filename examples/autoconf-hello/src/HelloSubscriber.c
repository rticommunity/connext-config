/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "Hello.h"
#include "HelloSubscriber.h"

/* How often the subscriber prints statistics on the data received */
#define POLL_PERIOD_SEC         1

/* Just a convenience macro for max values */
#ifndef MACRO_MAX
#define MACRO_MAX(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#define ONE_MEGABYTE        (1024*1024)

/* The structure passed as user data to the callback functions */
typedef struct CallbackData_t {
    DDS_Boolean verbose;
    long sample_id;             /* Sample ID of the last received sample     */
    long payload_size;          /* Size of data payload (set once)           */
    long sample_lost;           /* Total number of samples lost              */
    long sample_rcvd;           /* Total number of samples received          */
    long sample_rcvd_max;       /* Maximum number of samples to receive      */                                                
} CallbackData;


/*****************************************************************************/
/* CallbackData_initialize                                                   */
/*                                                                           */
/* Initialize the callback data structure                                    */
/*****************************************************************************/
static void CallbackData_initialize(CallbackData *ptr) {
    ptr->verbose = DDS_BOOLEAN_FALSE;
    ptr->sample_id = 0L;
    ptr->payload_size = 0L;
    ptr->sample_lost = 0L;
    ptr->sample_rcvd = 0L;
    ptr->sample_rcvd_max = 0L;
}


/*****************************************************************************/
/* process_data                                                              */
/*                                                                           */
/* Called from the OnDataAvailable for every valid sample received           */
/*                                                                           */
/*****************************************************************************/
static void process_data(
                        CallbackData *me,
                        const char * const instance) {
    char idChar[11];
    memcpy(idChar, instance, 10);
    idChar[10] = '\0';
    
    /* Updates the counters in the callback data structure.
     *
     * Parsing the string into an integer in sscanf is relatively
     * expensive and could impact the throughput. But since this is
     * just an example, let's keep it simple.
     */
    RTI_SSCANF(idChar, "%ld", &me->sample_id);
    ++(me->sample_rcvd);
    if (me->payload_size == 0) {
        /* This is the first sample, save the payload size */
        me->payload_size = (long)strlen(instance);
    }
}


/*****************************************************************************/
/* on_requested_deadline_missed                                              */
/*                                                                           */
/* For more information on the DDS_RequestedDeadlineMissedStatus structure:  */
/*  $NDDSHOME/doc/html/api_c/structDDS__RequestedDeadlineMissedStatus.html   */
/*                                                                           */
/*****************************************************************************/
static void on_requested_deadline_missed(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_RequestedDeadlineMissedStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: requested deadline missed.\n");
    }
}


/*****************************************************************************/
/* on_requested_incompatible_qos                                             */
/*                                                                           */
/* For more information on the DDS_RequestedIncompatibleQosStatus structure: */
/*  $NDDSHOME/doc/html/api_c/structDDS__RequestedIncompatibleQosStatus.html  */
/*                                                                           */
/*****************************************************************************/
static void on_requested_incompatible_qos(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_RequestedIncompatibleQosStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: requested incompatible Qos.\n");
    }
}


/*****************************************************************************/
/* on_sample_rejected                                                        */
/*                                                                           */
/* For more information on the DDS_SampleRejectedStatus structure:           */
/*  $NDDSHOME/doc/html/api_c/structDDS__SampleRejectedStatus.html            */
/*                                                                           */
/*****************************************************************************/
static void on_sample_rejected(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_SampleRejectedStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: sample rejected.\n");
    }
}


/*****************************************************************************/
/* on_liveliness_changed                                                     */
/*                                                                           */
/* For more information on the DDS_LivelinessChangedStatus structure:        */
/*  $NDDSHOME/doc/html/api_c/structDDS__LivelinessChangedStatus.html         */
/*                                                                           */
/*****************************************************************************/
static void on_liveliness_changed(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_LivelinessChangedStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: liveliness changed.\n");
    }
}


/*****************************************************************************/
/* on_sample_lost                                                            */
/*                                                                           */
/* For more information on the DDS_SampleLostStatus structure:               */
/*  $NDDSHOME/doc/html/api_c/structDDS__SampleLostStatus.html                */
/*                                                                           */
/*****************************************************************************/
static void on_sample_lost(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_SampleLostStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: sample lost.\n");
    }
    ++(me->sample_lost);
}


/*****************************************************************************/
/* on_subscription_matched                                                   */
/*                                                                           */
/* For more information on the DDS_SubscriptionMatchedStatus structure:      */
/*  $NDDSHOME/doc/html/api_c/structDDS__SubscriptionMatchedStatus.html       */
/*                                                                           */
/*****************************************************************************/
static void on_subscription_matched(
                        void* listener_data,
                        DDS_DataReader* reader,
                        const struct DDS_SubscriptionMatchedStatus *status) {
    CallbackData *me = (CallbackData *)listener_data;
    if (me->verbose) {
        printf("->Callback: subscription matched.\n");
        fflush(stdout);
    }
}


/*****************************************************************************/
/* on_data_available                                                         */
/*                                                                           */
/*****************************************************************************/
static void on_data_available(
                        void* listener_data,
                        DDS_DataReader* reader) {
    CallbackData *me                  = (CallbackData *)listener_data;
    DDS_StringDataReader *helloReader = NULL;
    struct DDS_StringSeq data_seq      = DDS_SEQUENCE_INITIALIZER;
    struct DDS_SampleInfoSeq info_seq  = DDS_SEQUENCE_INITIALIZER;
    DDS_ReturnCode_t rc;
    int i;

    /* Receive up to max sample count */
    if (me->sample_rcvd_max != 0 &&
        (me->sample_rcvd >= me->sample_rcvd_max)) {
        return;
    }

    /* Since we expect this function to get called quite often, here a
     * message is printed only if the verbosity is 2 or more
     */
    if (me->verbose > 2) {
        printf("->Callback: data available matched.\n");
    }

    /* The following narrow function should never fail in our case, as 
     * we have only one reader in the application. It simply performs 
     * only a safe cast of the generic data reader into a specific
     * DDS_StringDataReader.
     */
    helloReader = DDS_StringDataReader_narrow(reader);
    if (helloReader == NULL) {
        fprintf(stderr, "! Unable to narrow data reader into "
                        "DDS_StringDataReader\n");
        return;
    }

    rc = DDS_StringDataReader_take(
                        helloReader,
                        &data_seq, 
                        &info_seq, 
                        DDS_LENGTH_UNLIMITED,
                        DDS_ANY_SAMPLE_STATE, 
                        DDS_ANY_VIEW_STATE, 
                        DDS_ANY_INSTANCE_STATE);
    if (rc == DDS_RETCODE_NO_DATA) {
        return;
    } else if (rc != DDS_RETCODE_OK) {
        fprintf(stderr, "! Unable to take data from data reader, "
                        "error %d\n", rc);
        return;
    }

    for (i = 0; i < DDS_StringSeq_get_length(&data_seq); ++i) {
        if (DDS_SampleInfoSeq_get_reference(&info_seq, i)->valid_data) {
            /* Process the data */
            process_data(me, (char *)DDS_StringSeq_get(&data_seq, i));
        }
    }

    /* Finally tell DDS that we are done using the sample. Sample can be
     * recycled by DDS, according to Qos settings.
     */
    rc = DDS_StringDataReader_return_loan(
                        helloReader,
                        &data_seq, 
                        &info_seq);
    if (rc != DDS_RETCODE_OK) {
        fprintf(stderr, "! Unable to return loan, error %d\n", rc);
    }
}



/*****************************************************************************/
/* start_subscriber                                                          */
/*                                                                           */
/* Creates the DDS subscriber and data readers, then start receiving samples */
/* and prints statistics on the screen                                       */
/*                                                                           */
/* Input:                                                                    */
/*   participant: the DDS domain participant to use                          */
/*   topic: the DDS topic to use for publishing                              */
/*   libraryName: name of the library to use for entity creation             */
/*   profileName: name of the profile to use for entity creation             */
/*                                                                           */
/* Returns:                                                                  */
/*   The boolean value DDS_BOOLEAN_TRUE if an error occurred.                */
/*                                                                           */
/*****************************************************************************/
DDS_Boolean start_subscriber(DDS_DomainParticipant *participant,
                        DDS_Topic *topic,
                        DDS_Boolean verbose,
                        DDS_Long sample_count) {
    
    DDS_DataReader *data_reader            = NULL;
    struct DDS_DataReaderListener listener = DDS_DataReaderListener_INITIALIZER;
    struct DDS_Duration_t poll_period      = { POLL_PERIOD_SEC, 0 };
    CallbackData callback_data;
    struct DDS_Duration_t disc_period      = { 1, 0 };
    
    /* Statistics variables */
    long stat_first_sequence_id = 0;    /* ID of first sample */
    time_t time_now;            /* Time for every iteration */
    time_t start_time = time(NULL);          /* Time of first iteration w/samples */
    long stat_delta_time_sec;   /* Num. secs since first sample */
    long last_sample_id = 0;    /* Copy of the last sample_id */
    long last_sample_lost = 0;  /* Copy of the last sample_lost */
    long prev_sample_id = 0;    /* ID of sample lost on prev. iteration */
    long prev_sample_lost = 0;  /* Sample lost of prev. iteration */
    long stat_total_samples = 0;/* Total # of msgs received */
    long stat_samples_lost;     /* Samples lost for the last period */
    float stat_total_sample_per_sec = 0.0;
    float stat_current_sample_per_sec = 0.0;
    float stat_throughput = 0;
    
    /* Initialize the callback data that contains all the counters used for
     * statistics.
     */
    CallbackData_initialize(&callback_data);
    callback_data.verbose = verbose;
    callback_data.sample_rcvd_max = sample_count;
    
    /* By using the built-in types, you don't need to create a subscriber,
     * instead you can use DDS_DomainParticipant_create_datareader
     * that will automatically use the implicit subscriber.
     */
    
    /* Set up the data reader listener. Use the user data of the listener
     * to communicate the verbosity.
     */
    listener.as_listener.listener_data = &callback_data;
    listener.on_requested_deadline_missed = on_requested_deadline_missed;
    listener.on_requested_incompatible_qos = on_requested_incompatible_qos;
    listener.on_sample_rejected = on_sample_rejected;
    listener.on_liveliness_changed = on_liveliness_changed;
    listener.on_sample_lost = on_sample_lost;
    listener.on_subscription_matched = on_subscription_matched;
    listener.on_data_available = on_data_available;

    /* Create the data reader.
     * Just like before, if you want to customize the reader QoS,
     * use DDS_Subscriber_get_default_datareader_qos() to 
     * initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_DATAREADER_QOS_DEFAULT.
     * For more data reader API info, see:
     *     $NDDSHOME/doc/html/api_c/group__DDSReaderModule.html
     */
    if (verbose) {
        printf("Creating the data reader...\n");
    }

    data_reader = DDS_DomainParticipant_create_datareader(
                        participant,
                        DDS_Topic_as_topicdescription(topic),
                        &DDS_DATAREADER_QOS_DEFAULT,
                        &listener,
                        DDS_STATUS_MASK_ALL);
    if (data_reader == NULL) {
        fprintf(stderr, "! Unable to create DDS data reader\n");
        return DDS_BOOLEAN_FALSE;
    }

    /* wait for discovery */
    NDDS_Utility_sleep(&disc_period);
    
    /* Main loop */
    /* Print header every 30 lines */
    printf("\n");
    /*      12345678|1234567890|1234567890|1234567890|1234567890|1234567890|1234567890 */
    printf("Sec.from|Total     |Total Lost|Curr Lost |Average   |Current   |Current\n");
    printf("start   |samples   |samples   |samples   |smpls/sec |smpls/sec |Mb/sec\n");
    printf("--------+----------+----------+----------+----------+----------+----------\n");
    for (;;) {
        NDDS_Utility_sleep(&poll_period);

        /* Stop after sampleCount has been received */
        if (sample_count != 0 && (callback_data.sample_rcvd >= sample_count)) {
            printf("\nReceived %ld samples.\n", callback_data.sample_rcvd);
            break;
        }
    
        /* If the last sample_id received is < than the older one received,
         * it is because the publisher has restarted or there are two publishers
         * in the system.
         * In this case the stats won't be correct, so terminate the application
         */
        if (callback_data.sample_id < last_sample_id) {
            printf("Detected multiple publishers, or the publisher was restarted.\n");
            printf("If you have multiple publishers on the network or you restart\n");
            printf("the publisher, the statistics produced won't be accurate.");
            break;
        }
    
        /* Make a copy of the last sample received */
        last_sample_id   = callback_data.sample_id;
        last_sample_lost = callback_data.sample_lost;
        time_now = time(NULL);

        if (last_sample_id == 0) {
            if (verbose) {
                printf("No data...\n");
                fflush(stdout);
            }
            /* No data */
            continue;
        }

        /* If this is the first sample received, mark the sample ID and 
         * get the time 
         */
        if (stat_first_sequence_id == 0) {
            stat_first_sequence_id = last_sample_id;
            start_time = time(NULL);
            /* Don't consider this iteration in the statistics, as it is
             * not meaningful.
             */
            prev_sample_id = last_sample_id;
            prev_sample_lost = last_sample_lost;
            continue;
        }

        /* Then calculate the statistics */
        stat_delta_time_sec = (long)(time_now - start_time);
        stat_total_samples = last_sample_id - stat_first_sequence_id 
                        - last_sample_lost;
        stat_total_sample_per_sec = (float)stat_total_samples / (float)stat_delta_time_sec;
        stat_current_sample_per_sec = (float)(last_sample_id - prev_sample_id) 
                        / POLL_PERIOD_SEC;
        stat_samples_lost = last_sample_lost - prev_sample_lost;
        stat_throughput = callback_data.payload_size *
                          stat_current_sample_per_sec *
                          8.0f /
                          ONE_MEGABYTE;
        /* Finally prints the results to screen */
        printf("%8ld %10ld %10ld %10ld %10.2f %10.2f %10.2f\n",
                        stat_delta_time_sec,
                        stat_total_samples,
                        last_sample_lost,
                        stat_samples_lost,
                        stat_total_sample_per_sec,
                        stat_current_sample_per_sec,
                        stat_throughput);

        prev_sample_id = last_sample_id;
        prev_sample_lost = last_sample_lost;
    }
    
    /* This function terminates if sample_count is set ... */
    DDS_DataReader_set_listener(data_reader, NULL, DDS_STATUS_MASK_NONE);
    return DDS_BOOLEAN_TRUE;
}

