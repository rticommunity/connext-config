/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/

#include <iostream>
#include <string>
#include <cstdlib>
#include "Hello.h"
#include "HelloPublisher.h"
#include "HelloWorld.h"
#include "HelloWorldSupport.h"


/* Defines the maximum number of consecutive write errors. */
#define MAX_CONSECUTIVE_WRITE_ERROR         10



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
/*   The boolean value true if an error occurred.                            */
/*                                                                           */
/*****************************************************************************/
bool startPublisher(
                        DDSDomainParticipant *participant,
                        DDSTopic *topic,
                        DDS_Long verbose,
                        DDS_Long dataSize,
                        DDS_Long sampleCount) {

    DDSPublisher *publisher              = NULL;
    DDSDataWriter *dataWriter            = NULL;
    HelloWorldDataWriter *helloWriter    = NULL;
    HelloWorld *instance                 = NULL;
    DDS_Octet * payloadBuffer            = NULL;
    bool returnValue                     = false;
    DDS_Long i;
    DDS_ReturnCode_t rc;
    DDS_Duration_t send_period = {0,4}; /* time (sec, usec) to pause between bursts of 10,000 samples */
    DDS_Duration_t disc_period = {1,0};
    
    /* Create the publisher. 
     * Just like the participant, if you want to customize the publisher QoS,
     * use DDSDomainParticipant::get_instance()->get_default_publisher_qos() to 
     * initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_PUBLISHER_QOS_DEFAULT.
     * For more info on publisher API, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSPublisherModule.html
     */
    if (verbose) {
        std::cout << "Creating the publisher..." << std::endl;
    }
    publisher = participant->create_publisher(
                        DDS_PUBLISHER_QOS_DEFAULT,
                        NULL,           /* listener */
                        DDS_STATUS_MASK_NONE);
    if (publisher == NULL) {
        std::cerr << "! Unable to create DDS Publisher" << std::endl;
        goto exitFn;
    }

    /* Creates the DDS Data writer. 
     * Just like before, if you want to customize the writer QoS,
     * use DDSPublisher::get_default_datawriter_qos() to 
     * initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_DATAWRITER_QOS_DEFAULT.
     * For more data writer API info, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSWriterModule.html
     */
    if (verbose) {
        std::cout << "Creating the data writer..." << std::endl;
    }
    dataWriter = publisher->create_datawriter(
                        topic,
                        DDS_DATAWRITER_QOS_DEFAULT,
                        NULL,           /* listener */
                        DDS_STATUS_MASK_NONE);
    if (dataWriter == NULL) {
        std::cerr << "! Unable to create DDS data writer" << std::endl;
        goto exitFn;
    }

    /* wait for discovery */
    NDDSUtility::sleep(disc_period);

    /* The following narrow function should never fail, as it performs 
     * only a safe cast of the generic data writer into a specific
     * HelloWorldDataWriter.
     * In our case the following cast would do the same job:
     *
          helloWriter = static_cast<HelloWorldDataWriter *>(dataWriter);
     */
    helloWriter = HelloWorldDataWriter::narrow(dataWriter);
    if (helloWriter == NULL) {
        std::cerr << "! Unable to narrow data writer into HelloWorldDataWriter"
                  << std::endl;
        goto exitFn;
    }

    /* Everything is ready, start writing the data.
     * The create data function below simply allocates on the heap an 
     * instance of the data to send.
     * Alternatively you can allocate the HelloWorld instance on the
     * struct, and call the HelloWorld_initialize_ex() function
     * to initialize the field to a known value.
     * For more information on the user data type support, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSUserDataModule.html
     *
     * In this case we create an instance of the data on the heap, but
     * we tell the HelloWorld_create_data_ex to skip allocation of the
     * inner buffers (in our case, the 'prefix' string).
     */
    instance = HelloWorldTypeSupport::create_data_ex(
                        DDS_BOOLEAN_FALSE);
    if (instance == NULL) {
        std::cerr << "! Unable to create an instance of the data" << std::endl;
        std::cerr << "! This problem most likely is caused by out of memory"
                  << std::endl;
        goto exitFn;
    }

    /* Sets the length of the sequence we are sending */
    if (!DDS_OctetSeq_ensure_length(&instance->payload, 
                        dataSize, 
                        HELLODDS_MAX_PAYLOAD_SIZE)) {
        std::cerr << "! Unable to set payload size to " << dataSize 
                  << std::endl;
        std::cerr << "! Perhaps you are using a value too big "
                  << "(max allowed size is"  
                  << HELLODDS_MAX_PAYLOAD_SIZE
                  << ")" << std::endl;
        goto exitFn;
    }
    
    /* The payloadBuffer has been already allocated during the previous call
     * now we get a pointer to the buffer used to manage the sequence
     * so we can address directly the bytes.
     * For more information on sequence, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSSequenceModule.html
     *
     */
    payloadBuffer = instance->payload.get_contiguous_buffer();
    

    /* Fill up the buffer with some valid data:
     * The content of the payload buffer is a progressive series of incremental
     * numbers, starting from 0.
     */
    for (i = 0; i < dataSize; ++i) {
        payloadBuffer[i] = (i & 0xff);
    }
    
    sprintf(instance->prefix, "Hello world");
    
    /* Send the data! */
    std::cout << "Sending data..." << std::endl;
    i = 0; /* Use 'i' to keep track of consecutive write errors */
    for (instance->sampleId = 0;; ++(instance->sampleId) ) {
        rc = helloWriter->write(
                        *instance, 
                        DDS_HANDLE_NIL);
        if (rc != DDS_RETCODE_OK) {
            std::cerr << "! Write error " <<  rc << std::endl;
            if (++i > MAX_CONSECUTIVE_WRITE_ERROR) {
                std::cerr << "! Reached maximum number of failure, "
                          << "stopping writer..." << std::endl;
                goto exitFn;
            }
        } else {
            i = 0;  /* Always clear the error count in case of successful write */
        }
        if ((instance->sampleId % 10000) == 0) {
            if (verbose) {
                std::cout << "Sent " << instance->sampleId 
                          << " samples..." << std::endl;
            }
        NDDSUtility::sleep(send_period);
        }
        if (sampleCount != 0 && instance->sampleId >= sampleCount) {
            std::cout << "\nSent " << instance->sampleId 
                      << " samples." << std::endl;
            break;
        }
    }
    
    /* This code is reached if sampleCount is set */
    returnValue = true;

exitFn:
    if (instance != NULL) {
        HelloWorldTypeSupport::delete_data_ex(instance,
                        DDS_BOOLEAN_FALSE);
        instance = NULL;
    }
    
    return returnValue;
}

