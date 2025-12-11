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








CXX      = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Iexternal/rpi-rgb-led-matrix/include
LDFLAGS  = -Lexternal/rpi-rgb-led-matrix/lib -lrgbmatrix -lrt -lm -lpthread

all: bin/matrix_demo bin/matrix_daemon bin/local_shader

bin/matrix_demo: src/matrix_demo.cc
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

bin/matrix_daemon: src/matrix_daemon.cc
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

bin/local_shader: src/local_shader.cc
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf bin





bin/udp_matrix_receiver: src/udp_matrix_receiver.cc
	mkdir -p bin
	g++ -std=c++17 -O3 -Wall \
	 -Iexternal/rpi-rgb-led-matrix/include \
	 src/udp_matrix_receiver.cc \
	 -o bin/udp_matrix_receiver \
	 -Lexternal/rpi-rgb-led-matrix/lib \
	 -lrgbmatrix -lrt -lm -lpthread
