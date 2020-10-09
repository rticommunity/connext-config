/****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.       */
/*                                                                          */
/*         Permission to modify and use for internal purposes granted.      */
/* This software is provided "as is", without warranty, express or implied. */
/*                                                                          */
/****************************************************************************/

/*
 * Include the auto-generated support for the request and reply types
 */
#include "Primes.h"
#include "PrimesSupport.h"

#include "connext_c/connext_c_requester.h"

/*
 * Instantiate the requester structure and declare its functions
 */
RTI_CONNEXT_REQUESTER_DECL(PrimeNumberRequest, PrimeNumberReply, PrimeNumberRequester)

/*
 * Define TReq, the request type and TRep, the reply type and
 * Instantiate the implementation of the requester functions.
 *
 * Note: for simplicity we include the implementation here, but it could
 *       be compiled in its own .c file.
 */
#define TReq PrimeNumberRequest
#define TRep PrimeNumberReply
#define TRequester PrimeNumberRequester

#include "connext_c/generic/connext_c_requestreply_TReqTRepRequester.gen"

static int requester_shutdown(
    DDS_DomainParticipant * participant,
    PrimeNumberRequester * requester,
    PrimeNumberRequest * request)
{
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (request != NULL) {
        PrimeNumberRequestTypeSupport_delete_data(request);
    }

    if (requester != NULL) {
        retcode = RTI_Connext_Requester_delete(
            (RTI_Connext_Requester *) requester);
        if (retcode != DDS_RETCODE_OK) {
            fprintf(stderr, "delete requester error %d\n", retcode);
            status = -1;
        }
    }

    if (participant != NULL) {
        retcode = DDS_DomainParticipant_delete_contained_entities(participant);
        if (retcode != DDS_RETCODE_OK) {
            fprintf(stderr, "delete_contained_entities error %d\n", retcode);
            status = -1;
        }

        retcode = DDS_DomainParticipantFactory_delete_participant(
            DDS_TheParticipantFactory, participant);
        if (retcode != DDS_RETCODE_OK) {
            fprintf(stderr, "delete_participant error %d\n", retcode);
            status = -1;
        }
    }

    retcode = DDS_DomainParticipantFactory_finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        printf("ParticipantFactory finalize_instance error %d\n", retcode);
        status = -1;
    }

    return status;
}

int requester_main(int n, int primes_per_reply, int domain_id)
{
    int in_progress;
    int i, j;
    DDS_DomainParticipant *participant = NULL;
    DDS_ReturnCode_t retcode;
    struct RTI_Connext_RequesterParams requester_params =
        RTI_Connext_RequesterParams_INITIALIZER;
    PrimeNumberRequester *requester = NULL;
    PrimeNumberRequest *request = NULL;
    const struct DDS_Duration_t MAX_WAIT = {20, 0};
    struct PrimeNumberReplySeq replies = DDS_SEQUENCE_INITIALIZER;
    struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;

    /* Create the participant */
    participant = DDS_DomainParticipantFactory_create_participant(
        DDS_TheParticipantFactory, domain_id, &DDS_PARTICIPANT_QOS_DEFAULT,
        NULL /* listener */, DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        fprintf(stderr, "create_participant error\n");
        return -1;
    }

    /* Create the requester with that participant, and a QoS profile
     * defined in USER_QOS_PROFILES.xml
     */
    requester_params.participant = participant;
    requester_params.service_name = (char *) "PrimeCalculator";
    requester_params.qos_library_name = (char *) "RequestReplyExampleProfiles";
    requester_params.qos_profile_name = (char *) "RequesterExampleProfile";

    requester = PrimeNumberRequester_create_w_params(&requester_params);
    if (requester == NULL) {
        fprintf(stderr, "create requester error\n");
        requester_shutdown(participant, requester, request);
        return -1;
    }

    request = PrimeNumberRequestTypeSupport_create_data();
    if (request == NULL) {
        fprintf(stderr, "Create data error\n");
        requester_shutdown(participant, requester, request);
        return -1;
    }

    request->n = n;
    request->primes_per_reply = primes_per_reply;

    /* Send the request */
    retcode = PrimeNumberRequester_send_request(requester, request);

    if (retcode != DDS_RETCODE_OK) {
        fprintf(stderr, "send_request error %d\n", retcode);
        requester_shutdown(participant, requester, request);
        return -1;
    }

    /* Receive replies */
    retcode = PrimeNumberRequester_receive_replies(
        requester, &replies, &info_seq,
        1, /* Wait for at least one reply */
        DDS_LENGTH_UNLIMITED, &MAX_WAIT);

    in_progress = 1;
    while (retcode == DDS_RETCODE_OK && in_progress) {

        for (i = 0; i < PrimeNumberReplySeq_get_length(&replies); i++) {
            struct PrimeNumberReply * reply =
                PrimeNumberReplySeq_get_reference(&replies, i);
            struct DDS_SampleInfo * info =
                DDS_SampleInfoSeq_get_reference(&info_seq, i);

            if (info->valid_data) {

                for (j = 0; j < DDS_LongSeq_get_length(&reply->primes); j++) {
                    DDS_Long prime_number =
                        *DDS_LongSeq_get_reference(&reply->primes, j);

                    printf("%d ", prime_number);
                }

                if (reply->status != REPLY_IN_PROGRESS) {
                    in_progress = 0;
                    if (reply->status == REPLY_ERROR) {
                        fprintf(stderr, "Error in Replier\n");
                    } else { /* reply->status == REPLY_COMPLETED */
                        printf("DONE");
                        fflush(stdout);
                    }
                }

                printf("\n");
            }
        }

        /*
         * Return the loan to the middleware
         */
        PrimeNumberRequester_return_loan(requester, &replies, &info_seq);

        if (in_progress) {
            retcode = PrimeNumberRequester_receive_replies(
                requester, &replies, &info_seq,
                1, DDS_LENGTH_UNLIMITED, &MAX_WAIT);
        }
    }

    if (retcode != DDS_RETCODE_OK) {
        if (retcode == DDS_RETCODE_TIMEOUT) {
            fprintf(stderr, "Timed out waiting for prime numbers\n");
            return -1;
        } else {
            fprintf(stderr, "Error receiving replies %d\n", retcode);
            return -1;
        }
    }

    return requester_shutdown(participant, requester, request);
}

#if !(defined(RTI_VXWORKS) && !defined(__RTP__)) && !defined(RTI_PSOS)
int main(int argc, char *argv[])
{
    int domain_id = 0;
    int n;
    int primes_per_reply = 5;

    if (argc < 2) {
        printf("PrimeNumberRequester:\n");
        printf("Sends a request to calculate the prime numbers <= n\n");
        printf("Parameters: <n> [<primes_per_reply> = 5] [<domain_id> = 0]\n");
        return -1;
    }

    n = atoi(argv[1]);

    if (argc > 2) {
        primes_per_reply = atoi(argv[2]);
    }

    if (argc > 3) {
        domain_id = atoi(argv[3]);
    }

    /* Uncomment this to turn on additional logging
    NDDS_Config_Logger_set_verbosity(
                    NDDS_Config_Logger_get_instance(),
                    NDDS_CONFIG_LOG_VERBOSITY_WARNING);
    */

    printf("PrimeNumberRequester: Sending a request to calculate the ");
    printf("prime numbers <=  %d in sequences of %d or less elements ",
           n, primes_per_reply);
    printf("(on domain %d)\n", domain_id);
    fflush(stdout);

    return requester_main(n, primes_per_reply, domain_id);
}
#endif
