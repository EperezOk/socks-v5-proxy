#ifndef MONITORNIO_H
#define MONITORNIO_H

#include "selector.h"

#define MAX_ADMINS 3

/** handler del socket pasivo que atiende conexiones del protocolo de monitoreo */
void monitor_passive_accept(struct selector_key *key);

/** especifica la lista de users admin que pueden usar el protocolo de monitoreo
 * retorna 0 si anduvo todo bien
 * retorna -1 si el usuario ya esta registrado
 * retorna 1 si se alcanzo el limite de usuarios
 */
int monitor_register_admin(char *uname, char *token);

/** libera pools internos */
void connection_pool_destroy(void);

#endif