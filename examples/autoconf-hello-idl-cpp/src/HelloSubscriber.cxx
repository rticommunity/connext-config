/*****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.        */
/*                                                                           */
/*         Permission to modify and use for internal purposes granted.       */
/* This software is provided "as is", without warranty, express or implied.  */
/*                                                                           */
/*****************************************************************************/

#include <iostream>
#include <string>
#include <iomanip>

#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "Hello.h"
#include "HelloSubscriber.h"
#include "HelloWorld.h"
#include "HelloWorldSupport.h"

/* How often the subscriber prints statistics on the data received */
#define POLL_PERIOD_SEC         1

/* Just a convenience macro for max values */
#ifndef MACRO_MAX
#define MACRO_MAX(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#define ONE_MEGABYTE        (1024*1024)


/* The class containing DDS event callbacks */
class HelloListener: public DDSDataReaderListener {
private:
    const int _theVerbose;
    long _theLastSampleId;      // Sample ID of the last received sample
    long _thePayloadSize;       // Size of data payload (set once)
    long _theSampleLostCount;   // Total number of samples lost
    long _theSampleRcvdCount;   // Total number of samples received
    long _theSampleRcvdMax;     // Maximum number of samples to receive

// ---------------------------------------------------------------------------
// Object creation, copy and destruction
public:
    HelloListener(int verbose, int sampleCount):
        _theVerbose(verbose),
        _theLastSampleId(0),
        _thePayloadSize(0),
        _theSampleLostCount(0),
        _theSampleRcvdCount(0),
        _theSampleRcvdMax(sampleCount) {
    }
    
    ~HelloListener() {}
    
    HelloListener(const HelloListener &ref):
        _theVerbose(ref._theVerbose),
        _theLastSampleId(ref._theLastSampleId),
        _thePayloadSize(ref._thePayloadSize),
        _theSampleLostCount(ref._theSampleLostCount),
        _theSampleRcvdCount(ref._theSampleRcvdCount),
        _theSampleRcvdMax(ref._theSampleRcvdMax) {
    }

    HelloListener &operator=(const HelloListener &ref) {
        // verbosity remains the same
        _theLastSampleId = ref._theLastSampleId;
        _thePayloadSize  = ref._thePayloadSize;
        _theSampleLostCount = ref._theSampleLostCount;
        _theSampleRcvdCount = ref._theSampleRcvdCount;
        _theSampleRcvdMax = ref._theSampleRcvdMax;
        return *this;
    }

// ---------------------------------------------------------------------------
// Property getters
public:
    long getLastSampleId() const {
        return _theLastSampleId;
    }
    
    long getPayloadSize() const {
        return _thePayloadSize;
    }
    
    long getSampleLostCount() const {
        return _theSampleLostCount;
    }

    long getSampleRcvdCount() const {
        return _theSampleRcvdCount;
    }

