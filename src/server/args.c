#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "../include/args.h"

static unsigned short
port(const char *s, char* progname) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end 
    || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "%s: invalid port %s, should be an integer in the range of 1-65536.\n", progname, s);
        exit(1);
        return 1;
    }
    return (unsigned short)sl;
}

static void
user(char *s, struct users *user, char* progname) {
    char *p = strchr(s, ':');
    
    if(p == NULL) {
        fprintf(stderr, "%s: missing password for user %s.\n", progname, s);
        exit(1);
    } else if (s[0] == ':') {
        fprintf(stderr, "%s: missing username for password %s.\n", progname, p+1);
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }

}

static void
version(void) {
    fprintf(stderr, "socks5v version 1.0\n"
                    "ITBA Protocolos de Comunicación 2022/1 -- Grupo 13\n");
}

static void
usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTION]...\n"
        "   -h              Imprime la ayuda y termina.\n"
        "   -l<SOCKS addr>  Dirección donde servirá el proxy SOCKS. Por defecto escucha en todas las interfaces.\n"
        "   -N              Deshabilita los passwords disectors.\n"
        "   -L<conf  addr>  Dirección donde servirá el servicio de management. Por defecto escucha solo en loopback.\n"
        "   -p<SOCKS port>  Puerto TCP para conexiones entrantes SOCKS. Por defecto es 1080.\n"
        "   -P<conf  port>  Puerto TCP para conexiones entrantes del protocolo de configuracion. Por defecto es 8080.\n"
        "   -u<user>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.\n"
        "   -v              Imprime información sobre la versión y termina.\n"
        "\n",
        progname);
    exit(1);
}

void 
parse_args(const int argc, char **argv, struct socks5args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->socks_addr = DEFAULT_SOCKS_ADDR;
    args->socks_port = DEFAULT_SOCKS_PORT;
    args->is_default_socks_addr = true;         // Pongo los valores default. Si recibo un parametro con un valor los cambio abajo

    args->mng_addr   = DEFAULT_CONF_ADDR;
    args->mng_port   = DEFAULT_CONF_PORT;
    args->is_default_mng_addr = true;

    args->disectors_enabled = true;

    int c;
    int nusers = 0;

    while (true) {
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
        c = getopt(argc, argv, ":hl:L:Np:P:u:v");
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                args->socks_addr = optarg;
                args->is_default_socks_addr = false;
                break;
            case 'L':
                args->mng_addr = optarg;
                args->is_default_mng_addr = false;
                break;
            case 'N':
                args->disectors_enabled = false;
                break;
            case 'p':
                args->socks_port = port(optarg, argv[0]);
                break;
            case 'P':
                args->mng_port   = port(optarg, argv[0]);
                break;
            case 'u':
                if(nusers >= MAX_USERS) {
                    fprintf(stderr, "%s: sent too many users, maximum allowed is %d\n", argv[0], MAX_USERS);
                    exit(1);
                } else {
                    user(optarg, args->users + nusers, argv[0]);
                    printf("Found user #%d '%c' of value %s", optind, c, optarg);
                    nusers++;
                }
                break;
            case 'v':
                version();
                exit(0);
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
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
