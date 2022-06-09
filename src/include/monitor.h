#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"

/** Docs: https://docs.google.com/document/d/11LOlBxNPXL2N811n5hB9y6pTDAw9zajFgFRj6j-yLTM/edit */ 

enum monitor_req_method {
    method_get = 0x00,
    method_config = 0x01,
};

enum monitor_req_target_get {
    target_get_historic = 0x00,
    target_get_concurrent = 0x01,
    target_get_transfered = 0x02,
    // target_get_uptime = 0x02,
};

enum monitor_req_target_config_opt {
    target_config_pop3disector = 0x00,
};

struct monitor_req_target_config {
    enum monitor_req_target_config_opt option;
    bool (*isValid)(struct monitor_parser *);
};

union monitor_req_target {
    enum monitor_req_target_get get;
    struct monitor_req_target_config config;
};

struct monitor {
    enum    monitor_req_method     method;
    union   monitor_req_target     target;
    uint8_t                        dlen;
    uint8_t                        data[0xFF];
};

enum monitor_state {            
    monitor_version,
    monitor_method,
    monitor_target,
    monitor_dlen,
    monitor_data,
    monitor_ulen,
    monitor_uname,
    monitor_plen,
    monitor_passwd,
    monitor_authenticate,

    // apartir de aca estan done
    monitor_done,

    // y apartir de aca son considerado con error 
    monitor_error,
    monitor_error_unsupported_version,
    monitor_error_unsupported_method,
    monitor_error_unsupported_target,
    monitor_error_unsupported_data,
    monitor_error_unsupported_auth,
};

struct monitor_parser {
    struct monitor *request;
    enum monitor_state state;
    /** cuantos bytes tenemos que leer*/            /*Notemos que hay estados que tiene que leer varios bytes, por eso usamos el i de la estructura del request parser para saber cuantos bytes ya leimos de cada uno de estas estados de la direccion*/
    uint8_t len;
    /** cuantos bytes ya leimos */
    uint8_t i;
    // TODO: mover toda la logica al parser de auth, y delegar aca a ese parser
    uint8_t uname[0xFF];
    uint8_t passwd[0xFF];
};

enum monitor_response_status {
    status_ok = 0x00,
    status_invalid_version = 0x01,
    status_invalid_method = 0x02,
    status_invalid_target = 0x03,
    status_invalid_data = 0x04,
    status_invalid_auth = 0x05,
};

/** inicializa el parser */
void
monitor_parser_init(struct monitor_parser *p);

/**
 * por cada elemento del buffer llama a "monitor_parser_feed" hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 * 
 * param errored parametro de salida. si es diferente de NULL se deja dicho valor
 * si el parsing se debio a una condicion de error
 */
enum monitor_state
monitor_consume(buffer *b, struct monitor_parser *p, bool *errored);

/*
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir 
 * enviando caracteres o no
 * 
 * En caso de haber terminado permite tambien saber si se debe a un error.
 */
bool
monitor_is_done(const enum request_state st, bool *errored);

void
monitor_close(struct monitor_parser *p);

/*
 * serializa en buff una respuesta al request,
 * 
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no habia 
 * espacio suficiente.
 */
extern int
monitor_marshall(buffer *b,
                const enum monitor_response_status status);


/** convierte a errno en socks_response_status */
enum socks_response_status
errno_to_socks(int e);

#endif