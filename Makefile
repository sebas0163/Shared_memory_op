# Variables
CREATOR_BIN = creator
CREATOR_SRC = creator.c
GCC = gcc
CFLAGS = -lrt
MEMORY_SIZE = 4096  # Set default memory size in bytes
CONSTANTS = constants.h

# Default target
all: $(CREATOR_BIN)

$(CREATOR_BIN): $(CREATOR_SRC) $(CONSTANTS) 
	$(GCC) $(CFLAGS) $< -o $@

# Phony targets for cleaning up and running
.PHONY: clean run

clean:
	rm -f $(CREATOR_BIN)

run: all
	./$(CREATOR_BIN) $(MEMORY_SIZE)

