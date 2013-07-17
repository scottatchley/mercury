/*
 * Copyright (C) 2013 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#include "na.h"
#include "mercury_error.h"
#include "mercury_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char *ion_name;
    na_class_t *network_class = NULL;
    na_addr_t ion_target = 0;

    na_tag_t send_tag = 100;
    na_tag_t recv_tag = 101;

    na_request_t send_request = NA_REQUEST_NULL;
    na_request_t recv_request = NA_REQUEST_NULL;

    char *send_buf = NULL;
    char *recv_buf = NULL;

    na_size_t send_buf_len;
    na_size_t recv_buf_len;

    int *bulk_buf = NULL;
    int bulk_size = 1024*1024;
    na_mem_handle_t local_mem_handle = NA_MEM_HANDLE_NULL;

    na_tag_t bulk_tag = 102;
    na_tag_t ack_tag = 103;

    na_request_t bulk_request = NA_REQUEST_NULL;
    na_request_t ack_request = NA_REQUEST_NULL;

    int i;
    int na_ret;

    /* Initialize the interface */
    network_class = HG_Test_client_init(argc, argv, NULL);
    ion_name = getenv(HG_PORT_NAME);
    if (!ion_name) {
        fprintf(stderr, "getenv(\"%s\") failed\n", HG_PORT_NAME);
    }

    /* Perform an address lookup on the ION */
    na_ret = NA_Addr_lookup(network_class, ion_name, &ion_target);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not find addr %s\n", ion_name);
        return EXIT_FAILURE;
    }

    /* Allocate send and recv bufs */
    send_buf_len = NA_Msg_get_max_unexpected_size(network_class);
    recv_buf_len = send_buf_len;
    send_buf = malloc(send_buf_len);
    recv_buf = malloc(recv_buf_len);


    sprintf(send_buf, "test\n");
    puts("NA_Msg_send()");
    na_ret = NA_Msg_send(network_class, send_buf, send_buf_len, ion_target, send_tag, &send_request, NULL);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not send expected message\n");
        return EXIT_FAILURE;
    }

    na_ret = NA_Wait(network_class, recv_request, NA_MAX_IDLE_TIME, NA_STATUS_IGNORE);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Error during wait\n");
        return EXIT_FAILURE;
    }

    return 0;

    /* Send a message to addr */
    sprintf(send_buf, "Hello ION!\n");
    na_ret = NA_Msg_send_unexpected(network_class, send_buf, send_buf_len, ion_target, send_tag, &send_request, NULL);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not send unexpected message\n");
        return EXIT_FAILURE;
    }
    na_ret = NA_Msg_recv(network_class, recv_buf, recv_buf_len, ion_target, recv_tag, &recv_request, NULL);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not recv message\n");
        return EXIT_FAILURE;
    }

    na_ret = NA_Wait(network_class, send_request, NA_MAX_IDLE_TIME, NA_STATUS_IGNORE);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Error during wait\n");
        return EXIT_FAILURE;
    }
    na_ret = NA_Wait(network_class, recv_request, NA_MAX_IDLE_TIME, NA_STATUS_IGNORE);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Error during wait\n");
        return EXIT_FAILURE;
    }
    printf("Received from ION: %s\n", recv_buf);

    /* Prepare bulk_buf */
    bulk_buf = malloc(sizeof(int) * bulk_size);
    for (i = 0; i < bulk_size; i++) {
        bulk_buf[i] = i;
    }

    /* Register memory */
    printf("Registering local memory...\n");
    na_ret = NA_Mem_register(network_class, bulk_buf, sizeof(int) * bulk_size, NA_MEM_READ_ONLY, &local_mem_handle);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not register memory\n");
        return EXIT_FAILURE;
    }

    /* Serialize mem handle */
    printf("Serializing local memory handle...\n");
    na_ret = NA_Mem_handle_serialize(network_class, send_buf, send_buf_len, local_mem_handle);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not serialize memory handle\n");
        return EXIT_FAILURE;
    }

    /* Send mem handle */
    printf("Sending local memory handle...\n");
    na_ret = NA_Msg_send(network_class, send_buf, send_buf_len, ion_target, bulk_tag, &bulk_request, NULL);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not send memory handle\n");
        return EXIT_FAILURE;
    }

    /* Recv completion ack */
    printf("Receiving end of transfer ack...\n");
    na_ret = NA_Msg_recv(network_class, recv_buf, recv_buf_len, ion_target, ack_tag, &ack_request, NULL);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not receive acknowledgment\n");
        return EXIT_FAILURE;
    }

    na_ret = NA_Wait(network_class, bulk_request, NA_MAX_IDLE_TIME, NA_STATUS_IGNORE);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Error during wait\n");
        return EXIT_FAILURE;
    }
    na_ret = NA_Wait(network_class, ack_request, NA_MAX_IDLE_TIME, NA_STATUS_IGNORE);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Error during wait\n");
        return EXIT_FAILURE;
    }

    printf("Finalizing...\n");

    /* Free memory and addresses */
    na_ret = NA_Mem_deregister(network_class, local_mem_handle);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not unregister memory\n");
        return EXIT_FAILURE;
    }
    free(bulk_buf);
    bulk_buf = NULL;

    na_ret = NA_Addr_free(network_class, ion_target);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not free addr\n");
        return EXIT_FAILURE;
    }
    ion_target = NULL;

    free(recv_buf);
    recv_buf = NULL;

    free(send_buf);
    send_buf = NULL;

    na_ret = NA_Finalize(network_class);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not finalize interface\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
