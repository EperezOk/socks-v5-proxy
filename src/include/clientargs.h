#ifndef CLIENTARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define CLIENTARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define DEFAULT_CONF_ADDR           "127.0.0.1"
#define DEFAULT_CONF_PORT           8080

#define DEFAULT_DISECTORS_ENABLED   true

#define DATA_SIZE                   8192
#define TOKEN_SIZE                  16
#define USERNAME_SIZE               0xFF
#define PASSWORD_SIZE               0xFF

enum method {
    get     = 0x00,
    config  = 0x01
};

enum get_target {
    historic_connections    = 0x00,
    concurrent_connections  = 0x01,
    transferred_bytes       = 0x02,
    proxy_users_list        = 0x03,
    admin_users_list        = 0x04
};

enum config_target {
    toggle_disector     = 0x00,
    add_proxy_user      = 0x01,
    del_proxy_user      = 0x02,
    add_admin_user      = 0x03,
    del_admin_user      = 0x04
};

union target {
    enum get_target get_target;
    enum config_target config_target;
};

enum config_disector_data {
    disector_off    = 0x00,
    disector_on     = 0x01,
};

struct config_add_proxy_user {
    char        user[USERNAME_SIZE];
    char        separator;
    char        pass[PASSWORD_SIZE];
};

struct config_add_admin_user {
    char        user[USERNAME_SIZE];
    char        separator;
    char        token[TOKEN_SIZE];
};

union data {
    uint8_t                         optional_data;          // To send 0 according to RFC
    char                            user[USERNAME_SIZE];
    enum   config_disector_data     disector_data_params;
    struct config_add_proxy_user    add_proxy_user_params;
    struct config_add_admin_user    add_admin_user_params;
};

struct client_request_args {
    char            token[TOKEN_SIZE];
    enum method     method;
    union target    target;
    uint16_t        dlen;
    union data      data;
};

enum ip_version {
    ipv4 = 0x04,
    ipv6 = 0x06
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct client_request_args *args, struct sockaddr_in *sin4, struct sockaddr_in6 *sin6, enum ip_version *ip_version);

#endif