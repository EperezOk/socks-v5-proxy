#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../include/buffer.h"
#include "../include/monitor.h"
#include "../include/monitornio.h"
#include "../include/socks5nio.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

struct monitor_st {
    buffer                       *rb, *wb;
    struct monitor               monitor;
    struct monitor_parser        parser;
    enum monitor_response_status status;
};

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

static void
monitor_init(struct connection *state) {
    struct monitor_st *d    = &state->request;
    d->rb                   = &(state->read_buffer);
    d->wb                   = &(state->write_buffer);
    d->parser.monitor       = &d->monitor;
    d->status               = monitor_status_server_error;
    monitor_parser_init(&d->parser);
}

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

    monitor_init(state);

    return;

fail:
    if (client != -1)
        close(client);

    connection_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// MONITOR REQUEST
////////////////////////////////////////////////////////////////////////////////

struct admin {
    char    uname[0xff];    // null terminated
    char    token[0x11];    // 16 bytes fijos + \0
};

/** el admins[0] sera creado apenas se corra el servidor, pasando el token con el parametro adecuado */
struct admin admins[MAX_ADMINS]; // TODO: agregar admin root desde el server.c
size_t registered_admins = 0;
char *default_admin_uname = "root";

int monitor_register_admin(char *uname, char *token) { // ambos null terminated
    if (registered_admins >= MAX_ADMINS)
        return 1; // maximo numero de usuarios alcanzado
    
    if (strlen(token) != 0x10)
        return -1; // token invalido

    for (size_t i = 0; i < registered_admins; i++) {
        if (strcmp(uname, admins[i].uname) == 0)
            return -1; // username ya existente
    }

    // insertamos al final (podrian insertarse en orden alfabetico para mas eficiencia pero al ser pocos es irrelevante)
    strncpy(admins[registered_admins].uname, registered_admins == 0 ? default_admin_uname : uname, 0xff);
    strncpy(admins[registered_admins++].token, token, 0x11);
    return 0;
}

int monitor_unregister_admin(char *uname) {
    if (strcmp(default_admin_uname, uname) == 0)
        return -1; // no se puede remover el admin root

    for (size_t i = 1; i < registered_admins; i++) {
        if (strcmp(uname, admins[i].uname) == 0) {
            // movemos los elementos para tapar el hueco que pudo haber quedado
            if (i + 1 < registered_admins)
                memmove(&admins[i], &admins[i+1], sizeof(struct admin) * (registered_admins - (i + 1)));
            registered_admins--;
            return 0;
        }
    }
    return -1;  // usuario no encontrado
}

// entrega la lista de admins con formato <usuario>\0<usuario>
static uint16_t monitor_get_admins(char unames[MAX_ADMINS * 0xff]) {
    uint16_t dlen = 0;
    for (size_t i = 0; i < registered_admins; i++) {
        strcpy(unames + dlen, admins[i].uname);
        dlen += strlen(admins[i].uname) + 1; // incluimos el \0
    }
    return dlen - 1; // no pone el ultimo \0
}

static bool
monitor_is_admin(char *token) {
    for (size_t i = 0; i < registered_admins; i++) {
        if (strncmp(token, admins[i].token, 0x10) == 0)
            return true;
    }
    return false;
}

static void monitor_finish(struct selector_key* key);
static void monitor_process(struct selector_key *key, struct monitor_st *d);

/** lee todos los bytes de la request e inicia su proceso */
static void
monitor_read(struct selector_key *key) {
    struct monitor_st *d = &ATTACHMENT(key)->request;

    buffer *b       = d->rb;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if (n <= 0) {
        monitor_finish(key);
    } else {
        buffer_write_adv(b, n);
        int st = monitor_consume(b, &d->parser);
        if (monitor_is_done(st)) {
            monitor_process(key, d);    // ejecuta la accion pedida y escribe la response en el buffer
            selector_set_interest_key(key, OP_WRITE);   // pasamos al monitor_write() cuando podamos escribir
        }
    }
}

