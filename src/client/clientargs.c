#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "../include/clientargs.h"


static unsigned short
port(const char *s, char* progname) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end 
    || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "%s: invalid port %s, should be an integer in the range of 1-65536.\n", progname, s);
        exit(1);
    }
    return (unsigned short)sl;
}

static size_t
token_check(const char *src, char *dest_token, char *progname){
    size_t token_len;
    if((token_len = strlen(src)) != TOKEN_SIZE){
        fprintf(stderr, "%s: invalid toklen length (%zu), should be of exactly %d characters.\n", progname, token_len, TOKEN_SIZE);
        exit(1);
    }
    memcpy(dest_token, src, TOKEN_SIZE);
    return TOKEN_SIZE;
}

static size_t
string_check(const char *src, char *dest, char* field_name, size_t max_len, char* progname){
    size_t str_len;
    if((str_len = strlen(src)) > max_len){
        fprintf(stderr, "%s: invalid username length (%zu), should be of at most %zu characters.\n", progname, str_len, max_len);
        exit(1);
    }
    memcpy(dest, src, str_len);
    return str_len;
}

static size_t
username_with_password(char *src, struct config_add_proxy_user *user_params, char *progname){
    size_t str_len;
    char *separator = strchr(src, ':');

    if(separator == NULL) {
        fprintf(stderr, "%s: missing password for user %s.\n", progname, src);
        exit(1);
    } else if(src[0] == ':') {
        fprintf(stderr, "%s: missing username for password %s.\n", progname, separator + 1);
        exit(1);
    }

    char *username = strtok(src, ":");
    char *password = separator + 1;

    str_len = string_check(username, user_params->user, "username", USERNAME_SIZE, progname);
    user_params->separator = 0;
    str_len++;
    str_len += string_check(password, user_params->pass, "password", PASSWORD_SIZE, progname);
    
    return str_len;
}

static size_t
username_with_token(char *src, struct config_add_admin_user *admin_params, char *progname){
    size_t str_len;
    char *separator = strchr(src, ':');

    if(separator == NULL){
        fprintf(stderr, "%s: missing token for user %s.\n", progname, src);
        exit(1);
    } else if(src[0] == ':'){
        fprintf(stderr, "%s: missing username for token %s.\n", progname, separator + 1);
    }

    char* username = strtok(src, ":");
    char* token = separator + 1;

    str_len = string_check(username, admin_params->user, "username", USERNAME_SIZE, progname);
    admin_params->separator = 0;
    str_len++;
    str_len += token_check(token, admin_params->token, progname);
    return str_len;
}

static void
version(void) {
    fprintf(stderr, "Cliente Protocolo de Monitoreo de Servidor / Version 1\n"
                    "ITBA Protocolos de Comunicación 2022/1 -- Grupo 13\n");
}

static void
usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTIONS] [DESTINATION] [PORT]\n"
        "Options:\n"
        "-h                 imprime los comandos del programa.\n"
        "-c                 imprime la cantidad de conexiones concurrentes del server.\n"
        "-C                 imprime la cantidad de conexiones históricas del server.\n"
        "-b                 imprime la cantidad de bytes transferidos del server.\n"
        "-a                 imprime una lista con los usuarios del proxy.\n"
        "-A                 imprime una lista con los usuarios administradores.\n"
        "-n                 enciende el password disector en el server.\n"
        "-N                 apaga el password disector en el server.\n"
        "-u<user:pass>      agrega un usuario del proxy con el nombre y contraseña indicados.\n"
        "-U<user:token>     agrega un usuario administrador con el nombre y token indicados.\n"
        "-d<user>           borra el usuario del proxy con el nombre indicado.\n"
        "-D<user>           borra el usuario administrador con el nombre indicado.\n"
        "-v                 imprime la versión del programa.\n"
        "\n",
        progname);
    exit(1);
}

static void
set_get_data(struct client_request_args *args) {
    args->method = get;
    args->dlen = 0;
    args->data.optional_data = 0;
}

