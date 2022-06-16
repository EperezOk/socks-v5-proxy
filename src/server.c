/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "include/selector.h"
#include "include/socks5nio.h"
#include "include/monitornio.h"
#include "include/args.h"

#define MAX_CONNECTIONS 512

static const int FD_UNUSED = -1;
#define IS_FD_USED(fd) ((FD_UNUSED != fd))

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

static int bind_ipv4_socket(struct in_addr bind_address, unsigned port);
static int bind_ipv6_socket(struct in6_addr bind_address, unsigned port);

int
main(const int argc, char **argv) {
    struct socks5args args;

    parse_args(argc, argv, &args);

    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct in_addr server_ipv4_addr, monitor_ipv4_addr;
    int server_v4 = FD_UNUSED, monitor_v4 = FD_UNUSED;

    struct in6_addr server_ipv6_addr, monitor_ipv6_addr;
    int server_v6 = FD_UNUSED, monitor_v6 = FD_UNUSED;

    // socket pasivo socks IPv4
    if(inet_pton(AF_INET, args.socks_addr, &server_ipv4_addr) == 1){       // if parsing to ipv4 succeded
        server_v4 = bind_ipv4_socket(server_ipv4_addr, args.socks_port);
        if (server_v4 < 0) {
            err_msg = "unable to create IPv4 socks socket";
            goto finally;
        }
        fprintf(stdout, "Listening IPv4 socks on TCP port %d\n", args.socks_port);
    }

    // socket pasivo monitoreo IPv4
    if(inet_pton(AF_INET, args.mng_addr, &monitor_ipv4_addr) == 1) {
        monitor_v4 = bind_ipv4_socket(monitor_ipv4_addr, args.mng_port);
        if (monitor_v4 < 0) {
            err_msg = "unable to create IPv4 monitor socket";
            goto finally;
        }
        fprintf(stdout, "Listening IPv4 monitor on TCP port %d\n", args.mng_port);
    }

     // socket pasivo socks IPv6
    char* ipv6_addr_text = args.is_default_socks_addr ? DEFAULT_SOCKS_ADDR_V6 : args.socks_addr;

    if((!IS_FD_USED(server_v4) || args.is_default_socks_addr) && (inet_pton(AF_INET6, ipv6_addr_text, &server_ipv6_addr) == 1)){
        server_v6 = bind_ipv6_socket(server_ipv6_addr, args.socks_port);
        if (server_v6 < 0) {
            err_msg = "unable to create IPv6 socket";
            goto finally;
        }
        fprintf(stdout, "Listening IPv6 socks on TCP port %d\n", args.socks_port);
    }

     // socket pasivo monitoreo IPv6
     ipv6_addr_text = args.is_default_mng_addr ? DEFAULT_CONF_ADDR_V6 : args.mng_addr;

    if((!IS_FD_USED(monitor_v4) || args.is_default_mng_addr) && (inet_pton(AF_INET6, ipv6_addr_text, &monitor_ipv6_addr) == 1)){
        monitor_v6 = bind_ipv6_socket(monitor_ipv6_addr, args.mng_port);
        if (monitor_v6 < 0) {
            err_msg = "unable to create IPv6 socket";
            goto finally;
        }
        fprintf(stdout, "Listening IPv6 socks on TCP port %d\n", args.mng_port);
    }
    
    if(!IS_FD_USED(server_v4) && !IS_FD_USED(server_v6)) {
        fprintf(stderr, "unable to parse socks server ip\n");
        goto finally;
    }

    if(!IS_FD_USED(monitor_v4) && !IS_FD_USED(monitor_v6)) {
        fprintf(stderr, "unable to parse monitor server ip\n");
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    // seteamos los sockets pasivos como no bloqueantes
    if(IS_FD_USED(server_v4) && (selector_fd_set_nio(server_v4) == -1)){
        err_msg = "getting socks server ipv4 socket flags";
        goto finally;
    }

    if(IS_FD_USED(server_v6) && (selector_fd_set_nio(server_v6) == -1)) {
        err_msg = "getting socks server ipv6 socket flags";
        goto finally;
    }

    if(IS_FD_USED(monitor_v4) && (selector_fd_set_nio(monitor_v4) == -1)){
        err_msg = "getting monitor server ipv4 socket flags";
        goto finally;
    }

    if(IS_FD_USED(monitor_v6) && (selector_fd_set_nio(monitor_v6) == -1)) {
        err_msg = "getting monitor server ipv6 socket flags";
        goto finally;
    }

    const struct selector_init conf = {
        .signal = SIGALRM, // señal para las notificaciones internas del selector
        .select_timeout = { // tiempo maximo de bloqueo, es una estructura de timespec
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024); // initial elements
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    // handlers para cada tipo de accion (read, write y close) sobre el socket pasivo
    const struct fd_handler socksv5 = {
        .handle_read       = socksv5_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };

    if(IS_FD_USED(server_v4)){
        ss = selector_register(selector, server_v4, &socksv5, OP_READ, NULL);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "registering IPv4 socks fd";
            goto finally;
        }
    }
    if(IS_FD_USED(server_v6)){
        ss = selector_register(selector, server_v6, &socksv5, OP_READ, NULL);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "registering IPv6 socks fd";
            goto finally;
        }
    }

    const struct fd_handler monitor = {
        .handle_read       = monitor_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL,
    };

    if(IS_FD_USED(monitor_v4)){
        ss = selector_register(selector, monitor_v4, &monitor, OP_READ, NULL);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "registering IPv4 monitor fd";
            goto finally;
        }
    }
    if(IS_FD_USED(monitor_v6)){
        ss = selector_register(selector, monitor_v6, &monitor, OP_READ, NULL);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "registering IPv6 monitor fd";
            goto finally;
        }
    }

    // TODO: registrar admin default monitoreo (segun parametro REQUERIDO)
    // monitor_register_admin("root", args.admin_token);

    // register proxy users
    for (int i = 0; i < MAX_USERS && args.users[i].name != NULL; i++) {
        int register_status = socksv5_register_user(args.users[i].name, args.users[i].pass);
        if (register_status == -1)
            fprintf(stderr, "User already exists: %s\n", args.users[i].name);
        else if (register_status == 1)
            fprintf(stderr, "Maximum number of users reached\n");
    }

    if (!args.disectors_enabled)
        socksv5_toggle_disector(false);

    printf("\n----------------------- LOGS -----------------------\n\n");
    // termina con un ctrl + C pero dejando un mensajito
    while(!done) {
        err_msg = NULL;
        // se bloquea hasta que haya eventos disponible y los despacha.
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }

    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;

finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
            ss == SELECTOR_IO ? strerror(errno) : selector_error(ss)
        );
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }

    if(selector != NULL)
        selector_destroy(selector);

    selector_close();

    socksv5_pool_destroy();

    if (server_v4 >= 0)
        close(server_v4);

    if(server_v6 >= 0)
        close(server_v6);

    return ret;
}

static int
create_socket(sa_family_t family) {
    const int s = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        fprintf(stderr, "unable to create socket\n");
        return -1;
    }

    int sock_optval[] = { 1 }; //  valor de SO_REUSEADDR
    socklen_t sock_optlen = sizeof(int);
    // man 7 ip. no importa reportar nada si falla.
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void*)sock_optval, sock_optlen);
    return s;
}

static int
bind_socket(int server, struct sockaddr *address, socklen_t address_len) {
    if (bind(server, address, address_len) < 0) {
        fprintf(stderr, "unable to bind socket\n");
        return -1;
    }

    if (listen(server, 5) < 0) {
        fprintf(stderr, "unable to listen on socket\n");
        return -1;
    }

    return 0;
}

/** creates and binds an IPv4 socket */
static int
bind_ipv4_socket(struct in_addr bind_address, unsigned port) {
    const int server = create_socket(AF_INET);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr        = bind_address;
    addr.sin_port        = htons(port); // htons translates a short integer from host byte order to network byte order.

    if (bind_socket(server, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        return -1;

    return server;
}

/** creates and binds an IPv6 socket */
static int
bind_ipv6_socket(struct in6_addr bind_address, unsigned port) {
    const int server = create_socket(AF_INET6);
    setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &(int){1}, sizeof(int)); // man ipv6, si falla fallara el bind

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family      = AF_INET6;
    addr.sin6_addr        = bind_address;
    addr.sin6_port        = htons(port); // htons translates a short integer from host byte order to network byte order.

    if (bind_socket(server, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        return -1;

    return server;
}
