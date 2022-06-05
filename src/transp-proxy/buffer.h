#ifndef BUFFER_H_VelRDAxzvnuFmwEaR0ftrkIinkT
#define BUFFER_H_VelRDAxzvnuFmwEaR0ftrkIinkT

#include <stdbool.h>
#include <unistd.h>

/**
 * buffer.c - buffer con acceso directo (útil para I/O) que mantiene
 *            mantiene puntero de lectura y de escritura.
 *
 *
 * Para esto se mantienen dos punteros, uno de lectura
 * y otro de escritura, y se provee funciones para
 * obtener puntero base y capacidad disponibles.
 *
 * R=0
 * ↓
 * +---+---+---+---+---+---+
 * |   |   |   |   |   |   |
 * +---+---+---+---+---+---+
 * ↑                       ↑
 * W=0                     limit=6
 *
 * Invariantes:
 *    R <= W <= limit
 *
 * Se quiere escribir en el bufer cuatro bytes.
 *
 * R=0
 * ↓
 * +---+---+---+---+---+---+
 * | H | O | L | A |   |   |
 * +---+---+---+---+---+---+
 *                 ↑       ↑
 *                W=4      limit=6
 *
 * Quiero leer 3 del buffer
 *
 *            R=3
 *             ↓
 * +---+---+---+---+---+---+
 * | H | O | L | A |   |   |
 * +---+---+---+---+---+---+
 *                 ↑       ↑
 *                W=4      limit=6
 *
 * Quiero escribir 2 bytes mas
 *
 *            R=3
 *             ↓
 * +---+---+---+---+---+---+
 * | H | O | L | A |   | M |
 * +---+---+---+---+---+---+
 *                         ↑
 *                         limit=6
 *                         W=4
 * Compactación a demanda
 * R=0
 * ↓
 * +---+---+---+---+---+---+
 * | A |   | M |   |   |   |
 * +---+---+---+---+---+---+
 *             ↑           ↑
 *            W=3          limit=6
 *
 * Leo los tres bytes, como R == W, se auto compacta.
 *
 * R=0
 * ↓
 * +---+---+---+---+---+---+
 * |   |   |   |   |   |   |
 * +---+---+---+---+---+---+
 * ↑                       ↑
 * W=0                     limit=6
 */

typedef struct buffer {
    /** posicion de memoria del extremo izquierdo del buffer */
    uint8_t *data;

    /** límite superior del buffer. inmutable */
    uint8_t *limit;

    /** puntero de lectura */
    uint8_t *read;

    /** puntero de escritura */
    uint8_t *write;
} buffer;

/**
 * inicializa el buffer sin utilizar el heap
 */
void bufferInit(buffer *b, const size_t n, uint8_t *data);

/**
 * Recibe un buffer y la cantidad de bytes a escribir (deseables).
 * Retorna la cantidad de bytes que pudieron escribirse.
 */
size_t bufferWrite(buffer *b, const void *src, size_t nbyte);

/**
 * Recibe un buffer y lee todo lo posible hacia el mismo.
 * Retorna la cantidad de bytes leidos.
 */
size_t bufferRead(buffer *b, void *dest);

/**
 * obtiene un byte
 */
uint8_t bufferReadByte(buffer *b);

uint8_t* getReadPtr(buffer *b);

uint8_t* getWritePtr(buffer *b);

/** escribe un byte */
void bufferWriteByte(buffer *b, uint8_t c);

/** avanza el puntero de escritura */
void advanceWritePtr(buffer *b, const ssize_t bytes);

/** avanza el puntero de lectura */
void advanceReadPtr(buffer *b, const ssize_t bytes);

/**
 * Reinicia todos los punteros
 */
void bufferReset(buffer *b);

/** retorna true si hay bytes para leer del buffer */
bool bufferCanRead(buffer *b);

/** retorna true si se pueden escribir bytes en el buffer */
bool bufferCanWrite(buffer *b);

/** retorna la cantidad de bytes a leer en el buffer */
size_t bufferPendingRead(buffer *b);

/** retorna la cantidad de bytes disponibles para escribir en el buffer */
size_t bufferFreeSpace(buffer *b);

#endif