// solo debe retornar -1 en caso de error terminal en la conexion, si es un error en la request se pasa al paso de escritura (y retorno 0 por ej)
static void
monitor_process(struct selector_key *key, struct monitor_st *d) {
    uint8_t *data = NULL;
    // uint32_t *data = malloc(sizeof(uint32_t));
    uint16_t dlen = 1;
    bool numeric_data = false;
    int error_response = 0;

    if (!monitor_is_admin(d->parser.monitor->token)) {
        d->status = monitor_status_error_auth;
        goto finally;
    }

    switch (d->parser.monitor->method) {
        case monitor_method_get:
            switch (d->parser.monitor->target.target_get) {
                case monitor_target_get_concurrent: {
                    uint32_t cc = socksv5_current_connections();
                    dlen = sizeof(cc);
                    data = malloc(dlen);
                    *((uint32_t*)data) = cc;
                    numeric_data = true;
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_get_historic: {
                    uint32_t hc = socksv5_historic_connections();
                    dlen = sizeof(hc);
                    data = malloc(dlen);
                    *((uint32_t*)data) = hc;
                    numeric_data = true;
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_get_transfered: {
                    uint32_t bt = socksv5_bytes_transferred();
                    dlen = sizeof(bt);
                    data = malloc(dlen);
                    *((uint32_t*)data) = bt;
                    numeric_data = true;
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_get_proxyusers: {
                    char usernames[MAX_USERS * 0xff];
                    dlen = socksv5_get_users(usernames);
                    data = malloc(dlen);
                    memcpy(data, usernames, dlen);
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_get_adminusers: {
                    char usernames[MAX_USERS * 0xff];
                    dlen = monitor_get_admins(usernames);
                    data = malloc(dlen);
                    memcpy(data, usernames, dlen);
                    d->status = monitor_status_succeeded;
                    break;
                }
                default: {
                    d->status = monitor_status_invalid_target;
                    break;
                }
            }
            break;
        case monitor_method_config:
            switch (d->parser.monitor->target.target_config) {
                case monitor_target_config_pop3disector: {
                    bool to = d->parser.monitor->data.disector_data_params == disector_on ? true : false;
                    socksv5_toggle_disector(to);
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_config_add_proxyuser: {
                    error_response = socksv5_register_user(d->parser.monitor->data.add_proxy_user_param.user, d->parser.monitor->data.add_proxy_user_param.pass);
                    // error_response = socksv5_register_user("vyeli2", "tefaltacalle");
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_config_delete_proxyuser: {
                    error_response = socksv5_unregister_user(d->parser.monitor->data.user);
                    // error_response = socksv5_unregister_user("vyeli2");
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_config_add_admin: {
                    error_response = monitor_register_admin(d->parser.monitor->data.add_admin_user_param.user, d->parser.monitor->data.add_admin_user_param.token);
                    // error_response = monitor_register_admin("admin2", "mybeautifulbokee");
                    d->status = monitor_status_succeeded;
                    break;
                }
                case monitor_target_config_delete_admin: {
                    error_response = monitor_unregister_admin(d->parser.monitor->data.user);
                    // error_response = monitor_unregister_admin("admin2");
                    d->status = monitor_status_succeeded;
                    break;
                }
                default: {
                    d->status = monitor_status_invalid_target;
                    break;
                }
            }
            break;
        default:
            d->status = monitor_status_invalid_method;
            break;
    }

finally:

    if (error_response != 0)
       d->status = monitor_status_invalid_data;

    if (-1 == monitor_marshall(d->wb, d->status, dlen, data, numeric_data))
        abort(); // el buffer tiene que ser mas grande en la variable

    free(data);
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
        monitor_finish(key);
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b))
            monitor_finish(key); // terminamos de escribir, cerramos la conexion
    }
}

/** finaliza la conexion, tanto si termino con exito (status positivo o negativo) como si no (error de comunicacion) */
static void
monitor_finish(struct selector_key* key) {
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
