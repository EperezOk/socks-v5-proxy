#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "buffer.h"

void bufferReset(buffer *b) {
    b->read  = b->data;
    b->write = b->data;
}

void bufferInit(buffer *b, const size_t n, uint8_t *data) {
    b->data = data;
    bufferReset(b);
    b->limit = b->data + n;
}

bool bufferCanWrite(buffer *b) {
    return bufferFreeSpace(b) > 0;
}

size_t bufferWrite(buffer *b, const void *src, size_t nbyte) {
    assert(b->write <= b->limit);
    size_t freeSpace = bufferFreeSpace(b);
    if (freeSpace < nbyte) nbyte = freeSpace;
    memcpy(b->write, src, nbyte);
    advanceWritePtr(b, nbyte);
    return nbyte;
}

void bufferWriteByte(buffer *b, uint8_t c) {
    if(bufferCanWrite(b)) {
        *b->write = c;
        advanceWritePtr(b, 1);
    }
}

void advanceWritePtr(buffer *b, const ssize_t bytes) {
    if(bytes > -1) {
        b->write += (size_t) bytes;
        assert(b->write <= b->limit);
    }
}

size_t bufferFreeSpace(buffer *b) {
    return b->limit - b->write;
}

uint8_t* getWritePtr(buffer *b) {
    assert(b->write <= b->limit);
    return b->write;
}

bool bufferCanRead(buffer *b) {
    return bufferPendingRead(b) > 0;
}

size_t bufferRead(buffer *b, void *dest) {
    assert(b->read <= b->write);
    size_t nbyte = bufferPendingRead(b);
    memcpy(dest, b->read, nbyte);
    advanceReadPtr(b, nbyte);
    return nbyte;
}

uint8_t bufferReadByte(buffer *b) {
    uint8_t ret;
    if(bufferCanRead(b)) {
        ret = *b->read;
        advanceReadPtr(b, 1);
    } else {
        ret = 0;
    }
    return ret;
}

size_t bufferPendingRead(buffer *b) {
    return b->write - b->read;
}

uint8_t* getReadPtr(buffer *b) {
    assert(b->read <= b->write);
    return b->read;
}

static void bufferCompact(buffer *b) {
    if(b->data == b->read) {
        // nada por hacer
    } else if(b->read == b->write) {
        b->read  = b->data;
        b->write = b->data;
    } else {
        const size_t n = b->write - b->read;
        memmove(b->data, b->read, n);
        b->read  = b->data;
        b->write = b->data + n;
    }
}

void advanceReadPtr(buffer *b, const ssize_t bytes) {
    if(bytes > -1) {
        b->read += (size_t) bytes;
        assert(b->read <= b->write);

        if(b->read == b->write) {
            // compactacion poco costosa
            bufferCompact(b);
        }
    }
}
