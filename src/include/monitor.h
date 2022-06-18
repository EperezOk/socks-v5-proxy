#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"

#define TOKEN_SIZE 16
#define DATA_SIZE 8192
#define USERNAME_SIZE 256
#define PASSWORD_SIZE 256

/** Docs: https://docs.google.com/document/d/11LOlBxNPXL2N811n5hB9y6pTDAw9zajFgFRj6j-yLTM/edit */ 

/**
 * Request

    VER | TOKEN | MÉTODO | TARGET | DLEN | DATA
    1       16      1        1       2      1 to 8191


VERSIÓN:
Este campo DEBE ser X' 01' .

TOKEN: Este campo DEBE ser un código de 16 caracteres usado para acceder al server de monitoreo.

MÉTODO:
    X'00' GET
    X'01' CONFIG

TARGET:
GET
    X'00'  cantidad de conexiones históricas
    X'01'  cantidad de conexiones concurrentes
    X'02'  cantidad de bytes transferidos
    X'03'  listado de usuarios del proxy
    X'04'  listado de administradores
CONFIG
    X'00'  ON/OFF password disector POP3
    X'01'  agregar usuario del proxy
    X'02'  borrar usuarios del proxy
    X'03'  agregar usuario admin
    X'04'  borrar usuarios admin

DLEN: cantidad de bytes presentes en la sección DATA. Para el método GET, este campo DEBERÍA ser X'01' .

DATA: 
    GET
        Este campo DEBERÍA ser X'00' , pero NO DEBERÍA ser leído por el servidor.
    CONFIG
        Pass disector:
            X'00'  OFF
            X'01'  ON
        Agregar usuario del proxy
            <usuario>X'00'<contraseña> 
        Borrar usuario proxy
            <usuario>
        Agregar usuario admin
            <usuario>X'00'<token>
        Borrar usuario admin
            <usuario>
*/

enum monitor_state {            
    monitor_version,
    monitor_token,
    monitor_method,
    monitor_target,
    monitor_dlen,
    monitor_data,

    // apartir de aca estan done
    monitor_done,

    // y apartir de aca son considerado con error 
    monitor_error,
    monitor_error_unsupported_version,
    monitor_error_invalid_token,
    monitor_error_unsupported_method,
    monitor_error_unsupported_target,
    monitor_error_invalid_data,
};

enum monitor_response_status {
    monitor_status_succeeded                            = 0x00,
    monitor_status_invalid_version                      = 0x01,
    monitor_status_invalid_method                       = 0x02,
    monitor_status_invalid_target                       = 0x03,
    monitor_status_invalid_data                         = 0x04,
    monitor_status_error_auth                           = 0x05,
    monitor_status_server_error                         = 0x06,

};


enum monitor_method {
    monitor_method_get    = 0x00,
    monitor_method_config = 0x01,
};

enum monitor_target_get {
    monitor_target_get_historic   = 0x00,
    monitor_target_get_concurrent = 0x01,
    monitor_target_get_transfered = 0x02,
    monitor_target_get_proxyusers = 0x03,
    monitor_target_get_adminusers = 0x04,
};

enum monitor_target_config {
    monitor_target_config_pop3disector      = 0x00,
    monitor_target_config_add_proxyuser     = 0x01,
    monitor_target_config_delete_proxyuser  = 0x02,
    monitor_target_config_add_admin         = 0x03,
    monitor_target_config_delete_admin      = 0x04,
};


union monitor_target {
    enum monitor_target_get    target_get;
    enum monitor_target_config target_config;
};


enum config_disector_data {
    disector_off    = 0x00,
    disector_on     = 0x01,
};

struct config_add_proxy_user {
    char        user[USERNAME_SIZE];
    char        pass[PASSWORD_SIZE];
};

struct config_add_admin_user {
    char        user[USERNAME_SIZE];
    char        token[TOKEN_SIZE];
};

union data {
    char                            user[USERNAME_SIZE]; // To delete proxy user or admin user
    enum   config_disector_data     disector_data_params;
    struct config_add_proxy_user    add_proxy_user_param;
    struct config_add_admin_user    add_admin_user_param;
};


struct monitor {
    char                    token[TOKEN_SIZE];
    enum  monitor_method    method;
    union monitor_target    target;
    uint16_t                dlen;
    union data              data;
};


struct monitor_parser {
    struct monitor *monitor;
    enum monitor_state state;
    /** cuantos bytes tenemos que leer*/
    uint8_t len;
    /** cuantos bytes ya leimos */
    uint8_t i;
};

/** inicializa el parser */
void
monitor_parser_init(struct monitor_parser *p);

/**
 * por cada elemento del buffer llama a "monitor_parser_feed" hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 * param errored parametro de salida. si es diferente de NULL se deja dicho valor
 * si el parsing se debio a una condicion de error
 */
enum monitor_state
monitor_consume(buffer *b, struct monitor_parser *p);



/*
 * Permite distinguir a quien usa monitor_parser_feed si debe seguir 
 * enviando caracteres o no
 * 
 * En caso de haber terminado permite tambien saber si se debe a un error.
 */
bool
monitor_is_done(const enum monitor_state st);

/*
 * serializa en buff una respuesta al request del protocolo de monitoreo,
 * 
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no habia 
 * espacio suficiente.
 */
extern int
monitor_marshall(buffer *b, const enum monitor_response_status status, uint16_t dlen, void *data);

#endif
