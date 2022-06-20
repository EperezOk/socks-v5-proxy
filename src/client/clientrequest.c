#include <string.h>
#include "../include/clientargs.h"
#include "../include/clientrequest.h"

static void
serialize_config_data(struct client_request_args *args, char* buffer);

void
serialize_request(struct client_request_args *args, char *buffer){
    buffer[FIELD_VERSION_INDEX] = PROGRAM_VERSION;
    memcpy(FIELD_TOKEN(buffer), args->token, TOKEN_SIZE);
    
    buffer[FIELD_METHOD_INDEX] = args->method;

    // Sending in network order
    uint16_t dlen = htons(args->dlen);
    memcpy(FIELD_DLEN(buffer), &dlen, sizeof(uint16_t));

    switch(args->method){
        case get:
            memcpy(FIELD_TARGET(buffer), &args->target.get_target, sizeof(uint8_t));
            memcpy(FIELD_DATA(buffer), &args->data.optional_data, sizeof(uint8_t)); // data = 0 in get case
            break;
        case config:
            memcpy(FIELD_TARGET(buffer), &args->target.get_target, sizeof(uint8_t));
            serialize_config_data(args, buffer);
            break;
        default:
            // should not get here
            break;
    }
}

static void
serialize_config_data(struct client_request_args *args, char *buffer){
    uint8_t disector_value;
    size_t username_len;
    size_t extra_param_len;
    
    switch(args->target.config_target){
        case toggle_disector:
            disector_value = args->data.disector_data_params;
            memcpy(FIELD_DATA(buffer), &disector_value, sizeof(uint8_t));
            
            break;
        case add_proxy_user:
            username_len = strlen(args->data.add_proxy_user_params.user);
            memcpy(FIELD_DATA(buffer), args->data.add_proxy_user_params.user, username_len);
            
            buffer[FIELD_DATA_INDEX + username_len] = args->data.add_proxy_user_params.separator;
            
            extra_param_len = strlen(args->data.add_proxy_user_params.pass);
            memcpy(FIELD_DATA(buffer) + username_len + 1, args->data.add_proxy_user_params.pass, extra_param_len);

            break;
        case add_admin_user:
            username_len = strlen(args->data.add_admin_user_params.user);
            memcpy(FIELD_DATA(buffer), args->data.add_admin_user_params.user, username_len);
            
            buffer[FIELD_DATA_INDEX + username_len] = args->data.add_admin_user_params.separator;
            
            extra_param_len = strlen(args->data.add_admin_user_params.token);
            memcpy(FIELD_DATA(buffer) + username_len + 1, args->data.add_admin_user_params.token, extra_param_len);

            break;
        case del_proxy_user:
        case del_admin_user:
            memcpy(FIELD_DATA(buffer), args->data.user, args->dlen);
            break;
    }
}