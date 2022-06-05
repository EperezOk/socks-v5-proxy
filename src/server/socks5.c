// complete .......
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/buffer.h"
#include "../include/netutils.h"
#include "../include/socks5.h"


#define N(x) (sizeof(x)/sizeof((x)[0]))

/**
 * maneja cada conexion entrante. VERSION BLOQUEANTE.
 * 
 * @param fd descriptor de la conexion entrante
 * @param caddr informacion de la conexion entrante
*/
static void BLOCKING_socks5_handle_connection(const int fd, const struct sockaddr *caddr) {
    uint8_t method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
    struct request request; // representa request socks5 segun RFC 1928

    struct sockaddr *originaddr = 0x00;
    socklen_t origin_addr_len = 0;
    int origin_domain;
    int originfd = -1;

    uint8_t buff[10];
    buffer b;
    buffer_init(&b, N(buff), buff);

    // 0. lectura del hello enviado por el cliente
    if (read_hello(fd, &method))
        goto finally;

    // 1. envio de la respuesta
    const uint8_t r = (method == SOCKS_HELLO_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;

    hello_marshall(&b, r);

    // manda por el fd todo lo que haya para leer en el buffer (bloqueante)
    if (sock_blocking_write(fd, &b))
        goto finally;

    if (SOCKS_HELLO_NO_ACCEPTABLE_METHODS == method)
        goto finally;

    // 2. lectura del socket
    enum socks_response_status status = status_general_SOCKS_server_failure;
    if (read_request(fd, &request)) {
        status = status_general_SOCKS_server_failure;
    } else {
        // 3. procesamiento
        switch (request.cmd) {
            case socks_req_cmd_connect: {
                bool error = false;
                status = cmd_resolve(&request, &originaddr, &origin_addr_len, &origin_domain);

                if (originaddr == NULL) {
                    error = true;
                } else {
                    originfd = socket(origin_domain, SOCK_STREAM, 0);
                    if (originfd == -1) {
                        error = true;
                    } else {
                        if (-1 == connect(originfd, originaddr, origin_addr_len)) {
                            status = errno_to_socks(errno);
                            error = true;
                        } else {
                            status = status_succeeded;
                        }
                    }
                }

                if (error) {
                    if (originfd != -1) {
                        close(originfd);
                        originfd = -1;
                    }
                    close(fd);
                }
                break;
            }
            case socks_req_cmd_bind:
            case socks_req_cmd_associate:
            default:
                status = status_command_not_supported;
                break;
        }
    }

    log_request(status, caddr, originaddr);

    // 4. envio de respuesta al request
    request_marshall(&b, status);

    if (sock_blocking_write(fd, &b))
        goto finally;

    if (originfd == -1)
        goto finally;

    // 5. copia dos vias
    pthread_t tid;
    int fds[2][2] = {
        { originfd, fd },
        { fd, originfd },
    };

    if (pthread_create(&tid, NULL, copy_pthread, fds[0]))
        goto finally;

    socks_blocking_copy(fds[1][0], fds[1][1]);
    pthread_join(tid, 0);

finally:
    if (originfd != -1)
        close(originfd);
    
    close(fd);
}


