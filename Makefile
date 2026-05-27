.PHONY: all clean

SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,build/%.o,$(SRC))

CXXFLAGS = -std=c++14 -D_WIN32_WINNT=0x0601

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
    LIBS = -lz -lpthread
else
    LIBS = -lws2_32 -lz -lpthread
endif

all: ccraft2

ccraft2: $(OBJ) build/sqlite3.o
	g++ $(CXXFLAGS) $(OBJ) build/sqlite3.o -o ccraft2.exe $(LIBS)

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CXXFLAGS) -c $< -o $@

build/sqlite3.o: src/sqlite3.c
	mkdir -p build
	gcc -c src/sqlite3.c -o build/sqlite3.o

clean:
	rm -rf build ccraft2.exe