RGB_LIB_DIR := external/rpi-rgb-led-matrix

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -I$(RGB_LIB_DIR)/include
LDFLAGS  := -L$(RGB_LIB_DIR)/lib -lrgbmatrix -lrt -lm -lpthread

BIN_DIR  := bin
SRC_DIR  := src

all: $(BIN_DIR)/matrix_demo $(BIN_DIR)/matrix_daemon

$(BIN_DIR)/matrix_demo: $(SRC_DIR)/matrix_demo.cc
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/matrix_daemon: $(SRC_DIR)/matrix_daemon.cc
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean

