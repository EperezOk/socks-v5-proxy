#include "include/clientargs.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static void
serialize_request(struct client_request_args *args, struct client_serialized_request *request);

static void
serialize_config_data(struct client_request_args *args, struct client_serialized_request *request);

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

    struct client_serialized_request request;
    serialize_request(&args, &request);

    if(send(sock_fd, &request, sizeof(request) - DATA_SIZE + request.dlen, 0) < 0){
        perror("client socket send");
        return 1;
    }

    // char buf[2000];

    // if(recv(sock_fd, buf, ) < 0){
    //     perror("client socket recv");
    //     return 1;
    // }

    // parsing -> mostrar respuesta

    if(close(sock_fd) < 0){
        perror("client socket close");
        return 1;
    }

    return 0;
}

static void
serialize_request(struct client_request_args *args, struct client_serialized_request *request){
    // Fields without unions
    request->version = 1;
    memcpy(request->token, args->token, TOKEN_SIZE);
    request->method = args->method;
    // request.dlen = htons(args.dlen);
    request->dlen = args->dlen;

    // Fields with unions
    switch(request->method){
        case get:
            request->target = args->target.get_target;
            memcpy(request->data, &args->data.optional_data, sizeof(uint8_t));
            break;
        case config:
            request->target = args->target.config_target;
            serialize_config_data(args, request);
            break;
        default:
            // no deberia llegar aqui
            break;
    }
}

static void
serialize_config_data(struct client_request_args *args, struct client_serialized_request *request){
    uint8_t disector_value;
    size_t username_len;
    size_t extra_param_len;
    
    switch(request->target){
        case toggle_disector:
            disector_value = (uint8_t)args->data.disector_data_params;
            memcpy(request->data, &disector_value, sizeof(disector_value));
            break;
        case add_proxy_user:
            username_len = strlen(args->data.add_proxy_user_params.user);
            memcpy(request->data, args->data.add_proxy_user_params.user, username_len);
            
            request->data[username_len] = args->data.add_proxy_user_params.separator;
            
            extra_param_len = strlen(args->data.add_proxy_user_params.pass);
            memcpy(request->data + username_len + 1, args->data.add_proxy_user_params.pass, extra_param_len);

            break;
        case add_admin_user:
            username_len = strlen(args->data.add_admin_user_params.user);
            memcpy(request->data, args->data.add_admin_user_params.user, username_len);
            
            request->data[username_len] = args->data.add_admin_user_params.separator;
            
            extra_param_len = strlen(args->data.add_admin_user_params.token);
            memcpy(request->data + username_len + 1, args->data.add_admin_user_params.token, extra_param_len);

            break;
        case del_proxy_user:
        case del_admin_user:
            memcpy(request->data, args->data.user, request->dlen);
            break;
    }
}