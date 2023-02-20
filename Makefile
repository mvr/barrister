CC = clang++
CFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 -march=native -mtune=native -flto -fno-stack-protector -fomit-frame-pointer -g
LDFLAGS =

# CC = /usr/local/opt/llvm/bin/clang++
# LDFLAGS=-L/usr/local/opt/llvm/lib/c++ -Wl,-rpath,/usr/local/opt/llvm/lib/c++

all: Barrister

Barrister: Barrister.cpp LifeAPI.h
	$(CC) $(CFLAGS) -o Barrister Barrister.cpp $(LDFLAGS)
