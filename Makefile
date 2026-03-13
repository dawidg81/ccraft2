.PHONY: all clean

SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,build/%.o,$(SRC))

CXXFLAGS = -std=c++14 -D_WIN32_WINNT=0x0601

# Detect Windows
ifeq ($(OS),Windows_NT)
    MKDIR = if not exist build mkdir build
    RM = rmdir /s /q build & del /q ccraft2.exe
else
    MKDIR = mkdir -p build
    RM = rm -rf build ccraft2
endif

all: ccraft2

ccraft2: $(OBJ)
	g++ $(CXXFLAGS) $(OBJ) -o ccraft2 -lws2_32 -lz -lpthread

build/%.o: src/%.cpp
	$(MKDIR)
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	$(RM)
