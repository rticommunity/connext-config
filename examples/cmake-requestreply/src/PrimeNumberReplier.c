/****************************************************************************/
/*         (c) Copyright, Real-Time Innovations, All rights reserved.       */
/*                                                                          */
/*         Permission to modify and use for internal purposes granted.      */
/* This software is provided "as is", without warranty, express or implied. */
/*                                                                          */
/****************************************************************************/

#include <math.h>

/*
 * Include the auto-generated support for the request and reply types
 */
#include "Primes.h"
#include "PrimesSupport.h"

#include "connext_c/connext_c_replier.h"

/*
 * Instantiate the replier structure and declare its functions
 *
 */
RTI_CONNEXT_REPLIER_DECL(PrimeNumberRequest, PrimeNumberReply, PrimeNumberReplier)

/*
 * Define TReq, the request type and TRep, the reply type and
 * Instantiate the implementation of the replier functions.
 *
 * Note: for simplicity we include the implementation here, but it could
 *       be compiled in its own .c file.
 */
#define TReq PrimeNumberRequest
#define TRep PrimeNumberReply
#define TReplier PrimeNumberReplier

#include "connext_c/generic/connext_c_requestreply_TReqTRepReplier.gen"

static int replier_shutdown(
    DDS_DomainParticipant * participant,
    PrimeNumberReplier * replier,
    PrimeNumberRequest * request)
{
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (request != NULL) {
        PrimeNumberRequestTypeSupport_delete_data(request);
    }

    if (replier != NULL) {
        retcode = RTI_Connext_Replier_delete(
            (RTI_Connext_Replier *) replier);
        if (retcode != DDS_RETCODE_OK) {
            fprintf(stderr, "delete replier error %d\n", retcode);
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

void send_error_reply(
    PrimeNumberReplier * replier,
    const PrimeNumberRequest * request,
    const struct DDS_SampleIdentity_t * request_id)
{
    DDS_ReturnCode_t retcode;
    PrimeNumberReply * reply;

    reply = PrimeNumberReplyTypeSupport_create_data();
    if (reply == NULL) {
        fprintf(stderr, "Create reply data error\n");
    }

    reply->status = REPLY_ERROR;

    retcode = PrimeNumberReplier_send_reply(replier, reply, request_id);
    if (retcode != DDS_RETCODE_OK) {
        fprintf(stderr, "send_reply error %d\n", retcode);
    }

    PrimeNumberReplyTypeSupport_delete_data(reply);
}

int calculate_and_send_primes(
    PrimeNumberReplier * replier,
    const PrimeNumberRequest * request,
    const struct DDS_SampleIdentity_t * request_id)
{
    DDS_ReturnCode_t retcode;
    int i, m, k, length;
    int n = request->n;
    int primes_per_reply = request->primes_per_reply;
    int * prime = NULL;
    PrimeNumberReply * reply;

    reply = PrimeNumberReplyTypeSupport_create_data();
    if (reply == NULL) {
        fprintf(stderr, "Create reply data error\n");
        send_error_reply(replier, request, request_id);
        return -1;
    }

    DDS_LongSeq_set_maximum(&reply->primes, primes_per_reply);

    reply->status = REPLY_IN_PROGRESS;

    /* prime[i] indicates if i is a prime number */
    prime = (int *) malloc((n+1) * sizeof(int));
    if (prime == 0) {
        fprintf(stderr, "Bad allocation\n");
        send_error_reply(replier, request, request_id);
        return -1;
    }

    for (i = 0; i < n; i++) {
        prime[i] = 1;
    }

    prime[0] = 0;
    prime[1] = 0;

    m = (int) sqrt(n);

    for (i = 2; i <= m; i++) {
        if (prime[i]) {
            for (k = i*i; k <= n; k+=i) {
                prime[k] = 0;
            }

            /* Add a new element */
            length = DDS_LongSeq_get_length(&reply->primes);
            DDS_LongSeq_set_length(&reply->primes,  length + 1);
            *DDS_LongSeq_get_reference(&reply->primes, length) = i;

            if (length + 1 == primes_per_reply) {

                /* Send a reply now */
                retcode = PrimeNumberReplier_send_reply(
                    replier, reply, request_id);

                if (retcode != DDS_RETCODE_OK) {
                    fprintf(stderr, "send_reply error %d\n", retcode);
                    PrimeNumberReplyTypeSupport_delete_data(reply);
                    return -1;
                }

                DDS_LongSeq_set_length(&reply->primes, 0);
            }
        }
    }

    /* Calculation is done. Send remaining prime numbers */
    for (i = m + 1; i <= n; i++) {
        if (prime[i]) {

            length = DDS_LongSeq_get_length(&reply->primes);
            DDS_LongSeq_set_length(&reply->primes,  length + 1);
            *DDS_LongSeq_get_reference(&reply->primes, length) = i;

            if (length + 1 == primes_per_reply) {

                /* Send a reply now */
                retcode = PrimeNumberReplier_send_reply(
                    replier, reply, request_id);

                if (retcode != DDS_RETCODE_OK) {
                    fprintf(stderr, "send_reply error %d\n", retcode);
                    PrimeNumberReplyTypeSupport_delete_data(reply);
                    return -1;
                }

                DDS_LongSeq_set_length(&reply->primes, 0);
            }
        }
    }

    /* Send the last reply. Indicate that the calculation is complete and
     * send any prime number left in the sequence
     */
    reply->status = REPLY_COMPLETED;
    retcode = PrimeNumberReplier_send_reply(
        replier, reply, request_id);

    if (retcode != DDS_RETCODE_OK) {
        fprintf(stderr, "send_reply error %d\n", retcode);
        PrimeNumberReplyTypeSupport_delete_data(reply);
        return -1;
    }

    free(prime);
    PrimeNumberReplyTypeSupport_delete_data(reply);
    return 0;
}


int replier_main(int domain_id)
{
    int result;
    DDS_DomainParticipant *participant = NULL;
    DDS_ReturnCode_t retcode;
    struct RTI_Connext_ReplierParams replier_params =
        RTI_Connext_ReplierParams_INITIALIZER;
    PrimeNumberReplier * replier = NULL;
    PrimeNumberRequest * request = NULL;
    const struct DDS_Duration_t MAX_WAIT = {60, 0};
    struct DDS_SampleInfo request_info;
    struct DDS_SampleIdentity_t request_id;

    /* Create the participant */
    participant = DDS_DomainParticipantFactory_create_participant(
        DDS_TheParticipantFactory, domain_id, &DDS_PARTICIPANT_QOS_DEFAULT,
        NULL /* listener */, DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        fprintf(stderr, "create_participant error\n");
        return -1;
    }

    /* Create the replier with that participant, and a QoS profile
     * defined in USER_QOS_PROFILES.xml
     */
    replier_params.participant = participant;
    replier_params.service_name = (char *) "PrimeCalculator";
    replier_params.qos_library_name = (char *) "RequestReplyExampleProfiles";
    replier_params.qos_profile_name = (char *) "ReplierExampleProfile";

    replier = PrimeNumberReplier_create_w_params(&replier_params);
    if (replier == NULL) {
        fprintf(stderr, "create replier error\n");
        replier_shutdown(participant, replier, request);
        return -1;
    }


    request = PrimeNumberRequestTypeSupport_create_data();
    if (request == NULL) {
        fprintf(stderr, "Create request data error\n");
        replier_shutdown(participant, replier, request);
        return -1;
    }

    /*
     * Receive requests and process them
     */
    retcode = PrimeNumberReplier_receive_request(
            replier, request, &request_info, &MAX_WAIT);

    while(retcode == DDS_RETCODE_OK) {

        if (request_info.valid_data) {
            DDS_SampleInfo_get_sample_identity(&request_info, &request_id);

            /* This constant is defined in Primes.idl */
            if (request->n <= 0 ||
                request->primes_per_reply <= 0 ||
                request->primes_per_reply > PRIME_SEQUENCE_MAX_LENGTH) {

                fprintf(stderr, "Cannot process request\n");
                send_error_reply(replier, request, &request_id);
            } else {

                printf("Calculating prime numbers below %d... ", request->n);
                fflush(stdout);

                /* This operation could be executed in a separate thread,
                 * to process requests in parallel
                 */
                calculate_and_send_primes(replier, request, &request_id);

                printf("DONE\n");
                fflush(stdout);
            }
        }

        retcode = PrimeNumberReplier_receive_request(
                replier, request, &request_info, &MAX_WAIT);
    }

    if (retcode == DDS_RETCODE_TIMEOUT) {
        printf("No request received for %d seconds. Shutting down replier\n",
            MAX_WAIT.sec);
        result = 0;
    } else {
        fprintf(stderr, "Error in replier %d\n", retcode);
        result = -1;
    }

    replier_shutdown(participant, replier, request);

    return result;
}

#if !(defined(RTI_VXWORKS) && !defined(__RTP__)) && !defined(RTI_PSOS)
int main(int argc, char *argv[])
{
    int domain_id = 0;

    if (argc > 1) {
        domain_id = atoi(argv[1]);
    }


    /* Uncomment this to turn on additional logging
    NDDS_Config_Logger_set_verbosity(
                    NDDS_Config_Logger_get_instance(),
                    NDDS_CONFIG_LOG_VERBOSITY_WARNING);
    */

    printf("PrimeNumberReplier running (on domain %d)\n", domain_id);
    fflush(stdout);
    return replier_main(domain_id);
}
#endif
