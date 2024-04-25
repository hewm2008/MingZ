BIN_DIR := $(shell pwd)/bin
LIB_DIR := $(shell pwd)/lib
SRC_DIR := $(shell pwd)/src
INCLUDE_DIR := $(shell pwd)/include

OS := $(shell uname -s)

.PHONY: all clean

all: $(BIN_DIR)/mingz

$(BIN_DIR)/mingz: $(SRC_DIR)/mingz.cpp | $(BIN_DIR)
ifeq ($(OS),Darwin)
	g++ --std=c++11 -g -O3 $< -lz -ldeflate -pthread -I$(INCLUDE_DIR) -o $@  -lz
else
	g++ -L$(LIB_DIR) -Wl,-rpath=$(LIB_DIR) --std=c++11 -g -O3 $< -lz -ldeflate -pthread -I$(INCLUDE_DIR) -o $@
endif

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
