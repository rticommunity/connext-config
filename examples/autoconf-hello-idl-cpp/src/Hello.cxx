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
#include <fstream>
#include "ndds/ndds_cpp.h"
#include "Hello.h"
#include "HelloWorld.h"
#include "HelloWorldSupport.h"
#include "HelloPublisher.h"
#include "HelloSubscriber.h"

/*****************************************************************************/
/* Local constants & macros                                                  */
/*****************************************************************************/

/* Command line defaults */
#define DEFAULT_DOMAIN_ID           0
#define APPLICATION_NAME            "Hello"
#define DEFAULT_PAYLOAD             1024

/* Application running mode */
#define APP_MODE_UNDEFINED          0
#define APP_MODE_PUBLISHER          1
#define APP_MODE_SUBSCRIBER         2

/* This is the name of the topic used for our test */
#define DEFAULT_TOPIC_NAME          "Hello IDL"

/* The maximum value for Domain ID. The effective value depends on the QoS,
 * in particular to the value of the field:
 *              DDS_RtpsWellKnownPorts_t.domainId_gain
 * The default value is 250, so we just limit to that statically so we can
 * check the validity before attempting to create the domain participant.
 */
#define DOMAIN_ID_MAX               250


/*****************************************************************************/
/* Structure to hold the parsed command line arguments                       */
/*****************************************************************************/
struct CommandLineArguments {
    DDS_Long    domainId;
    DDS_Long    mode;
    DDS_Long    dataSize;
    DDS_Long    sampleCount;
    DDS_Long    verbose;        /* 0=no verbose, 1=app verbose, 2=DDS */
    std::string topicName;
    
    CommandLineArguments() {
        mode      = APP_MODE_UNDEFINED;
        domainId  = DEFAULT_DOMAIN_ID;
        dataSize  = DEFAULT_PAYLOAD;
        sampleCount = 0;
        verbose   = 0;
        topicName = DEFAULT_TOPIC_NAME;
    }
};



/*****************************************************************************/
/* Overload the std::ostream::operator<<() to print a                        */
/* DDS_ProductVersion_t.                                                     */
/*****************************************************************************/
std::ostream &operator<<(std::ostream &os, const DDS_ProductVersion_t &arg) {
    os << (int)arg.major << '.' 
       << (int)arg.minor
       << (char)arg.release << " (build "
       << (int)arg.revision << ')';
    return os;
}

/*****************************************************************************/
/* fileExist                                                                 */
/*                                                                           */
/* Just a convenience function that checks if a file exist and can be read   */
/*****************************************************************************/
static bool fileExist(const char *fileName) {
    std::ifstream stream;
    stream.open(fileName);
    if (!stream) {
        return false;
    }
    stream.close();
    return true;
}


/*****************************************************************************/
/* start_application                                                         */
/*                                                                           */
/* Creates the DDS Domain Participant, then delegates the execution to the   */
/* correct pub/sub function.                                                 */
/* If entity creation is successful, this function does not return.          */
/*                                                                           */
/* Input:                                                                    */
/*   arg: pointer to the CommandLineArguments structure                      */
/*                                                                           */
/* Returns:                                                                  */
/*   false if an error occurred.                                             */
/*                                                                           */
/*****************************************************************************/
static bool startApplication(
                        const CommandLineArguments &arg) {
    DDS_ReturnCode_t         rc;
    DDSDomainParticipant *   participant = NULL;
    DDSTopic *               topic = NULL;
    bool                     returnValue = false;

    std::cout <<
        "# The output below depends on the QoS profile"        << std::endl << 
        "# provided to this application."                      << std::endl <<
        "# -> For more information on the provided example"    << std::endl <<
        "#    profiles, please see the Getting Started Guide." << std::endl <<
        "# -> For detailed product performance metrics, visit" << std::endl <<
        "#    http://www.rti.com/products/data_distribution/index.html" << std::endl <<
        "#    and click on Benchmarks."           << std::endl << std::endl;

    /* This example creates DDS entities using the default QoS.
     * The default QoS can be modified in two ways:
     * 1. By placing a file called 'USER_QOS_PROFILES.xml' in the directory
     *    where the application is launched.
     * 2. By setting the environment variable 'NDDS_QOS_PROFILES' to point
     *    to a valid file containing NDDS QoS profile definitions.
     *
     * This section check if a QoS profile file is accessible, and prints
     * a warning if that's not true.
     */
     
    /* First look in the current directory to see if the USER_QOS_PROFILES.xml
     * file exist.
     */
    if (!fileExist("USER_QOS_PROFILES.xml")) {
        /* Then look for the env variable 'NDDS_QOS_PROFILES'... 
         */
        char *envVal;
#if defined(RTI_WIN32) && (_MSC_VER > 1310) 
        size_t envValSize;
        _dupenv_s(&envVal, &envValSize, "NDDS_QOS_PROFILES"); 
#else 
        envVal = getenv("NDDS_QOS_PROFILES");
#endif
        if (envVal == NULL || !fileExist(envVal)) {
            std::cout << "! Warning:" << std::endl
                      << "! Default QoS profile definition file not found." 
                      << std::endl
                      << "! The application will use the DDS default QoS." 
                      << std::endl
                      << "! If you want to use different QoS, make sure you "
                      << "have the QoS definition file"
                      << std::endl
                      << "! (USER_QOS_PROFILES.xml) in the current working "
                      << "directory"
                      << std::endl
                      << "! or set the environment variable NDDS_QOS_PROFILES "
                      << "to point"
                      << std::endl
                      << "! to a file containing the default QoS profile"
                      << std::endl;
        }
#if defined(RTI_WIN32) && (_MSC_VER > 1310) 
        if (envVal) free(envVal);
#endif
    }

    /* If you need to customize any DDS factory QoS, uncomment the following
     * code: 
     *
    {
        DDS_DomainParticipantFactoryQos factoryQos;
        rc = DDSDomainParticipantFactory::get_instance()->get_qos(factoryQos);
        if (rc != DDS_RETCODE_OK) { 
            std::cerr << "! Unable to get participant factory QoS: "
                      << rc << std::endl;
            goto exitFn;
        }
        
        // Modify the factory QoS here
        
        rc = DDSDomainParticipantFactory::get_instance()->set_qos(factoryQos);
        if (rc != DDS_RETCODE_OK) { 
            std::cerr << "! Unable to set participant factory QoS: "
                      << rc << std::endl;
            goto exitFn;
        }
    }
    */
    
            
    /* Creates the DDS Domain Participant.
     * The following command will create a DDS Domain participant without
     * installing any status callback listener.
     * If you want to create a domain participant with different QoS,
     * use DDSDomainParticipantFactory::get_default_participant_qos
     * to obtain a copy of the default participant QoS, change them,
     * then use them instead of DDS_PARTICIPANT_QOS_DEFAULT:
     *
     
        DDS_DomainParticipantQos myQos;
        DDSDomainParticipantFactory::get_instance()->get_default_participant_qos(
                        myQos);
        // Modify the participant QoS here

        // Then create the domain participant using 'myQos'...
     *
     * Note: for more info on the domain participant API see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSDomainParticipantModule.html
     */
    if (arg.verbose) {
        std::cout << "Creating domain participant..." << std::endl;
    }
    participant = DDSDomainParticipantFactory::get_instance()->
                        create_participant(
                        arg.domainId,
                        DDS_PARTICIPANT_QOS_DEFAULT,
                        NULL,   /* Listener */
                        DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        std::cerr << "! Unable to create DDS domain participant" << std::endl;
        goto exitFn;
    }


    /* Registers the type used in this example with the domain participant.
     * For more information on user-defined data types:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSUserDataModule.html
     */
    if (arg.verbose) {
        std::cout << "Registering the type..." << std::endl;
    }
    rc = HelloWorldTypeSupport::register_type(
                        participant, 
                        HelloWorldTypeSupport::get_type_name());
    if (rc != DDS_RETCODE_OK) {
        std::cerr << "! Unable to register '"
                  << HelloWorldTypeSupport::get_type_name() 
                  << "' data type: " 
                  << rc << std::endl;
        goto exitFn;
    }


    /* Creates the topic.
     * The following command will create the topic without
     * installing any status callback listener.
     * If you want to create a topic with different QoS,
     * use DDS_DomainParticipant_get_default_topic_qos
     * to obtain a copy of the default topic QoS, change them,
     * then use them instead of DDS_TOPIC_QOS_DEFAULT:
     *
     {
        DDS_TopicQos myQos;
        participant->get_default_topic_qos(myQos);
        
        // Modify the topic QoS here

        // Then create the topic using 'myQos'...        
     }
     *
     * Note: for more info on the topic API see:
     *     $NDDSHOME/doc/html/api_cpp/group__DDSTopicEntityModule.html
     */
    if (arg.verbose) {
        std::cout << "Creating the topic..." << std::endl;
    }
    topic = participant->create_topic(
                        arg.topicName.c_str(),
                        HelloWorldTypeSupport::get_type_name(), 
                        DDS_TOPIC_QOS_DEFAULT,
                        NULL,           /* listener */
                        DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        std::cerr << "! Unable to create topic '"
                  << arg.topicName
                  << std::endl;
        goto exitFn;
    }

    /* Creates the publisher or subscriber */
    if (arg.mode == APP_MODE_PUBLISHER) {
        if (!startPublisher(
                        participant, 
                        topic,
                        arg.verbose,
                        arg.dataSize,
                        arg.sampleCount)) {
            goto exitFn;
        }
    } else {
        if (!startSubscriber(
                        participant, 
                        topic,
                        arg.verbose,
                        arg.sampleCount)) {
            goto exitFn;
        }
    }
    returnValue = true;
    
exitFn:
    if (arg.verbose) {
        std::cout << "Cleaning up..." << std::endl;
    }
    if (participant != NULL) {
        /* Perform a clean shutdown of the participant and all the contained
         * entities
         */
        rc = participant->delete_contained_entities();
        if (rc != DDS_RETCODE_OK) {
            std::cerr << "! Unable to delete participant contained entities: "
                      << rc << std::endl;
        }

        rc = DDSDomainParticipantFactory::get_instance()->delete_participant(
                        participant);
        if (rc != DDS_RETCODE_OK) {
            std::cerr << "! Unable to delete participant: " << rc << std::endl;
        }
    }
    return returnValue;
}




