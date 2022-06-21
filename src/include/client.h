#ifndef CLIENT_H
#define CLIENT_H

#define MAX_BYTES_DATA          65535
#define MAX_CLIENT_REQUESTS     20

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "clientargs.h"
#include "clientrequest.h"
#include "clientresponse.h"

void handle_get_ok_status(struct client_request_args arg, uint8_t *buf, uint8_t *combinedlen, uint8_t *numeric_data_array, uint32_t *numeric_response);

void handle_config_ok_status(struct client_request_args arg);

#endif
