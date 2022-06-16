#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../include/buffer.h";
#include "../include/monitornio.h";
#include "../include/socks5nio.h";

struct connection {
    /** informacion del cliente */
    int                           client_fd;

    /** estados del parseo de la request. contendra tanto el PARSER como el ESTADO de la conexion. */
    struct monitor_st             request;

    /** buffers para ser usados por read_buffer y write_buffer */
    uint8_t raw_buff_a[0xffff], raw_buff_b[0xffff];
    buffer  read_buffer, write_buffer;

    /** siguiente en la pool */
    struct connection *next;
};

struct monitor_st {
    buffer       *rb, *wb;
    // TODO: agregar parser y state
};

/** Pool de structs connection para ser reusados */
static const unsigned       max_pool = 5;  // tamaño max
static unsigned             pool_size = 0; // tamaño actual
static struct connection    *pool = 0;     // pool propiamente dicho

static struct connection *connection_new(int client_fd) {
    struct connection *ret;

    if (pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret = pool;
        pool = pool->next;
        ret->next = 0; // lo sacamos de la pool para retornarlo y usarlo
    }

    if (ret == NULL)
        goto finally;
    
    memset(ret, 0x00, sizeof(*ret)); // inicializamos en 0 todo

    ret->client_fd = client_fd;

    buffer_init(&ret->read_buffer, N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

finally:
    return ret;
}

/**
 * destruye un  `struct connection', tiene en cuenta el pool de objetos.
 */
static void
connection_destroy(struct connection *s) {
    if(s == NULL) return;

    if(pool_size < max_pool) { // agregamos a la pool
        s->next = pool;
        pool    = s;
        pool_size++;
    } else {
        free(s);    // realmente destruye
    }
}

// TODO: llamar al finalizar server.c
void
connection_pool_destroy(void) {
    struct connection *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}

/** obtiene el struct (connection *) desde la llave de selección  */
#define ATTACHMENT(key) ((struct connection *)(key)->data)

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el servidor de monitoreo.
 */
static void monitor_read   (struct selector_key *key);
static void monitor_write  (struct selector_key *key);
static void monitor_close  (struct selector_key *key);

static const struct fd_handler monitor_handler = {
    .handle_read   = monitor_read,  // selector despierta para lectura
    .handle_write  = monitor_write, // selector despierta para escritura
    .handle_close  = monitor_close, // se llama en el selector_unregister_fd
};

/** Intenta aceptar la nueva conexión entrante*/
void
monitor_passive_accept(struct selector_key *key) {
    struct connection *state = NULL;

    const int client = accept(key->fd, NULL, NULL);

    if (client == -1 || selector_fd_set_nio(client) == -1)
        goto fail;

    // instancio estructura de estado
    state = connection_new(client);
    if (state == NULL)
        goto fail;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &monitor_handler, OP_READ, state))
        goto fail;

    return;

fail:
    if (client != -1)
        close(client);

    connection_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

struct admin {
    char    uname[0xff];
    char    token[0x10];
};

/** el admins[0] sera creado apenas se corra el servidor, pasando el token con el parametro adecuado */
struct admin admins[MAX_ADMINS]; // TODO: agregar admin root desde el server.c
size_t registered_admins = 0;

// TODO: funcion a llamarse en el monitor_process()
int monitor_register_admin(char *uname, char *token) {
    if (registered_admins >= MAX_ADMINS)
        return 1; // maximo numero de usuarios alcanzado
    
    if (strnlen(token, 0x10) < 0x10)
        return -1; // token invalido

    for (size_t i = 0; i < registered_admins; i++) {
        if (strcmp(uname, admins[i].uname) == 0)
            return -1; // username ya existente
    }

    // insertamos al final (podrian insertarse en orden alfabetico para mas eficiencia pero al ser pocos es irrelevante)
    strncpy(admins[registered_admins].uname, registered_admins == 0 ? "root" : uname, 0xff);
    strncpy(admins[registered_admins++].token, token, 0x10);
    return 0;
}

static bool
monitor_is_admin(char *token) {
    for (size_t i = 0; i < registered_admins; i++) {
        if (strncmp(token, admins[i].token, 0x10) == 0)
            return true;
    }
    return false;
}

static void
monitor_init(struct selector_key *key) {
    struct monitor_st *d    = &ATTACHMENT(key)->request;

    d->rb                   = &(ATTACHMENT(key)->read_buffer);
    d->wb                   = &(ATTACHMENT(key)->write_buffer);
    // TODO: incorporar con el parser
    // d->parser.request       = &d->request;
    // d->status               = status_monitor_server_failure;
    // monitor_parser_init(&d->parser);
}

static int monitor_process(struct selector_key *key, struct monitor_st *d);

/** lee todos los bytes de la request e inicia su proceso */
static void
monitor_read(struct selector_key *key) {
    struct monitor_st *d = &ATTACHMENT(key)->request;

    buffer *b       = d->rb;
    bool error      = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if (n <= 0) {
        monitor_done(key);
    } else {
        buffer_write_adv(b, n);
        // TODO: llamada a funciones del parser
        int st = monitor_consume(b, &d->parser, &error);
        if (monitor_is_done(st, 0)) {
            if (monitor_process(key, d) == -1) // solo debe retornar -1 en caso de error terminal, si es un error en la request se pasa a la escritura
                monitor_done(key);
            else
                selector_set_interest_key(key, OP_WRITE);
        }
    }
}

// solo debe retornar -1 en caso de error terminal en la conexion, si es un error en la request se pasa al paso de escritura (y retorno 0 por ej)
static int
monitor_process(struct selector_key *key, struct monitor_st *d) {
    // TODO: pasar el token
    if (!monitor_is_admin(d->parser.token)) {
        // setear status invalid auth
        return;
    }

    // TODO: implementar con los campos del parser
    switch (d->method) {
        case GET:
            switch (d->target) {
                case bytes_transferred: {

                    break;
                }
                default: {
                    // TODO: setear en parser status invalid target
                    break;
                }
            }
            break;
        case CONFIG:
            switch (d->target) {
                case disector_on: {

                }
            }
            break;
        default:
            // TODO: setear en el estado del parser invalid method
            break;
    }

    if (-1 == monitor_marshall(d->wb, d->status))
        abort(); // el buffer tiene que ser mas grande en la variable
}

static void
monitor_write(struct selector_key *key) {
    struct monitor_st *d = &ATTACHMENT(key)->request;

    buffer *b    = d->wb;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_read_ptr(b, &count);
    // como nos desperto el select, al menos 1 byte tenemos que poder mandar
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if (n == -1) {
        monitor_done(key);
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b))
            monitor_done(key); // terminamos de escribir, cerramos la conexion
    }
}

/** finaliza la conexion, tanto si termino con exito (status positivo o negativo) como si no (error de comunicacion) */
static void
monitor_done(struct selector_key* key) {
    int client_fd = ATTACHMENT(key)->client_fd;
    if (client_fd != -1) {
        if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, client_fd)) // desencadena el monitor_close()
            abort();
        close(client_fd);
    }
}

static void
monitor_close(struct selector_key *key) {
    connection_destroy(ATTACHMENT(key));
}
