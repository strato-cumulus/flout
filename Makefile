BUILD_DIR = build
BIN_DIR = bin

OBJECT_SOURCES = $(wildcard */*.c)
OBJECTS = $(addsuffix .o, $(basename $(OBJECT_SOURCES)))
ALL_OBJECT_SOURCES = $(wildcard **/*.c)
ALL_OBJECTS = $(basename $(OBJECT_SOURCES))

ENTRYPOINTS = $(wildcard *.c)
BINARIES = $(basename $(ENTRYPOINTS))

all: $(ALL_OBJECTS) $(BINARIES) clean

$(ALL_OBJECTS):
	mkdir -p $(BUILD_DIR)/$(shell dirname $@.c)
	gcc $@.c -c -o $(BUILD_DIR)/$@.o

$(BINARIES): $(ENTRYPOINTS)
	mkdir -p $(BIN_DIR)
	gcc $@.c $(addprefix $(BUILD_DIR)/, $(OBJECTS)) -o $(BIN_DIR)/$@ -lm -lpthread

clean: $(ALL_OBJECTS)
	rm -rf $(BUILD_DIR)
