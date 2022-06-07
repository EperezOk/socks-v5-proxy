/**
 * socks5.c -- Implementacion de forma bloqueante un proxy SOCKS5 
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>

#include "socks5.h"
#include "hello.h"
#include "request.h"
#include "netutils.h"


//... codigo que no anote de pthreads


// Este metodo se lo pasamos desde el server.c como puntero a funcion al parser
/* Notemos que el selected por defecto esta con 255 inicializada en el parser, 
que significa no nos pusimos de acuerdo.*/
/** callback del parser utilizado en 'read_hello' */
static void
on_hello_method(struct hello_parser *p, const uint8_t method) {
    uint8_t *selected = p->data;

    if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method) {
        *selected = method;
    }

    //Aca deberia estar la otra opcion de AUTENTICATION que seria 0x01
}




/**
 * lee e interpreta la trama "hello" que arriba por fd
 * 
 * return true ante un error
 */ 

static bool
read_hello(const int fd, const uint8_t *method) {
    // 0, lectura del primer hello
    uint8_t buff[256 + 1 + 1];
    buffer buffer; buffer_init(&buffer, N(buff), buff);
    struct hello_parser hello_parser = {
        .data                     = (void*)method, //En data esta guardada la direccion de una variable sobre que metodo eligio el proxy para responder de todas las que mostro el cliente. Inicialmente debe estar en 0xFF que se pasa como parametro
        .on_authentication_method = on_hello_method,  //le pasamos el callback que definimos aca arriba
    };
    hello_parser_init(&hello_parser);
    bool error = false;
    size_t buffsize;
    ssize_t n;
    do {
        uint8_t *ptr = buffer_write_ptr(&buffer, &buffersieze);
        n = recv(fd, ptr, buffsize, 0);
        if (n > 0) {
            buffer_write_adv(&buffer, n);
            const enum hello_state st = hello_consume(&buffer, &hello_parser, &error);
            if(hello_is_done(st, &error)) {
                break;
            }
        } else {
            break;
        }
        
    } while (true);
    
    if(!hello_is_done(hello_parser.state, &error)) {
        error = true;
    }
    hello_parser_close(&hello_parser);
    return error;
}


/**
 * lee e itnerpreta la trama "request" que arriba por fd
 * 
 * return true ante un error
 */
static bool
read_request(const int fd, struct request *request) {
    uint8_t buff[22];
    buffer buffer; buffer_init(&buffer, N(buff), buff);
}