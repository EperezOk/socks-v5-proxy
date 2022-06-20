#include "include/clientargs.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static void
serialize_request(struct client_request_args *args, char *buffer);

static void
serialize_config_data(struct client_request_args *args, char* buffer);

#define BASE_RESPONSE_DATA      3
#define MAX_BYTES_DATA          65535
#define BASE_REQUEST_DATA       21

#define PROGRAM_VERSION 1

// Serialized buffer fields
#define FIELD_VERSION   0
#define FIELD_TOKEN     1
#define FIELD_METHOD    17
#define FIELD_TARGET    18
#define FIELD_DLEN      19
#define FIELD_DATA      21

int
main(const int argc, char **argv) {
    struct client_request_args  args;
    struct sockaddr_in          sin4;
    struct sockaddr_in6         sin6;
    enum ip_version             ip_version;

    parse_args(argc, argv, &args, &sin4, &sin6, &ip_version);

    // socket -> connect -> send -> recv -> close

    int sock_fd;

    if(ip_version == ipv4){
        if((sock_fd = socket(sin4.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0){
            perror("client socket ipv4 creation");
            return 1;
        }
    
        if(connect(sock_fd, (struct sockaddr *)&sin4, sizeof(sin4)) < 0){
            perror("client socket ipv4 connect");
            return 1;
        }
    } else {
        if((sock_fd = socket(sin6.sin6_family, SOCK_STREAM, IPPROTO_TCP)) < 0){
            perror("client socket ipv6 creation");
            return 1;
        }
    
        if(connect(sock_fd, (struct sockaddr *)&sin6, sizeof(sin6)) < 0){
            perror("client socket ipv6 connect");
            return 1;
        }
    }

    char writeBuffer[BASE_REQUEST_DATA + MAX_BYTES_DATA];
    serialize_request(&args, writeBuffer);
                            // version 1 + token 2 + method 1 + target 1 + dlen 2 + data length
    if(send(sock_fd, &writeBuffer, BASE_REQUEST_DATA + args.dlen, 0) < 0){
        perror("client socket send");
        return 1;
    }

    uint8_t buf[BASE_RESPONSE_DATA + MAX_BYTES_DATA];

    static uint8_t combinedlen[2] = {0};
    static uint8_t numeric_data_array[4] = {0};
    static uint16_t dlen;
    static uint32_t numeric_response;
    
    long n = -1; 
    while ((n = recv(sock_fd, buf, BASE_RESPONSE_DATA + MAX_BYTES_DATA, 0)) != 0) {
        if (n < 0) {
            perror("client socket recv");
            abort();
        }
    }


    // termine de recibir

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

    switch (buf[0]) {
        case monitor_resp_status_ok:
            if (args.method == get) {
                combinedlen[0] =  buf[1];
                combinedlen[1] =  buf[2]; 
                dlen = ntohs(*(uint16_t*)combinedlen); // obtengo el dlen
                switch (args.target.get_target) {
                    case historic_connections:      // recibe uint32 (4 bytes)
                    case concurrent_connections:    // recibe uint32 (4 bytes)
                    case transferred_bytes:         // recibe uint32 (4 bytes)
                        for (int i = 0, j = 3; i < 4; i++) {
                            numeric_data_array[i] = buf[j++];
                        }
                        numeric_response = ntohl(*(uint32_t*)numeric_data_array);
                        if(args.target.get_target == historic_connections) {
                            printf("The amount of historic connections is: %d\n", numeric_response);
                        } else {
                            printf("The amount of %s is: %d\n",  args.target.get_target == concurrent_connections ? "concurrent connections" : "transferred bytes", numeric_response);
                        }
                        break;
                    case proxy_users_list:
                    case admin_users_list: 
                        printf("Printing %s user list:  \n", args.target.get_target == proxy_users_list ? "proxy" : "admin");
                        for (uint16_t i = 3; i < dlen + 3; i++) {
                            if (buf[i] == 0) {
                                putchar('\n');
                            } else {
                                putchar(buf[i]);
                            }
                        }
                        putchar('\n'); // el ultimo nombre de la lista no tiene \0
                        break;
                default:
                    break;
                }
            }
            else {
                switch (args.target.config_target) {
                    case toggle_disector:
                        printf("The pop3 password disector is now: %s\n", args.data.disector_data_params == disector_off ? "OFF" : "ON");
                        break;
                    case add_proxy_user:
                        printf("The proxy user: '%s' is now added to the server\n", args.data.add_proxy_user_params.user);
                        break;
                    case del_proxy_user:
                        printf("The proxy user: '%s' is now deleted in the server\n", args.data.add_proxy_user_params.user);
                        break;
                    case add_admin_user:
                        printf("The admin: '%s' is now added in the server\n", args.data.add_proxy_user_params.user);
                        break;
                    case del_admin_user:
                        printf("The admin: '%s' is now deleted in the server\n", args.data.add_proxy_user_params.user);
                        break;
                    default:
                        break;
                    }
                
            }
            break;
        case monitor_resp_status_invalid_version:
            printf("The version of the request you have sent is incorrect!\n");
            break;
        case monitor_resp_status_invalid_method:
            printf("The method of the request you have sent is incorrect!\n");
            break;
        case monitor_resp_status_invalid_target:
            printf("The target of the request you have sent is incorrect!\n");
            break;
        case monitor_resp_status_invalid_data:
            printf("The data of the request you have sent is incorrect!\n");
            break;
        case monitor_resp_status_error_auth:
            printf("The token of the request you have sent is incorrect!\n");
            break;
        case monitor_resp_status_server_error:
            printf("The server could not resolve your request!\n");
            break;
        default:
            break;
    }

    if(close(sock_fd) < 0){
        perror("client socket close");
        return 1;
    }

    return 0;
}

static void
serialize_request(struct client_request_args *args, char *buffer){
    // Fields without unions
    buffer[FIELD_VERSION] = PROGRAM_VERSION;
    // token 16 characters
    memcpy(buffer + FIELD_TOKEN, args->token, TOKEN_SIZE);
    
    buffer[FIELD_METHOD] = args->method;
    // request->method = args->method;

    // SENDING IN NETWORK ORDER
    uint16_t dlen = htons(args->dlen);
    memcpy(buffer + FIELD_DLEN, &dlen, sizeof(uint16_t));
    // request->dlen = htons(args->dlen);


    // Fields with unions
    switch(args->method){
        case get:
            memcpy(buffer + FIELD_TARGET, &args->target.get_target, sizeof(uint8_t));
            memcpy(buffer + FIELD_DATA, &args->data.optional_data, sizeof(uint8_t)); // data = 0 in get case
            // request->target = args->target.get_target;
            // memcpy(request->data, &args->data.optional_data, sizeof(uint8_t));
            break;
        case config:
            memcpy(buffer + FIELD_TARGET, &args->target.get_target, sizeof(uint8_t));
            serialize_config_data(args, buffer);
            break;
        default:
            // no deberia llegar aqui
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
            memcpy(buffer + FIELD_DATA, &disector_value, sizeof(uint8_t));
            
            break;
        case add_proxy_user:
            username_len = strlen(args->data.add_proxy_user_params.user);
            memcpy(buffer + FIELD_DATA, args->data.add_proxy_user_params.user, username_len);
            
            buffer[FIELD_DATA + username_len] = args->data.add_proxy_user_params.separator;
            
            extra_param_len = strlen(args->data.add_proxy_user_params.pass);
            memcpy(buffer + FIELD_DATA + username_len + 1, args->data.add_proxy_user_params.pass, extra_param_len);

            break;
        case add_admin_user:
            username_len = strlen(args->data.add_admin_user_params.user);
            memcpy(buffer + FIELD_DATA, args->data.add_admin_user_params.user, username_len);
            
            buffer[FIELD_DATA + username_len] = args->data.add_admin_user_params.separator;
            
            extra_param_len = strlen(args->data.add_admin_user_params.token);
            memcpy(buffer + FIELD_DATA + username_len + 1, args->data.add_admin_user_params.token, extra_param_len);

            break;
        case del_proxy_user:
        case del_admin_user:
            memcpy(buffer + FIELD_DATA, args->data.user, args->dlen);
            break;
    }
}
