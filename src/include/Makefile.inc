COMPILER := gcc
CFLAGS := -Wall -g -std=gnu11 -pedantic -fsanitize=address -pthread
OBJECT_CLIENT := ./client.c
OBJECT_SERVER := ./server.c
