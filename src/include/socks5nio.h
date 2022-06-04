#ifndef SOCKS5NIO_H
#define SOCKS5NIO_H

void
socksv5_pool_destroy(void);

void
socksv5_passive_accept(struct selector_key *key);

#endif