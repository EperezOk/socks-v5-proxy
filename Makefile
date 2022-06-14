include ./src/include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./src/client/*.c)
SOURCES_SERVER := $(wildcard ./src/server/*.c)
SOURCES_COMMON := $(wildcard ./src/utils/*.c)

OBJECTS_CLIENT := ./src/$(TARGET_CLIENT).o $(SOURCES_CLIENT:.c=.o)
OBJECTS_SERVER := ./src/server.o $(SOURCES_SERVER:.c=.o)
OBJECTS_COMMON := $(SOURCES_COMMON:.c=.o)
OBJECTS = $(OBJECTS_SERVER) $(OBJECTS_CLIENT) $(OBJECTS_COMMON)

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_CLIENT): $(OBJECTS_CLIENT) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@

$(TARGET_SERVER): $(OBJECTS_SERVER) $(OBJECTS_COMMON)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean
