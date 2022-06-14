#ifndef SOCKS5NIO_H
#define SOCKS5NIO_H

#include <netdb.h>
#include "selector.h"

#define MAX_USERS 10

/** handler del socket pasivo que atiende conexiones socksv5 */
void socksv5_passive_accept(struct selector_key *key);

/** especifica la lista de users que pueden usar el servidor proxy */
void socksv5_register_user(char *uname, char *passwd);

/** libera pools internos */
void socksv5_pool_destroy(void);

#endif