    long getSampleRcvdMax() const {
        return _theSampleRcvdMax;
    }
    
    
// ---------------------------------------------------------------------------
// DDSDataReaderListener interface callbacks
public:    
    /* Process the data.
     * This method is invoked from the 'on_data_available' for every valid 
     * sample received.
     * It takes a reference (non-const) to the instance.
     */
    void processData(HelloWorld &instance) {
        /* If you want to access the inner payload buffer and verify the 
         * integrity of the data, uncomment the following section:
         *
        DDS_Octet *payloadBuffer = NULL;
        payloadBuffer = instance.payload.get_contiguous_buffer();
        
        for (int pos = 0; pos < instance.payload.length(); ++pos) {
            if (payloadBuffer[pos] != (pos & 0xff)) {
                std::cerr << "! data is not the same" << std::endl;
                return;
            }
        }
        */
        if (_thePayloadSize == 0) {
            // This is the first sample: save the payload size.
            _thePayloadSize = instance.payload.length();
        }
        _theLastSampleId = instance.sampleId;
        ++_theSampleRcvdCount;
    }


// ---------------------------------------------------------------------------
// DDSDataReaderListener interface callbacks
public:
    // For more information on the DDS_RequestedDeadlineMissedStatus struct:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__RequestedDeadlineMissedStatus.html
    void on_requested_deadline_missed(DDSDataReader* /*reader*/,
                        const DDS_RequestedDeadlineMissedStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: requested deadline missed." << std::endl;
        }
    }

    // For more information on the DDS_RequestedIncompatibleQosStatus struct:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__RequestedIncompatibleQosStatus.html
    void on_requested_incompatible_qos(DDSDataReader* /*reader*/,
                        const DDS_RequestedIncompatibleQosStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: requested incompatible Qos." 
                      << std::endl;
        }
    }
    
    // For more information on the DDS_SampleRejectedStatus structure:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__SampleRejectedStatus.html
    void on_sample_rejected(DDSDataReader* /*reader*/,
                        const DDS_SampleRejectedStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: sample rejected." << std::endl;
        }
    }

    // For more information on the DDS_LivelinessChangedStatus structure:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__LivelinessChangedStatus.html
    void on_liveliness_changed(DDSDataReader* /*reader*/,
                        const DDS_LivelinessChangedStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: liveliness changed." << std::endl;
        }
    }

    // For more information on the DDS_SampleLostStatus structure:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__SampleLostStatus.html
    void on_sample_lost(DDSDataReader* /*reader*/,
                        const DDS_SampleLostStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: sample lost." << std::endl;
        }
        ++_theSampleLostCount;
    }


    // For more information on the DDS_SubscriptionMatchedStatus structure:
    //  $NDDSHOME/doc/html/api_cpp/structDDS__SubscriptionMatchedStatus.html
    void on_subscription_matched(DDSDataReader* /*reader*/,
                        const DDS_SubscriptionMatchedStatus& /*status*/) {
        if (_theVerbose > 0) {
            std::cout << "->Callback: subscription matched." << std::endl;
        }
    }

    void on_data_available(DDSDataReader* reader) {
        HelloWorldSeq dataSeq;
        DDS_SampleInfoSeq infoSeq;
        DDS_ReturnCode_t rc;

        /* Receive up to sample count
         */
        if (_theSampleRcvdMax != 0 && 
            (_theSampleRcvdCount >= _theSampleRcvdMax)) {
            return;
        }
        
        /* Since we expect this function to get called quite often, here a
         * message is printed only if the verbosity is 2 or more
         */
        if (_theVerbose > 2) {
            std::cout << "->Callback: data available matched." << std::endl;
        }

        /* The following narrow function should never fail in our case, as 
         * we have only one reader in the application. It simply performs 
         * only a safe cast of the generic data reader into a specific
         * HelloWorldDataReader.
         */
        HelloWorldDataReader *helloReader = HelloWorldDataReader::narrow(reader);
        if (helloReader == NULL) {
            std::cerr << "! Unable to narrow data reader" << std::endl;
            return;
        }

        rc = helloReader->take(
                            dataSeq, 
                            infoSeq, 
                            DDS_LENGTH_UNLIMITED,
                            DDS_ANY_SAMPLE_STATE, 
                            DDS_ANY_VIEW_STATE, 
                            DDS_ANY_INSTANCE_STATE);
        if (rc == DDS_RETCODE_NO_DATA) {
            return;
        } else if (rc != DDS_RETCODE_OK) {
            std::cerr << "! Unable to take data from data reader, error " 
                      << rc << std::endl;
            return;
        }

        for (int i = 0; i < dataSeq.length(); ++i) {
            if (infoSeq[i].valid_data) {
                // Process the data
                processData(dataSeq[i]);            
            }
        }
    
        rc = helloReader->return_loan(dataSeq, infoSeq);
        if (rc != DDS_RETCODE_OK) {
            std::cerr << "! Unable to return loan, error "
                      << rc << std::endl;
        }
    }
};  // End of class HelloListener



