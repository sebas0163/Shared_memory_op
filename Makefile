# Variables
CREATOR_BIN = creator
CREATOR_SRC = creator.c
CLIENT_BIN = client
CLIENT_SRC = client.c
RECREATOR_BIN = recreator
RECREATOR_SRC = recreator.c
STADISTICS_BIN = stadistics
STADISTICS_SRC = stadistics.c
GCC = gcc
CFLAGS = -lrt
MEMORY_SIZE = 8  	# Set default memory size in bytes
CONSTANTS = constants.h
TEXT_FILE = lorem.txt 	# Set default text file to read
MODE = 1 	#0 = manual mode, 1 = automatic mode
PERIOD = 500	#miliseconds transcurred between every client read/write

# Default target
all: $(CREATOR_BIN) $(CLIENT_BIN) $(RECREATOR_BIN)

$(CLIENT_BIN): $(CLIENT_SRC) $(CONSTANTS)
	$(GCC) $(CFLAGS) $< -o $@

$(RECREATOR_BIN): $(RECREATOR_SRC) $(CONSTANTS)
	$(GCC) $(CFLAGS) $< -o $@

$(CREATOR_BIN): $(CREATOR_SRC) $(CONSTANTS) 
	$(GCC) $(CFLAGS) `pkg-config --cflags gtk+-3.0` -o $@ $< `pkg-config --libs gtk+-3.0`

$(STADISTICS_BIN): $(STADISTICS_SRC) $(CONSTANTS) 
	$(GCC) $(CFLAGS) $< -o $@
# Phony targets for cleaning up and running
.PHONY: clean run_creator run_client run_stadistics

clean:
	rm -f $(CREATOR_BIN) $(CLIENT_BIN) $(RECREATOR_BIN)

run_creator:
	./$(CREATOR_BIN) $(MEMORY_SIZE)

run_client:
	./$(CLIENT_BIN) $(TEXT_FILE) $(MEMORY_SIZE) $(MODE) $(PERIOD)

run_recreator:
	./$(RECREATOR_BIN) $(TEXT_FILE) $(MEMORY_SIZE) $(MODE) $(PERIOD)
run_stadistics:
	./$(STADISTICS_BIN)