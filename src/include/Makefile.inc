COMPILER := gcc
CFLAGS := -Wall -g -fsanitize=address -pthread
OBJECT_CLIENT := ./client.c
OBJECT_SERVER := ./server.c
