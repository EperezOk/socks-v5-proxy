# SOCKSv5 Protocol Server

## Introduction

This repository consists in a C-based, **non-blocking** implementation of the SOCKSv5 Protocol, and contains the following features:
 - Serve multiple clients simultaneously and in an **event-driven** fashion.
 - Support user/password authentication according to [RFC 1929](https://datatracker.ietf.org/doc/html/rfc1929).
 - Support outgoing connections to TCP services, being able to connect to IPv4/IPv6 addresses or FQDN's.
 - Report bugs to clients.
 - Implement mechanisms to collect metrics in order to monitor system operation:
	 - Historical amount of connections.
	 - Concurrent amount of connections.
	 - Amount of bytes transferred.
 - Implement mechanisms that allow managing users and changing server configuration during runtime.
 - Implement an access log that allows an administrator to understand each user access.
 - Monitor traffic and generate an access credential log like ettercap for at least POP3.

SOCKSv5 Protocol is designed to provide a framework for client-server applications in both the TCP and UDP domains to conveniently and securely use the services of a network firewall. The protocol is conceptually a "shim-layer" between the application layer and the transport layer, and as such does not provide network-layer gateway services, such as forwarding of ICMP messages.

For more information about this protocol: [RFC 1928](https://datatracker.ietf.org/doc/html/rfc1928).

## Usage

To compile both the server and client run "make" or "make all" on the root folder of the project.

```sh
user@USER:~/socksv5-protocol$ make all
```

Both will be generated on the root folder with the names of "socks5d" for the server and "client" for the client.

To get more information about the options of both run them with the flag "-h". Below there is an extract of both commands' help page.

```sh
user@USER:~/socksv5-protocol$ ./socks5d -h
Usage: ./socks5d [OPTION]...
   -h              Imprime la ayuda y termina.
   -l<SOCKS addr>  Dirección donde servirá el proxy SOCKS. Por defecto escucha en todas las interfaces.
   -N              Deshabilita los passwords disectors.
   -L<conf  addr>  Dirección donde servirá el servicio de management. Por defecto escucha solo en loopback.
   -p<SOCKS port>  Puerto TCP para conexiones entrantes SOCKS. Por defecto es 1080.
   -P<conf  port>  Puerto TCP para conexiones entrantes del protocolo de configuracion. Por defecto es 8080.
   -u<user>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.
   -v              Imprime información sobre la versión y termina.
```

```sh
user@USER:~/socksv5-protocol$ ./client -h
Usage: ./client [OPTIONS]... TOKEN [DESTINATION] [PORT]
Options:
-h                  imprime los comandos del programa y termina.
-c                  imprime la cantidad de conexiones concurrentes del server.
-C                  imprime la cantidad de conexiones históricas del server.
-b                  imprime la cantidad de bytes transferidos del server.
-a                  imprime una lista con los usuarios del proxy.
-A                  imprime una lista con los usuarios administradores.
-n                  enciende el password disector en el server.
-N                  apaga el password disector en el server.
-u <user:pass>      agrega un usuario del proxy con el nombre y contraseña indicados.
-U <user:token>     agrega un usuario administrador con el nombre y token indicados.
-d <user>           borra el usuario del proxy con el nombre indicado.
-D <user>           borra el usuario administrador con el nombre indicado.
-v                  imprime la versión del programa y termina.
````

## Files

This project's report is located on the root folder, on the file "report.pdf".

The RFC of the monitor protocol designed, "RFC-monitor-protocol-es.pdf", is also located on the root folder. This was included in the project to make clear each part of the protocol's options, on both requests and responses, along with some examples on how to operate it.

Also included on the root folder is a man page, which explains all of the server options. To open it, run "man ./socks5d.8" on the root folder of the project.

## Authors
- 61490 - Baliarda Gonzalo
- 61475 - Perez Ezequiel
- 61011 - Ye Li Valentín
- 61482 - Birsa Nicolás
