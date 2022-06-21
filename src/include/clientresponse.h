#include <string.h>
#include <stdio.h>
#include "../include/clientrequest.h"
#include "../include/clientargs.h"


//Valores posibles del campo status en la response del protocolo
enum monitor_resp_status {
    monitor_resp_status_ok              = 0x00,
    monitor_resp_status_invalid_version = 0x01,
    monitor_resp_status_invalid_method  = 0x02,
    monitor_resp_status_invalid_target  = 0x03,
    monitor_resp_status_invalid_data    = 0x04,
    monitor_resp_status_error_auth      = 0x05,
    monitor_resp_status_server_error    = 0x06,
};

void handle_get_ok_status(struct client_request_args arg, uint8_t *buf, uint8_t *combinedlen, uint8_t *numeric_data_array, uint32_t *numeric_response);

void handle_config_ok_status(struct client_request_args arg);

void process_response (uint8_t c, struct client_request_args *args, uint8_t *buf, uint8_t *combinedlen, uint8_t *numeric_data_array, uint32_t *numeric_response);
