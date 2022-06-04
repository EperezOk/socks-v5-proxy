SOURCES_FOLDER = src

all:
	cd $(SOURCES_FOLDER); make $@

client:
	cd $(SOURCES_FOLDER); make $@

server:
	cd $(SOURCES_FOLDER); make $@

clean:
	cd $(SOURCES_FOLDER); make $@

proxy:
	cd $(SOURCES_FOLDER); make $@

cleanp:
	cd $(SOURCES_FOLDER); make $@

.PHONY: all clean client server proxy