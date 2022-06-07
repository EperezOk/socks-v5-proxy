#ifndef HELLO_H
#define HELLO_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"

static const uint8_t METHOD_NO_AUTHENTICATION_REQUIRED = 0x00;
static const uint8_t METHOD_NO_ACCEPTABLE_METHODS = 0x00;

/**
	The client connects to the server, and sends a version
	identifier/method selection message:

			+----+----------+----------+
			|VER | NMETHODS | METHODS  |
			+----+----------+----------+
			| 1  |    1     | 1 to 255 |
			+----+----------+----------+

	The VER field is set to X'05' for this version of the protocol.  The
	NMETHODS field contains the number of method identifier octets that
	appear in the METHODS field.
*/



enum hello_state {
	hello_version,
	/*Debemos leer la cant de metodos*/
	hello_nmethods, 
	/* Nos encontramosl eyendo los metodos */
	hello_methods, 
	hello_done,
	hello_error_unsupported_version,
};

struct hello_parser {
	/** invocando cada vez que se presenta un nuevo metodo */
	void (*on_authentication_method)
		(struct hello_parser *parser, const uint8_t method);

	/** permite al usuario del parser almacenar sus datos */
	void *data;
	/****** zona privada ******/
	enum hello_state state;
	/* metodos que faltan por leer */
	uint8_t remaining;
};

/** inicializa el parser */
void hello_parser_init (struct hello_parser *p);

/** entrega un byte al parser, retorna true si se llego al final */
enum hello_state hello_parser_feed (struct hello_parser *p, uint8_t b);

/*
 * por cada elemento del buffer llama a "hello_parser_feed" hasta que 
 * el parseo se encuentra completo o se requieren mas bytes.
 * 
 * param errored parametro de salida. si es diferente de NULL se deja dicho valor
 * si el parsing se debio a una condicion de error
 */
enum hello_state
hello_consume(buffer *b, struct hello_parser *p, bool *errored);

/*
 * Permite distinguir a quien usa hello_parser_feed si debe seguir 
 * enviando caracteres o no.
 * 
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool 
hello_is_done(const enum hello_state state, bool *errored);

/*
 * En caso de que se haya llegado a un estado de error, permite obtener
 * representacion textual que describe el problema
 */
extern const char *
hello_error(const struct hello_parser *p);


/* Libera recursos internos del parser */
void hello_parser_close(struct hello_parser *p);

static const uint8_t SOCKS_HELLO_NOAUTHENTICATION_REQUIRED = 0x00;	/* Faltaria definir un AUTHENTICATION REQUIRED que seria 0x01 */
/*
 * If the selected METHOD is X'FF', none of the methods listed by the
 * client are acceptable, and the client MUST close the connection.
 */
static const uint8_t SOCKS_HELLO_NO_ACCEPTABLE_METHODS = 0xFF;


/*
 * serializa en buff una respuesta al hello.
 * 
 * Retorna la cant de bytes del buffer o -1 si no habia
 * espacio sufuciente.
 */ 
int 
hello_marsharll(buffer *b, const uint8_t method);

#endif