/*****************************************************************************/
/* usage                                                                     */
/*                                                                           */
/* Prints help on command line accepted by this application                  */
/*****************************************************************************/
static void usage() {
    std::cout << "Usage:" << std::endl;
    std::cout << "    " << APPLICATION_NAME 
                        << " pub [arguments]     Run as publisher" << std::endl;
    std::cout << "    " << APPLICATION_NAME 
                        << " sub [arguments]     Run as subscriber" << std::endl;
    std::cout << "Where arguments are:" << std::endl;
    std::cout << "  -h | --help                 "
              << "Shows this page" << std::endl;
    std::cout << "  -v | --verbose              "
              << "Increase output verbosity (can be repeated)" << std::endl;
    std::cout << "  -d | --domain <domainID>    "
              << "Sets the DDS domain ID [default=" << DEFAULT_DOMAIN_ID 
              << ']' << std::endl;
    std::cout << "  -t | --topic <name>         "
              << "Sets topic name [default=" << DEFAULT_TOPIC_NAME
              << ']' << std::endl;
    std::cout << "  -s | --size <num>           "
              << "Sets payload size in bytes [default=" << DEFAULT_PAYLOAD
              << ']' << std::endl;
    std::cout << "  -c | --sampleCount <num>    "
              << "Sets number of samples to send/receive [default=0(UNLIMITED)"
              << ']' << std::endl;
    std::cout << std::endl;
}


/*****************************************************************************/
/* ENSURE_ONE_MORE_ARGUMENT                                                  */
/*                                                                           */
/* A simple macro used to check if there are enough command line args        */
/*****************************************************************************/
#define ENSURE_ONE_MORE_ARGUMENT(argc, argv, i, argName)                      \
    if (i+1 >= argc) {                                                         \
        usage();                                                              \
        std::cerr << "! Error: missing value for " << argName                 \
                  << " argument" << std::endl;                                \
        return EXIT_FAILURE;                                                  \
    }


