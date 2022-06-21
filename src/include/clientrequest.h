#ifndef CLIENTREQUEST_H
#define CLIENTREQUEST_H

#include "clientargs.h"

#define PROGRAM_VERSION             1

#define BASE_REQUEST_DATA           21

// Serialized buffer fields indexes
#define FIELD_VERSION_INDEX         0
#define FIELD_TOKEN_INDEX           1
#define FIELD_METHOD_INDEX          17
#define FIELD_TARGET_INDEX          18
#define FIELD_DLEN_INDEX            19
#define FIELD_DATA_INDEX            21

#define FIELD_VERSION(mem_pos)      mem_pos + FIELD_VERSION_INDEX
#define FIELD_TOKEN(mem_pos)        mem_pos + FIELD_TOKEN_INDEX
#define FIELD_METHOD(mem_pos)       mem_pos + FIELD_METHOD_INDEX
#define FIELD_TARGET(mem_pos)       mem_pos + FIELD_TARGET_INDEX
#define FIELD_DLEN(mem_pos)         mem_pos + FIELD_DLEN_INDEX
#define FIELD_DATA(mem_pos)         mem_pos + FIELD_DATA_INDEX

void
serialize_request(struct client_request_args *args, char *token, char *buffer);

#endif
