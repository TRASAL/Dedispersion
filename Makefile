
INSTALL_ROOT ?= $(HOME)
INCLUDES := -I"include" -I"$(INSTALL_ROOT)/include"
LIBS := -L"$(INSTALL_ROOT)/lib"

CC := g++
CFLAGS := -std=c++11 -Wall
LDFLAGS := -lm -lOpenCL -lutils -lisaOpenCL -lAstroData

ifdef DEBUG
	CFLAGS += -O0 -g3
else
	CFLAGS += -O3 -g0
endif

all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning
	-@mkdir -p lib
	$(CC) -o lib/libDedispersion.so -shared -Wl,-soname,libDedispersion.so bin/Shifts.o bin/Dedispersion.o $(CFLAGS)

bin/Shifts.o: include/Shifts.hpp src/Shifts.cpp
	-@mkdir -p bin
	$(CC) -o bin/Shifts.o -c -fpic src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: bin/Shifts.o include/Dedispersion.hpp src/Dedispersion.cpp
	-@mkdir -p bin
	$(CC) -o bin/Dedispersion.o -c -fpic src/Dedispersion.cpp $(INCLUDES) $(CFLAGS)

bin/DedispersionTest: include/configuration.hpp src/DedispersionTest.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp bin/Dedispersion.o bin/Shifts.o $(INCLUDES) $(LIBS) $(LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: include/configuration.hpp src/DedispersionTuning.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp bin/Dedispersion.o bin/Shifts.o $(INCLUDES) $(LIBS) $(LDFLAGS) $(CFLAGS)

clean:
	-@rm bin/*
	-@rm lib/*

install: all
	-@mkdir -p $(INSTALL_ROOT)/include
	-@cp include/Shifts.hpp $(INSTALL_ROOT)/include
	-@cp include/Dedispersion.hpp $(INSTALL_ROOT)/include
	-@mkdir -p $(INSTALL_ROOT)/lib
	-@cp lib/* $(INSTALL_ROOT)/lib
	-@mkdir -p $(INSTALL_ROOT)/bin
	-@cp bin/DedispersionTest $(INSTALL_ROOT)/bin
	-@cp bin/DedispersionTuning $(INSTALL_ROOT)/bin
