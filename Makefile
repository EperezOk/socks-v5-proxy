include ./src/include/Makefile.inc

SOURCES_FOLDER = src

all:
	cd $(SOURCES_FOLDER); make $@

$(TARGET_CLIENT):
	cd $(SOURCES_FOLDER); make $@

$(TARGET_SERVER):
	cd $(SOURCES_FOLDER); make $@

clean:
	cd $(SOURCES_FOLDER); make $@
	rm -rf $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean

# TODO: delete these
proxy:
	cd $(SOURCES_FOLDER); make $@

cleanp:
	cd $(SOURCES_FOLDER); make $@
