
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

ifdef LOFAR
	CFLAGS += -DHAVE_HDF5
	INCLUDES += -I"$(HDF5INCLUDE)"
	LIBS += -L"$(HDF5LIB)"
	LDFLAGS += -lhdf5 -lhdf5_cpp -lz
endif
ifdef PSRDADA
	CFLAGS += -DHAVE_PSRDADA
	INCLUDES += -I"$(PSRDADA)/src"
	DADA_DEPS := $(PSRDADA)/src/dada_hdu.o $(PSRDADA)/src/ipcbuf.o $(PSRDADA)/src/ipcio.o $(PSRDADA)/src/ipcutil.o $(PSRDADA)/src/ascii_header.o $(PSRDADA)/src/multilog.o $(PSRDADA)/src/tmutil.o
endif

all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning
	-@mkdir -p lib
	$(CC) -o lib/libDedispersion.so -shared -Wl,-soname,libDedispersion.so bin/Shifts.o bin/Dedispersion.o $(CFLAGS)

bin/Shifts.o: $(INSTALL_ROOT)/include/Observation.hpp include/Shifts.hpp src/Shifts.cpp
	-@mkdir -p bin
	$(CC) -o bin/Shifts.o -c -fpic src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: $(INSTALL_ROOT)/include/utils.hpp $(INSTALL_ROOT)/include/Bits.hpp bin/Shifts.o include/Dedispersion.hpp src/Dedispersion.cpp
	-@mkdir -p bin
	$(CC) -o bin/Dedispersion.o -c -fpic src/Dedispersion.cpp $(INCLUDES) $(CFLAGS)

bin/DedispersionTest: $(INSTALL_ROOT)/include/ReadData.hpp include/configuration.hpp src/DedispersionTest.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp bin/Dedispersion.o bin/Shifts.o $(DADA_DEPS) $(INCLUDES) $(LIBS) $(LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: $(INSTALL_ROOT)/include/ReadData.hpp include/configuration.hpp src/DedispersionTuning.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(DADA_DEPS) $(INCLUDES) $(LIBS) $(LDFLAGS) $(CFLAGS)

clean:
	-@rm bin/*
	-@rm lib/*

install: all
	-@cp include/Shifts.hpp $(INSTALL_ROOT)/include
	-@cp include/Dedispersion.hpp $(INSTALL_ROOT)/include
	-@cp lib/* $(INSTALL_ROOT)/lib
	-@cp bin/DedispersionTest $(INSTALL_ROOT)/bin
	-@cp bin/DedispersionTuning $(INSTALL_ROOT)/bin
