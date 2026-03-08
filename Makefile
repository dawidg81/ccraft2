.PHONY: all clean

SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,build/%.o,$(SRC))

all: mcc

mcc: $(OBJ)
	g++ $(OBJ) -o mcc -lpthread -lws2_32

build/%.o: src/%.cpp
	mkdir -p build
	g++ -c $< -o $@

clean:
	rm -rf build
	rm -f mcc.exe mcc
