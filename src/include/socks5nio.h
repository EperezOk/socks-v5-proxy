#ifndef SOCKS5NIO_H
#define SOCKS5NIO_H

#include <netdb.h>
#include "selector.h"

#define MAX_USERS 10

/** handler del socket pasivo que atiende conexiones socksv5 */
void socksv5_passive_accept(struct selector_key *key);

/** especifica la lista de users que pueden usar el servidor proxy
 * retorna 0 si anduvo todo bien
 * retorna -1 si el usuario ya esta registrado
 * retorna 1 si se alcanzo el limite de usuarios
 */
int socksv5_register_user(char *uname, char *passwd);

int socksv5_unregister_user(char *uname);

/** prende/apaga el disector de passwords pop3 */
void socksv5_toggle_disector(bool to);

/** libera pools internos */
void socksv5_pool_destroy(void);

/** consultar estadisticas del servidor */
size_t socksv5_historic_connections();
size_t socksv5_current_connections();
size_t socksv5_bytes_transferred();
size_t socksv5_get_users(char *unames[MAX_USERS]);

#endif