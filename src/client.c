#include "include/clientargs.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int
main(const int argc, char **argv) {
    struct client_request_args  args;
    struct sockaddr_in          sin4;
    struct sockaddr_in6         sin6;
    enum ip_version             ip_version;

    parse_args(argc, argv, &args, &sin4, &sin6, &ip_version);

    snprintf(args.token, TOKEN_SIZE, "%s", "mybeautifultoke");

    printf("- token: %s\n", args.token);
    printf("- method: %d\n", args.method);
    printf("- target: %d\n", args.target.config_target);
    printf("- dlen: %d\n", args.dlen);
    printf("- port: %hu\n", sin6.sin6_port);
    printf("- ipv6: %s\n", sin6.sin6_family == AF_INET6 ? "yes" : "no");

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

    if(send(sock_fd, &args, sizeof(args) - sizeof(args.data) + args.dlen, 0) < 0){
        perror("client socket send");
        return 1;
    }

    if(close(sock_fd) < 0){
        perror("client socket close");
        return 1;
    }

    return 0;
}
