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

// #include "include/socks5.h"
#include "include/selector.h"
#include "include/socks5nio.h"

#define DEFAULT_PORT    1080
#define MAX_CONNECTIONS 512

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

static int bind_ipv4_socket(struct in_addr bind_address, unsigned port);
static int bind_ipv6_socket(struct in6_addr bind_address, unsigned port);

int
main(const int argc, const char **argv) {
    unsigned port = DEFAULT_PORT;

    if(argc == 1) {
        // utilizamos el default
    } else if(argc == 2) {
        // parseamos el segundo argumento a int para tomarlo como puerto
        char *end     = 0;
        const long sl = strtol(argv[1], &end, 10);

        if (end == argv[1]|| '\0' != *end 
           || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
           || sl < 0 || sl > USHRT_MAX) {
            fprintf(stderr, "port should be an integer: %s\n", argv[1]);
            return 1;
        }
        port = sl;
    } else {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    // TODO: aca deberiamos usar la direccion recibamos en la opcion de linea de comandos "-l"
    struct in_addr address = { .s_addr = htonl(INADDR_ANY) };
    const int server = bind_ipv4_socket(address, port);
    if (server < 0) {
        err_msg = "unable to create IPv4 socket";
        goto finally;
    }
    fprintf(stdout, "Listening IPv4 socks on TCP port %d\n", port);

    // TODO: aca deberiamos usar la direccion recibamos en la opcion de linea de comandos "-l"
    const int server_v6 = bind_ipv6_socket(in6addr_any, port);
    if (server_v6 < 0) {
        err_msg = "unable to create IPv6 socket";
        goto finally;
    }
    fprintf(stdout, "Listening IPv6 socks on TCP port %d\n", 1081);

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    // seteamos los sockets pasivos como no bloqueantes
    if(selector_fd_set_nio(server) == -1 || selector_fd_set_nio(server_v6) == -1) {
        err_msg = "getting server socket flags";
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

    ss = selector_register(selector, server, &socksv5, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering IPv4 fd";
        goto finally;
    }

    ss = selector_register(selector, server_v6, &socksv5, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering IPv6 fd";
        goto finally;
    }

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

    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

    socksv5_pool_destroy();

    if (server >= 0)
        close(server);

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
    if(bind(server, address, address_len) < 0) {
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

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family      = AF_INET6;
    addr.sin6_addr        = bind_address;
    addr.sin6_port        = htons(port); // htons translates a short integer from host byte order to network byte order.

    if (bind_socket(server, (struct sockaddr*) &addr, sizeof(addr)) == 1)
        return -1;

    return server;
}
