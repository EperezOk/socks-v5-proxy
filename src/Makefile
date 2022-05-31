include ./include/Makefile.inc

SOURCES_CLIENT := $(wildcard ./client/*)
SOURCES_SERVER := $(wildcard ./server/*)

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
	
.PHONY: all clean $(TARGET_CLIENT) $(TARGET_SERVER)