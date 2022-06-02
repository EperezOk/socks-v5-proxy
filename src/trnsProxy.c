/**
    Handle multiple socket connections with select and fd_set on Linux
*/
  
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include "include/buffer.h"

#define TRUE   1
#define FALSE  0
#define PORT 8080
#define MAX_CLIENTS 500
#define BUFF_SIZE 1024
#define WELCOME_MSG "Transparent Proxy v1.0 \r\n"

typedef struct client {
    int fd;
    struct buffer writeBuffController;
    uint8_t writeBuff[BUFF_SIZE];
    struct buffer readBuffController;
    uint8_t readBuff[BUFF_SIZE];
} client;

int main(int argc , char *argv[]) {
    int opt = TRUE;
    int master_socket, addrlen, new_socket, activity, i, valread, sd;

    client client_socket[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS; i++)
        client_socket[i].fd = 0;

    // create a master socket - socket pasivo
    // type: SOCK_STREAM | SOCK_NONBLOCK para hacerlo no bloqueante
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
  
    // set master socket to allow multiple connections , this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
  
    // type of socket created
    struct sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // aceptar cualquier direccion ip y puerto
    address.sin_port = htons(PORT);
      
    // bind the socket to localhost port 8080
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d \n", PORT);
     
    // try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
      
    // accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
     
    // set of socket descriptors
    fd_set readfds;
    fd_set writefds;

    while(TRUE) {
        // clear the socket set
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        // add master socket to set
        FD_SET(master_socket, &readfds);
        int max_sd = master_socket;
         
        // check if there are child sockets to set and set them
        for (i = 0; i < MAX_CLIENTS; i++) {
            // socket descriptor
            sd = client_socket[i].fd;
             
            // if valid socket descriptor then add to read list
            if(sd > 0) {
                //if (buffer_can_write(&client_socket[i].readBuffController))
                    FD_SET(sd, &readfds);
                //if (buffer_can_read(&client_socket[i].writeBuffController))
                    FD_SET(sd, &writefds);
            }
             
            // highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }
  
        // wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(max_sd + 1, &readfds, &writefds, NULL, NULL);
    
        if ((activity < 0) && (errno!=EINTR))
            printf("select error");
          
        // If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            // TODO: hacer no bloqueante?
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
          
            // inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
            // send new connection greeting message
            if (send(new_socket, WELCOME_MSG, strlen(WELCOME_MSG), 0) != strlen(WELCOME_MSG)) 
                perror("send");
              
            puts("Welcome message sent successfully");
              
            // add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                // if position is empty
                if (client_socket[i].fd == 0) {
                    client_socket[i].fd = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);
                    break;
                }
            }
        }
          
        // else its some IO operation on some other socket :)
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i].fd;
              
            if (FD_ISSET(sd, &readfds)) {
                buffer *br = &client_socket[i].readBuffController;
                size_t wbytes = 0;
                uint8_t *read_ptr = buffer_write_ptr(br, &wbytes);
                // check if it was for closing
                if ((valread = read(sd, read_ptr, wbytes)) == 0) {
                    // somebody disconnected, get his details and print
                    getpeername(sd , (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                      
                    // close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i].fd = 0;
                }
                // else echo back the message that came in
                else {
                    buffer_write_adv(br, valread);
                    /*
                    Paso 1:
                    readBuffer:  4 5 6 - -
                    writeBuffer: - - - - -
                    
                    Paso 2:
                    readBuffer:  - - - - -
                    writeBuffer: 4 5 6 - -

                    Paso 3:
                    readBuffer:  3 - - - -
                    writeBuffer: 4 5 6 1 2
                    */
                    size_t rbytes = 0;
                    read_ptr = buffer_read_ptr(br, &rbytes);

                    // CHECK: guardar cosas en el writeBuffer en este momento?
                    buffer *bw = &client_socket[i].writeBuffController;
                    uint8_t *write_ptr = buffer_write_ptr(bw, &wbytes);
                    size_t writtenBytes = rbytes > wbytes ? wbytes : rbytes;
                    memcpy(write_ptr, read_ptr, writtenBytes);
                    buffer_read_adv(br, writtenBytes);
                    buffer_write_adv(bw, writtenBytes);
                }
            }

            if (FD_ISSET(sd, &writefds)) {
                // PARA NO BLOQUEAR ACA, EL SELECT DEBERIA CHECKEAR TAMBIEN SOCKETS DE ESCRITURA (3er param, ademas de los de lectura). Para poder hacer eso y pasar a la siguiente ronda sin hacer el send en esta y haciendolo en la proxima, cada socket de escritura necesitara un buffer para mandar el send() en la proxima ronda
                buffer *bw = &client_socket[i].writeBuffController;
                size_t writeRbytes = 0; // lo que lei del write buffer
                uint8_t *read_ptr = buffer_read_ptr(bw, &writeRbytes);

                send(sd, read_ptr, writeRbytes, 0); // OJO: ESTO PODRIA BLOQUEARSE si el cliente no consume y se llena el buffer de salida

                buffer_read_adv(bw, writeRbytes);

                buffer *br = &client_socket[i].readBuffController;
                size_t readRbytes = 0; //  lo que puedo leer nuevo
                read_ptr = buffer_read_ptr(br, &readRbytes); //
                
		size_t readBytes;
                uint8_t *write_ptr = buffer_write_ptr(bw, &readBytes); // cuanto puedo escribir
		readBytes = readBytes > readRbytes ? readRbytes : readBytes;             

		memcpy(write_ptr, read_ptr, readBytes);

                buffer_read_adv(br, readBytes);
                buffer_write_adv(bw, readBytes);
            }
        }
    }
      
    return 0;
}
