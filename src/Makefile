include ./include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./client/*.c)
SOURCES_SERVER := $(wildcard ./server/*.c)
SOURCES_COMMON := $(wildcard ./utils/*.c)

TARGETS := $(wildcard ../*.out)
TARGET_CLIENT := client
TARGET_SERVER := server

OBJECTS_CLIENT := $(TARGET_CLIENT).o $(SOURCES_CLIENT:.c=.o)
OBJECTS_SERVER := $(TARGET_SERVER).o $(SOURCES_SERVER:.c=.o)
OBJECTS_COMMON := $(SOURCES_COMMON:.c=.o)
OBJECTS = $(OBJECTS_SERVER) $(OBJECTS_CLIENT) $(OBJECTS_COMMON)

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_CLIENT): $(OBJECTS_CLIENT) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o ../$@.out

$(TARGET_SERVER): $(OBJECTS_SERVER) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o ../$@.out

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGETS)

.PHONY: all clean $(TARGET_CLIENT) $(TARGET_SERVER)

# TODO: DELETE FROM HERE ONCE TRANSPARENT PROXY IS DELETED
SOURCES_PROXY := $(wildcard ./transp-proxy/*.c)
TARGET_PROXY := proxy
OBJECTS_PROXY := $(TARGET_PROXY).o $(SOURCES_PROXY:.c=.o)

$(TARGET_PROXY): $(OBJECTS_PROXY)
	$(CC) $(CFLAGS) $^ -o ../$@.out

cleanp:
	make clean;
	rm -rf $(OBJECTS_PROXY) $(TARGET_PROXY).out;