/*****************************************************************************/
/* main                                                                      */
/*                                                                           */
/* Application main entry point                                              */
/*****************************************************************************/
int main(int argc, const char **argv) {
    CommandLineArguments arg;    
    int i;

    std::cout << "Hello Example Application" << std::endl;
    std::cout << "Copyright 2008 Real-Time Innovations, Inc.\n" << std::endl;

    /* Ensure there are enough arguments in the command line */
    if (argc < 2) {
        usage();
        std::cerr << "! Invalid number of arguments." << std::endl;
        std::cerr << "! You must specify at least running mode (pub/sub)" 
                  << std::endl;
        return EXIT_FAILURE;
    }

    /* Parse the running mode: pub/sub */    
    if (RTI_STRCASECMP(argv[1], "pub") == 0) {
        arg.mode = APP_MODE_PUBLISHER;
    } else if (RTI_STRCASECMP(argv[1], "sub") == 0) {
        arg.mode = APP_MODE_SUBSCRIBER;
    } else if (!RTI_STRNCMP(argv[1], "-h", 2) || 
               !RTI_STRNCMP(argv[1], "--help", 6)) {
        usage();
        return EXIT_SUCCESS;
    } else {
        usage();
        std::cerr << "! Invalid mode: '" << argv[1] << "'" << std::endl;
        std::cerr << "! Valid modes are only 'pub' or 'sub'" << std::endl;
        return EXIT_FAILURE;
    }

    /* Parse the optional arguments */
    for (i = 2; i < argc; ++i) {
        if (!RTI_STRNCMP(argv[i], "-h", 2) || 
            !RTI_STRNCMP(argv[i], "--help", 6)) {
            usage();
            return EXIT_SUCCESS;
        }
        
        if (!RTI_STRNCMP(argv[i], "-v", 2) || 
            !RTI_STRNCMP(argv[i], "--verbose", 9)) {
            ++arg.verbose;
            continue;
        }

        if (!RTI_STRNCMP(argv[i], "-d", 2) || 
            !RTI_STRNCMP(argv[i], "--domain", 8)) {
            char *ptr;
            ENSURE_ONE_MORE_ARGUMENT(argc, argv, i, "--domain")
            arg.domainId = strtol(argv[++i], &ptr, 10);
            if (*ptr != '\0') {
                usage();
                std::cerr << "! Value of --domain argument is not a number: " 
                          << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            if (arg.domainId < 0 || arg.domainId > DOMAIN_ID_MAX) {
                usage();
                std::cerr << "! Invalid DDS Domain ID: " << arg.domainId
                          << std::endl;
                std::cerr << "! The domain ID must be between 0 and "
                          << DOMAIN_ID_MAX << " (inclusive)" << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }

        if (!RTI_STRNCMP(argv[i], "-s", 2) || 
            !RTI_STRNCMP(argv[i], "--size", 6)) {
            char *ptr;
            ENSURE_ONE_MORE_ARGUMENT(argc, argv, i, "--size")
            arg.dataSize = strtol(argv[++i], &ptr, 10);
            if (*ptr != '\0') {
                usage();
                std::cerr << "! Invalid value for --size option:"
                          << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            if (arg.dataSize <= 0) {
                usage();
                std::cerr << "! Invalid value for --size argument: "
                          << arg.dataSize << std::endl;
                return EXIT_FAILURE;
            }
            if (arg.dataSize >= HELLODDS_MAX_PAYLOAD_SIZE) {
                usage();
                std::cerr << "! Value too big for --size argument: "
                          << arg.dataSize << std::endl;
                std::cerr << "! In this example the max size is set to: " 
                          << HELLODDS_MAX_PAYLOAD_SIZE << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }

        if (!RTI_STRNCMP(argv[i], "-t", 2) || 
            !RTI_STRNCMP(argv[i], "--topic", 7)) {
            ENSURE_ONE_MORE_ARGUMENT(argc, argv, i, "--topic")
            arg.topicName = std::string(argv[++i]);
            continue;
        }

        if (!RTI_STRNCMP(argv[i], "-c", 2) || 
            !RTI_STRNCMP(argv[i], "--sampleCount", 13)) {
            char *ptr;
            ENSURE_ONE_MORE_ARGUMENT(argc, argv, i, "--sampleCount")
            arg.sampleCount = strtol(argv[++i], &ptr, 10);
            if (*ptr != '\0') {
                usage();
                std::cerr << "! Invalid value for --sampleCount option:"
                          << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            if (arg.sampleCount < 0) {
                usage();
                std::cerr << "! Invalid value for --sampleCount argument: "
                          << arg.dataSize << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }
        
        usage();
        std::cerr << "! Unknown argument " << argv[i] << std::endl;
        return EXIT_FAILURE;
    } /* foreach argument */

    if (arg.verbose) {
        const DDS_ProductVersion_t &prodVersion = 
                        NDDSConfigVersion::get_instance().get_product_version();
        std::cout << "Running with the following arguments:" << std::endl;
        std::cout << "    Payload size..... : " << arg.dataSize << std::endl;
        std::cout << "    Sample count..... : " << arg.sampleCount << std::endl;
        std::cout << "    Domain ID........ : " << arg.domainId << std::endl;
        std::cout << "    Topic name....... : " << arg.topicName << std::endl;
        std::cout << "RTI Product Version.. : " << prodVersion << std::endl;        

        /* Alternatively, you can print all the components of the 
         * NDDSConfigVersion object with a single call to:
         *       NDDSConfigVersion::get_instance().to_string()
         */
    }

    /* If the verbosity is bigger than 1, also turn on RTI DDS status logging
     * For more info, see: 
     *            $NDDSHOME/doc/html/api_cpp/group__NDDSConfigModule.html
     */
    if (arg.verbose > 1) {
        NDDSConfigLogger::get_instance()->set_verbosity_by_category(
                        NDDS_CONFIG_LOG_CATEGORY_API,
                        NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    }

    /* Finally, starts the application */
    startApplication(arg);

    // Application terminates if sampleCount is set.
    std::cout << "Done." << std::endl;
    return EXIT_SUCCESS;
}
