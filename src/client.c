#include "include/clientargs.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

int
main(const int argc, char **argv) {
    struct client_request_args args;

    parse_args(argc, argv, &args);

    sprintf(args.token, "%s", "mybeautifulboke");

    printf("- token: %s\n", args.token);
    printf("- method: %d\n", args.method);
    printf("- target: %d\n", args.target.config_target);
    printf("- dlen: %d\n", args.dlen);

    // socket -> connect -> send -> recv -> close

    int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8080);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock_fd, (struct sockaddr *)&sin, sizeof(sin));

    send(sock_fd, "hola mundo\n", 11, 0);

    close(sock_fd);

    return 0;
}
