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

ccraft2: $(OBJ)
	g++ $(CXXFLAGS) $(OBJ) -o ccraft2.exe $(LIBS)

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build ccraft2.exe
