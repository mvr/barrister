CC = clang++
CFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 -march=native -mtune=native -flto -fno-stack-protector -fomit-frame-pointer -g
LDFLAGS =

# CC = /usr/local/opt/llvm/bin/clang++
# LDFLAGS=-L/usr/local/opt/llvm/lib/c++ -Wl,-rpath,/usr/local/opt/llvm/lib/c++

PROFDATAEXE = /usr/local/opt/llvm/bin/llvm-profdata
ifneq ($(wildcard instrumenting/pass2.profdata),)
	INSTRUMENTFLAGS = -fprofile-use=instrumenting/pass2.profdata
else
	INSTRUMENTFLAGS =
endif

all: Barrister

Barrister: Barrister.cpp LifeAPI.h *.hpp
	$(CC) $(CFLAGS) $(INSTRUMENTFLAGS) -o Barrister Barrister.cpp $(LDFLAGS)

instrument: Barrister.cpp LifeAPI.h *.hpp
	$(CC) $(CFLAGS) -fprofile-generate=instrumenting/pass1 -o instrumenting/pass1-Barrister Barrister.cpp
	instrumenting/pass1-Barrister inputs/benchmark.toml
	$(PROFDATAEXE) merge instrumenting/pass1 -o instrumenting/pass1.profdata
	$(CC) $(CFLAGS) -fno-lto -fprofile-use=instrumenting/pass1.profdata -fcs-profile-generate=instrumenting/pass2 -o instrumenting/pass2-Barrister Barrister.cpp
	instrumenting/pass2-Barrister inputs/benchmark.toml
	$(PROFDATAEXE) merge instrumenting/pass1.profdata instrumenting/pass2 -o instrumenting/pass2.profdata
	touch Barrister.cpp
