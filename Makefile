CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Iinclude
LDFLAGS  ?=

SRC = src/main.cpp src/rng.cpp src/config.cpp src/base_game.cpp src/hold_win.cpp src/sim.cpp
OBJ = $(SRC:src/%.cpp=obj/%.o)
BIN = bin/threepot_sim

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDFLAGS)

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin obj:
	mkdir -p $@

clean:
	rm -rf obj bin
