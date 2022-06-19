/**
 * monitor.c -- parser del request de protocolo nuestro de monitoreo
 */
#include <string.h> //memset
#include <arpa/inet.h> //ntohs

#include "../include/monitor.h"


#define IS_ALNUM(x) (x>='a' && (x) <= 'z') || (x>='A' && x <= 'Z') || (x>='0' && x <= '9')

static int separated = 0;

static uint8_t combinedlen[2] = {0};

static void
remaining_set(struct monitor_parser *p, uint16_t len) {
    p->i = 0;
    p->len = len; 
}

static int 
remaining_is_done(struct monitor_parser *p) {
    return p->i >= p->len;
}

extern void
monitor_parser_init(struct monitor_parser *p) {
    p->state = monitor_version;
    memset(p->monitor, 0, sizeof(*(p->monitor)));
}

static enum monitor_state
version(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;
    switch (c) {
        case 0x01:
            remaining_set(p, TOKEN_SIZE);
            next = monitor_token;
            break;
        default:
            next = monitor_error_unsupported_version;
            break;
    }

    return next;
}

static enum monitor_state
token(const uint8_t c, struct monitor_parser *p) {
    p->monitor->token[p->i++] = c;
    if (remaining_is_done(p))
        return monitor_method;
    return monitor_token;
}

static enum monitor_state
method(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;

    p->monitor->method = c;
    switch (p->monitor->method) {
        case monitor_method_get:
        case monitor_method_config:
            next = monitor_target;
            break;
        default:
            next = monitor_error_unsupported_method;
            break;
    }

    return next;
}

static enum monitor_state
target(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;

    switch(p->monitor->method) {
        case monitor_method_get:
            switch (c) {
                case monitor_target_get_historic:
                case monitor_target_get_concurrent:
                case monitor_target_get_transfered:
                case monitor_target_get_proxyusers:
                case monitor_target_get_adminusers:
					p->monitor->target.target_get = c;
                    next = monitor_done;
                    break;
                default:
                    next = monitor_error_unsupported_target;
                    break;
            }
            break;
        case monitor_method_config:
            switch (c) {
                case monitor_target_config_pop3disector:
                case monitor_target_config_add_proxyuser:
                case monitor_target_config_delete_proxyuser:
                case monitor_target_config_add_admin:
                case monitor_target_config_delete_admin:
					p->monitor->target.target_config = c;
                    remaining_set(p, 2); // vamos a leer 2 bytes para el dlen
                    next = monitor_dlen;
                    break;
                default:
                    next = monitor_error_unsupported_target;
                    break;
            }
            break;
        default:
            // impossible
            break;
    }

    return next;
}

//Si llego aca estamos con un target de tipo config
static enum monitor_state
dlen(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;

    if (!remaining_is_done(p) && (p->i == 2 && p->monitor->method == monitor_method_config && p->monitor->target.target_config == monitor_target_config_pop3disector)) {
        combinedlen[p->i++] = c;
        next = monitor_dlen;
    } else {
        p->monitor->dlen = ntohs(*(uint16_t*)combinedlen); // Para evitar problemas de endianness armo el uint16 de dlen segun el endianness del sistema. (Suponiendo que me los manda en network order "bigendean")
        switch (p->monitor->target.target_config) {
            case monitor_target_config_pop3disector:
                next = monitor_data;
                break;
            case monitor_target_config_add_proxyuser:
                remaining_set(p, p->monitor->dlen);
                next = monitor_data;
                break;
            case monitor_target_config_delete_proxyuser:
                remaining_set(p, p->monitor->dlen); 
                next = monitor_data;
                break;
            case monitor_target_config_add_admin:
                remaining_set(p, p->monitor->dlen);
                next = monitor_data;
                break;
            case monitor_target_config_delete_admin:
                remaining_set(p, p->monitor->dlen);
                next = monitor_data;
                break;
            default:
                next = monitor_error;
                break;
        }
    }
    
    return next;
}