void 
parse_args(const int argc, char **argv, struct client_request_args *args, struct sockaddr_in *sin4, struct sockaddr_in6 *sin6, enum ip_version *ip_version) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users, ademas de poner los campos opcionales en 0 (y el separator)
    memset(sin4, 0, sizeof(*sin4));
    memset(sin6, 0, sizeof(*sin6));

    int c;

    sin4->sin_family = AF_INET;
    sin4->sin_port = htons(port(DEFAULT_CONF_PORT, argv[0]));
    inet_pton(AF_INET, DEFAULT_CONF_ADDR, &sin4->sin_addr);
    *ip_version = ipv4;

    while (1) {
        /*
            Uso: getopt(argc, argv, optstring)
            - argc, argv: Los de consola
            - optstring: String con los caracteres de las opciones válidas concatenados. Pueden tener 2 formatos:
                - '<opcion>'    ->  Para opciones sin argumentos (ej: "./socks5d -h")
                - '<opcion>:'   ->  Para opciones con argumentos (ej: "./socks5d -p9999")
                - '<opcion>::'  ->  Para opciones con argumentos opcionales (No tenemos asi que no se usa)
            En el caso de que se lea una opcion válida se incrementa la variable 'optind' y se guarda el valor del
            argumento leido en 'optarg' si tiene argumento, sino se setea en 0 (optarg = NULL).
            Además, se puede aregar un ':' al principio del string, así los errores no se imprimen en pantalla. Esto sirve
            para diferenciar cuando el error es por argumento invalido (getopt retorna '?') o que el argumento es valido
            pero falta su valor (getopt retorna '!'). En ambos retornos, el argumento procesado se guarda en 'optopt' y se
            puede usar en los mensajes de error custom.
        */
        c = getopt(argc, argv, ":hcCbaAnNu:U:d:D:hv");
        if (c == -1){
            break;
        }

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'c':
                // Get concurrent connections
                set_get_data(args);
                args->target.get_target = concurrent_connections;
                break;
            case 'C':
                // Get historic connections
                set_get_data(args);
                args->target.get_target = historic_connections;
                break;
            case 'b':
                // Get bytes transferred
                set_get_data(args);
                args->target.get_target = transferred_bytes;
                break;
            case 'a':
                // Get list of proxy users
                set_get_data(args);
                args->target.get_target = proxy_users_list;
                // TODO: Show list of proxy users
                break;
            case 'A':
                // Get list of admin users
                set_get_data(args);
                args->target.get_target = admin_users_list;
                // TODO: Show list of admin users
                break;
            case 'n':
                // Turns on password disector
                args->method = config;
                args->target.config_target = toggle_disector;
                args->dlen = 1;
                args->data.disector_data_params = disector_on;
                break;
            case 'N':
                // Turns off password disector
                args->method = config;
                args->target.config_target = toggle_disector;
                args->dlen = 1;
                args->data.disector_data_params = disector_off;
                break;
            case 'u':
                // Adds proxy user
                args->method = config;
                args->target.config_target = add_proxy_user;
                args->dlen = username_with_password(optarg, &args->data.add_proxy_user_params, argv[0]);
                break;
            case 'U':
                // Adds admin user
                args->method = config;
                args->target.config_target = add_admin_user;
                args->dlen = username_with_token(optarg, &args->data.add_admin_user_params, argv[0]);
                break;
            case 'd':
                // Deletes proxy user
                args->method = config;
                args->target.config_target = del_proxy_user;
                args->dlen = string_check(optarg, args->data.user, "username", USERNAME_SIZE, argv[0]);
                break;
            case 'D':
                // Deletes admin user
                args->method = config;
                args->target.config_target = del_admin_user;
                args->dlen = string_check(optarg, args->data.user, "username", USERNAME_SIZE, argv[0]);
                break;
            case 'v':
                // Prints program version
                version();
                break;
            case ':':
                fprintf(stderr, "%s: missing value for option -%c.\n", argv[0], optopt);
                usage(argv[0]);
                exit(1);
                break;
            case '?':
                fprintf(stderr, "%s: invalid option -%c.\n", argv[0], optopt);
                usage(argv[0]);
                exit(1);
            default:
                // no deberia llegar aca
                break;
        }
    }

    for ( ; optind < argc ; optind++){
        if(argc - optind == 1) {                        // port (field in ipv4 and ipv6 structs is on same memory place)
            if(*ip_version == ipv4)
                sin4->sin_port = htons(port(argv[optind], argv[0]));
            else
                sin6->sin6_port = htons(port(argv[optind], argv[0]));
        }
        else if(argc - optind == 2) {                   // destination ip
            if(inet_pton(AF_INET, argv[optind], &sin4->sin_addr) > 0){
                sin4->sin_family = AF_INET;
                *ip_version = ipv4;
            } else if(inet_pton(AF_INET6, argv[optind], &sin6->sin6_addr.s6_addr) > 0) {
                sin6->sin6_family = AF_INET6;
                *ip_version = ipv6;
            } else {
                printf("error parsing ip\n");
                exit(1);
            }
        }
    }

    // Token parsing
    char* token = getenv(TOKEN_ENV_VAR_NAME);
    if(token == NULL){
        fprintf(stderr, "%s: token not present, set it with 'export %s=<token>'.\n", argv[0], TOKEN_ENV_VAR_NAME);
        exit(1);
    }
    token_check(getenv("MONITOR_TOKEN"), args->token, argv[0]);

}
