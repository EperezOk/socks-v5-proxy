# SOCKSv5 Protocol Server

## Introduction

The use of network firewalls, systems that effectively isolate an organizations internal network structure from an exterior network, such as the INTERNET is becoming increasingly popular.  These firewall systems typically act as application-layer gateways between networks, usually offering controlled TELNET, FTP, and SMTP access. With the emergence of more sophisticated application layer protocols designed to facilitate global information discovery, there exists a need to provide a general framework for these protocols to transparently and securely traverse a firewall.

There exists, also, a need for strong authentication of such traversal in as fine-grained a manner as is practical. This requirement stems from the realization that client-server relationships emerge between the networks of various organizations, and that such relationships need to be controlled and often strongly authenticated.

SOCKSv5 Protocol is designed to provide a framework for client-server applications in both the TCP and UDP domains to conveniently and securely use the services of a network firewall. The protocol is conceptually a "shim-layer" between the application layer and the transport layer, and as such does not provide network-layer gateway services, such as forwarding of ICMP messages.

For more information about this protocol: [RFC 1928](https://datatracker.ietf.org/doc/html/rfc1928)

This repository consists in the implementation of a proxy server for this protocol which MUST contain:

 - Serve multiple clients simultaneosly
 - Support user/password authentication according to RFC1929
	 - More information in: [RFC 1929](https://datatracker.ietf.org/doc/html/rfc1929)
 - Support outgoing connections to TCP services, IPv4 and IPv6 addresses or using FQDN in order to resolve these addresses.
 - Report bugs to clients
 - Implement mechanisms to collect metrics in order to monitor system operation (these metrics can be volatile)
	 - Historical amount of connections
	 - Concurrent amount of connections
	 - Amount of bytes transfered
	 -  
 - Implement mechanisms that allow managing users and change server configuration in runtime
 - Implement an access log that allows an administrator to understand each user access.
 - Monitor traffic and generate an access credential log like ettercap for at least POP3.

## Usage

To compile both the server and client run "make" or "make all". Both will be generated on the root folder with the names of "socks5d" for the server and "client" for the client.

To get more information about the options of both run them with the flag "-h".

## Files

This project's report is located on the root folder, on the file "Informe.pdf".

The RFC of the monitor protocol designed, "RFC protocolo monitoreo.pdf", is also located on the root folder. This was included in the project to make clear each of the protocol's options, on both requests and responses, along with some examples on how to operate with it.

## Authors
- 61490 - Baliarda Gonzalo
- 61475 - Perez Ezequiel
- 61011 - Ye Li Valentín
- 61482 - Birsa Nicolás