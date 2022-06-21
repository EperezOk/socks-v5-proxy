#include "include/client.h"

#include <string.h>
#include <sys/socket.h>

#define BASE_RESPONSE_DATA      3


int
main(const int argc, char **argv) {
    struct client_request_args  args[MAX_CLIENT_REQUESTS] = {0};
    struct sockaddr_in          sin4;
    struct sockaddr_in6         sin6;
    enum ip_version             ip_version;
    char                        token[TOKEN_SIZE];

    size_t arg_amount = parse_args(argc, argv, args, token, &sin4, &sin6, &ip_version);

    char writeBuffer[BASE_REQUEST_DATA + MAX_BYTES_DATA];
    uint8_t buf[BASE_RESPONSE_DATA + MAX_BYTES_DATA];

    static uint8_t combinedlen[2] = {0};
    static uint8_t numeric_data_array[4] = {0};
    static uint32_t numeric_response;

    // socket -> connect -> send -> recv -> close

    int sock_fd;

    for(size_t i=0 ; i < arg_amount ; i++){
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
    
        serialize_request(&args[i], token, writeBuffer);
                                // version 1 + token 2 + method 1 + target 1 + dlen 2 + data length
        if(send(sock_fd, &writeBuffer, BASE_REQUEST_DATA + args[i].dlen, 0) < 0){
            perror("client socket send");
            return 1;
        }
        
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
                if (args[i].method == get) 
                    handle_get_ok_status(args[i], buf, combinedlen, numeric_data_array, &numeric_response); 
                else
                    handle_config_ok_status(args[i]);
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

        // TODO: Check if necessary to reset all of these
        memset(writeBuffer, 0, BASE_REQUEST_DATA + MAX_BYTES_DATA);
        memset(buf, 0, BASE_RESPONSE_DATA + MAX_BYTES_DATA);
        memset(combinedlen, 0, 2);
        memset(numeric_data_array, 0, 4);
    }

    return 0;
}

void handle_get_ok_status(struct client_request_args arg, uint8_t *buf, uint8_t *combinedlen, uint8_t *numeric_data_array, uint32_t *numeric_response) {
    combinedlen[0] = buf[1];
    combinedlen[1] = buf[2]; 
    uint16_t dlen = ntohs(*(uint16_t*)combinedlen); // obtengo el dlen
    switch (arg.target.get_target) {
        case historic_connections:      // recibe uint32 (4 bytes)
        case concurrent_connections:    // recibe uint32 (4 bytes)
        case transferred_bytes:         // recibe uint32 (4 bytes)
            for (int k = 0, j = 3; k < 4; k++) {
                numeric_data_array[k] = buf[j++];
            }
            *numeric_response = ntohl(*(uint32_t*)numeric_data_array);
            if(arg.target.get_target == historic_connections) {
                printf("The amount of historic connections is: %u\n", *numeric_response);
            } else {
                printf("The amount of %s is: %u\n",  arg.target.get_target == concurrent_connections ? "concurrent connections" : "transferred bytes", *numeric_response);
            }
            break;
        case proxy_users_list:
        case admin_users_list: 
            printf("Printing %s user list:  \n", arg.target.get_target == proxy_users_list ? "proxy" : "admin");
            for (uint16_t k = 3; k < dlen + 3; k++) {
                if (buf[k] == 0) {
                    putchar('\n');
                } else {
                    putchar(buf[k]);
                }
            }
            putchar('\n'); // el ultimo nombre de la lista no tiene \0
            break;
    default:
        break;
    }
}

void handle_config_ok_status(struct client_request_args arg) {
    switch (arg.target.config_target) {
        case toggle_disector:
            printf("The pop3 password disector is now: %s\n", arg.data.disector_data_params == disector_off ? "OFF" : "ON");
            break;
        case add_proxy_user:
            printf("The proxy user: '%s' is now added to the server\n", arg.data.add_proxy_user_params.user);
            break;
        case del_proxy_user:
            printf("The proxy user: '%s' is now deleted in the server\n", arg.data.add_proxy_user_params.user);
            break;
        case add_admin_user:
            printf("The admin: '%s' is now added in the server\n", arg.data.add_proxy_user_params.user);
            break;
        case del_admin_user:
            printf("The admin: '%s' is now deleted in the server\n", arg.data.add_proxy_user_params.user);
            break;
    }      
}