static enum monitor_state
data(const uint8_t c, struct monitor_parser *p) {
    enum monitor_state next;

    switch(p->monitor->target.target_config) { 
        case monitor_target_config_pop3disector:
            p->monitor->data.disector_data_params = c;
            next = monitor_done;
            break;
        
        case monitor_target_config_add_proxyuser: 
            // Si el primer caracter es 0 directamente tiro error ya que el usuario no puede ser vacio
            if (p->i == 0 && c == 0) {
                next = monitor_error_invalid_data;
                break;
            }

            if (remaining_is_done(p)) {
                p->monitor->data.add_proxy_user_param.pass[p->i++] = 0; // null terminated para password
                next = monitor_done;
                break;
            }

            if (IS_ALNUM(c)) {
                if (separated == 0) {
                    p->monitor->data.add_proxy_user_param.user[p->i++] = c;
                } else {
                    p->monitor->data.add_proxy_user_param.pass[p->i++] = c;
                }
            } else if (c == 0 && separated == 0) { // primer separador \0 pongo el null terminated en el username
                p->monitor->data.add_proxy_user_param.user[p->i++] = c;
                separated = 1;
            } else { // Si no es alfanumerico ni fue el primer 0 separador entonces no es un dato valido
                next = monitor_error_invalid_data;
                break;
            }
            
            next = monitor_data;
            break;

        case monitor_target_config_delete_proxyuser:
            if (remaining_is_done(p)) {
                p->monitor->data.user[p->i++] = 0; // null terminated para username
                next = monitor_done;
                break;
            }
        
            if (IS_ALNUM(c)) {
                p->monitor->data.user[p->i++] = c;
            } else {
                next = monitor_error_invalid_data;
                break;
            } 

            next = monitor_data;
            break;

        case monitor_target_config_add_admin:
            // Si el primer caracter es 0 directamente tiro error ya que el usuario no puede ser vacio
            if (p->i == 0 && c == 0) {
                next = monitor_error_invalid_data;
                break;
            }

            if (remaining_is_done(p)) {
                p->monitor->data.add_admin_user_param.token[p->i++] = 0; // null terminated para password
                next = monitor_done;
                break;
            }

            if (IS_ALNUM(c)) {
                if (separated == 0) {
                    p->monitor->data.add_admin_user_param.user[p->i++] = c;
                } else {
                    p->monitor->data.add_admin_user_param.token[p->i++] = c;
                }
            } else if (c == 0 && separated == 0) { // primer separador \0 pongo el null terminated en el username
                p->monitor->data.add_admin_user_param.user[p->i++] = c;
                separated = 1;
            } else { // Si no es alfanumerico ni fue el primer 0 separador entonces no es un dato valido
                next = monitor_error_invalid_data;
                break;
            }
            
            next = monitor_data;
            break;

        case monitor_target_config_delete_admin:
             if (remaining_is_done(p)) {
                p->monitor->data.user[p->i++] = 0; // null terminated para username
                next = monitor_done;
                break;
            }
        
            if (IS_ALNUM(c)) {
                p->monitor->data.user[p->i++] = c;
            } else {
                next = monitor_error_invalid_data;
                break;
            } 

            next = monitor_data;
            break;
    }


    return next;
}


/** entrega un byte al parser, retorna true si se llego al final */
static enum monitor_state 
monitor_parser_feed(struct monitor_parser *p, const uint8_t c) {
    enum monitor_state next;

    switch(p->state) {
        case monitor_version:
            next = version(c, p);
            break;
        case monitor_token:
            next = token(c, p);
            break;
        case monitor_method:
            next = method(c, p);
            break;
        case monitor_target:
            next = target(c, p);
            break;
        case monitor_dlen:
            next = dlen(c, p);
            break;
        case monitor_data:
            next = data(c, p);
            break;
        case monitor_done:
        case monitor_error:
        case monitor_error_unsupported_version:
        case monitor_error_invalid_token:
        case monitor_error_unsupported_method:
        case monitor_error_unsupported_target:
        case monitor_error_invalid_data:
            next = p->state;
            break;
        default:
            next = monitor_error;
            break;
    }

    return p->state = next;
}

extern bool
monitor_is_done(const enum monitor_state st) {
    return st >= monitor_done;
}

extern enum monitor_state
monitor_consume(buffer *b, struct monitor_parser *p) {
    enum monitor_state st = p->state;

    while (buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);
        st = monitor_parser_feed(p, c); // cargamos 1 solo byte
        if (monitor_is_done(st))
            break;
    }
    return st;
}

extern int
monitor_error_marshall(buffer *b, struct monitor_parser *p) {
    enum monitor_state st = p->state;

    size_t n;
    buffer_write_ptr(b, &n);

    if (n < 4)
        return -1;

    // STATUS
    switch(st) {
        case monitor_error_unsupported_version:
            buffer_write(b, monitor_status_invalid_version);
            break;
        case monitor_error_invalid_token:
            buffer_write(b, monitor_status_error_auth);
            break;
        case monitor_error_unsupported_method:
            buffer_write(b, monitor_status_invalid_method);
            break;
        case monitor_error_unsupported_target:
            buffer_write(b, monitor_status_invalid_target);
            break;
        case monitor_error_invalid_data:
            buffer_write(b, monitor_status_invalid_data);
            break;
        default:
            buffer_write(b, monitor_status_server_error);
            break;
    }

    union dlen {
        uint16_t len;
        uint8_t byte[2];
    };
    union dlen datalen;
    datalen.len = htons(1); 

    // DLEN
    buffer_write(b, datalen.byte[0]);
    buffer_write(b, datalen.byte[1]);

    // DATA
    buffer_write(b, 0);

    return 4;
}

extern int
monitor_marshall(buffer *b, const enum monitor_response_status status, uint16_t dlen, void *data, bool numeric_data) {
    // llenar status y dlen primero, checkeando el espacio que hay en el buffer (si te quedas sin espacio en el buffer retornas -1)
    size_t n;
    buffer_write_ptr(b, &n);

    if (n < (size_t) dlen + 3)
        return -1;
    
    uint8_t array[2];
    //Bigendian store most significant from the lowest memory
    array[0]= (dlen >> 8);
    array[1]= dlen & 0xff;
    


    buffer_write(b, status);
    buffer_write(b, array[0]);
    buffer_write(b, array[1]);


    uint8_t numeric_response[4];

    if (numeric_data) {
        dlen = htons(dlen);
        uint32_t number = htonl(*((uint32_t*)data));
        memcpy(numeric_response, &number, sizeof(uint32_t));

        for (int i = 0; i < 4; i++) {
            buffer_write(b, numeric_response[i]);
        }
    } else {
        uint8_t *databytes = (uint8_t *) data; 
        if (databytes == NULL) {
            buffer_write(b, 0);
        } else {
            for (uint16_t i = 0; i < dlen; i++) {
                buffer_write(b, databytes[i]);
            }
        }
    }

    return dlen + 3;
}
