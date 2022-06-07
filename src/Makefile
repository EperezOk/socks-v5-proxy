include ./include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./client/*.c)
SOURCES_SERVER := $(wildcard ./server/*.c)

OBJECTS_CLIENT := $(OBJECT_CLIENT:.c=.o) $(SOURCES_CLIENT:.c=.o)
OBJECTS_SERVER := $(OBJECT_SERVER:.c=.o) $(SOURCES_SERVER:.c=.o)
OBJECTS = $(OBJECTS_SERVER) $(OBJECTS_CLIENT)

TARGETS := $(wildcard ../*.out)
TARGET_CLIENT := client
TARGET_SERVER := server

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_CLIENT): $(OBJECTS_CLIENT)
	$(COMPILER) $(CFLAGS) $^ -o ../$@.out

$(TARGET_SERVER): $(OBJECTS_SERVER)
	$(COMPILER) $(CFLAGS) $^ -o ../$@.out

%.o: %.c
	$(COMPILER) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGETS)

# TODO: DELETE AFTER TRANSPARENT PROXY IS DELETED
proxy: trnsProxy.o ./transp-proxy/buffer.o
	$(COMPILER) $(CFLAGS) $^ -o ../$@.out -pthread

cleanp:
	make clean;
	rm -f ./trnsProxy.o ../proxy.out ./transp-proxy/buffer.o;

.PHONY: all clean $(TARGET_CLIENT) $(TARGET_SERVER)