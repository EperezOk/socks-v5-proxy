#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define DEFAULT_SOCKS_ADDR          "0.0.0.0"
#define DEFAULT_SOCKS_ADDR_V6       "::0"
#define DEFAULT_SOCKS_PORT          1080

#define DEFAULT_CONF_ADDR           "127.0.0.1"
#define DEFAULT_CONF_ADDR_V6        "::1"
#define DEFAULT_CONF_PORT           8080

#define DEFAULT_DISECTORS_ENABLED   true

#define MAX_USERS           10

struct users {
    char            *name;
    char            *pass;
};

struct socks5args {
    char            *socks_addr;
    bool            is_default_socks_addr;
    unsigned short  socks_port;

    char            *mng_addr;
    bool            is_default_mng_addr;
    unsigned short  mng_port;

    bool            disectors_enabled;

    struct users    users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct socks5args *args);

#endif