/*****************************************************************************/
/* startSubscriber                                                           */
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
/*   The boolean value true if an error occurred.                            */
/*                                                                           */
/*****************************************************************************/
bool startSubscriber(DDSDomainParticipant *participant,
                        DDSTopic *topic,
                        DDS_Long verbose,
                        DDS_Long sampleCount) {
    
    DDSSubscriber *subscriber = NULL;
    DDSDataReader *dataReader = NULL;
    DDS_Duration_t poll_period = {POLL_PERIOD_SEC, 0};
    HelloListener listener(verbose, sampleCount);
    DDS_Duration_t disc_period = {1, 0};
    
    /* Create the subscriber. 
     * Just like the participant, if you want to customize the subscriber QoS,
     * use DDSDomainParticipant::get_instance()->get_default_subscriber_qos() 
     * to initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_SUBSCRIBER_QOS_DEFAULT.
     * For more info on subscriber API, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSSubscriberModule.html
     */
    if (verbose) {
        std::cout << "Creating the subscriber..." << std::endl;
    }
    subscriber = participant->create_subscriber(
                        DDS_SUBSCRIBER_QOS_DEFAULT,
                        NULL,           /* listener */
                        DDS_STATUS_MASK_NONE);
    if (subscriber == NULL) {
        std::cerr << "! Unable to create DDS Subscriber" << std::endl;
        return false;
    }
    
    /* Create the data reader.
     * Just like before, if you want to customize the reader QoS,
     * use DDSSubscriber::get_default_datareader_qos() to 
     * initialize a local copy of the default QoS, modify them, then
     * use them in the creation call below instead of 
     * DDS_DATAREADER_QOS_DEFAULT.
     * For more data reader API info, see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSReaderModule.html
     */
    if (verbose) {
        std::cout << "Creating the data reader..." << std::endl;
    }

    dataReader = subscriber->create_datareader(
                        topic,
                        DDS_DATAREADER_QOS_DEFAULT,
                        &listener,
                        DDS_STATUS_MASK_ALL);
    if (dataReader == NULL) {
        std::cerr << "! Unable to create DDS data reader" << std::endl;
        return false;
    }

    /* wait for discovery */
    NDDSUtility::sleep(disc_period);
    
    /* Statistics variables */
    long statFirstSequenceId = 0; // ID of first sample
    time_t timeNow;               // Time for every iteration
    time_t startTime = time(NULL);             // Time of first iteration w/samples
    long statDeltaTimeSec;        // Num. secs since first sample
    long lastSampleId = 0;        // Copy of the last sample_id
    long lastSampleLost = 0;      // Copy of the last sample_lost
    long prevSampleId = 0;        // ID of sample lost on prev. iteration
    long prevSampleLost = 0;      // Sample lost of prev. iteration
    long statTotalSamples = 0;    // Total # of msgs received
    long statSampleLost;          // Samples lost for the last period
    float statTotalSamplePerSec = 0.0;
    float statCurrentSamplePerSec = 0.0;
    float statThroughput = 0;
    
    /* Print header every 30 lines */
    std::cout << std::endl;
    /*            12345678|1234567890|1234567890|1234567890|1234567890|1234567890|1234567890 */
    std::cout << "Sec.from|Total     |Total Lost|Curr Lost |Average   |Current   |Current" << std::endl;
    std::cout << "start   |samples   |samples   |samples   |smpls/sec |smpls/sec |Mb/sec" << std::endl;
    std::cout << "--------+----------+----------+----------+----------+----------+----------" << std::endl;
    for (;;) {
        NDDS_Utility_sleep(&poll_period);

        if (sampleCount != 0 && (listener.getSampleRcvdCount() >= sampleCount)) {
            std::cout << "\nReceived " << listener.getSampleRcvdCount()
                      << " samples." << std::endl;
            break;
        }
    
        /* If the last sample_id received is < than the older one received,
         * it is because the publisher has restarted or there are two publishers
         * in the system.
         * In this case the stats won't be correct, so terminate the application
         */
        if (listener.getLastSampleId() < lastSampleId) {
            std::cout << "Detected multiple publishers, or the publisher was restarted." << std::endl;
            std::cout << "If you have multiple publishers on the network or you restart" << std::endl;
            std::cout << "the publisher, the statistics produced won't be accurate.";
            break;
        }
    
        /* Make a copy of the last sample received */
        lastSampleId   = listener.getLastSampleId();
        lastSampleLost = listener.getSampleLostCount();
        timeNow = time(NULL);

        if (lastSampleId == 0) {
            if (verbose) {
                std::cout << "No data..." << std::endl;
            }
            /* No data */
            continue;
        }

        /* If this is the first sample received, mark the sample ID and 
         * get the time.
         */
        if (statFirstSequenceId == 0) {
            statFirstSequenceId = lastSampleId;
            startTime = time(NULL);
            /* Don't consider this iteration in the statistics, as it is
             * not meaningful.
             */
            prevSampleId = lastSampleId;
            prevSampleLost = lastSampleLost;
            continue;
        }

        /* Then calculate the statistics */
        statDeltaTimeSec = (long)(timeNow - startTime);
        statTotalSamples = lastSampleId - statFirstSequenceId 
                        - lastSampleLost;
        statTotalSamplePerSec = (float)statTotalSamples / (float)statDeltaTimeSec;
        statCurrentSamplePerSec = (float)(lastSampleId - prevSampleId) 
                        / POLL_PERIOD_SEC;
        statSampleLost = lastSampleLost - prevSampleLost;
        statThroughput = listener.getPayloadSize() * statCurrentSamplePerSec *
                         8.0f /
                         ONE_MEGABYTE;
        // Finally prints the results to screen
        std::cout.setf(std::ios_base::fixed);
        std::cout.setf(std::ios_base::showpoint);
        std::cout << std::setfill(' ') << std::setw(8) << std::right << statDeltaTimeSec << ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::right << statTotalSamples<< ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::right << lastSampleLost<< ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::right << statSampleLost<< ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::setprecision(2) << std::right << statTotalSamplePerSec<< ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::setprecision(2) << std::right << statCurrentSamplePerSec<< ' ';
        std::cout << std::setfill(' ') << std::setw(10) << std::setprecision(2) << std::right << statThroughput<< ' ' << std::endl;
        prevSampleId = lastSampleId;
        prevSampleLost = lastSampleLost;
    }
    
    // This function terminates if sampleCount is set.
    dataReader->set_listener(NULL);
    return true;
}

