CC = clang++
CFLAGS = -std=c++11 -Wall -Wextra -pedantic -O3 -march=native -mtune=native -flto -fno-stack-protector -fomit-frame-pointer -g
# CFLAGS = -std=c++11 -Wall -Wextra -pedantic -g -fno-stack-protector -fomit-frame-pointer
LDFLAGS =

all: Prop

Prop: Prop.cpp LifeAPI.h
	$(CC) $(CFLAGS) -o Prop Prop.cpp $(LDFLAGS)
