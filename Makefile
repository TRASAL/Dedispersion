
ROOT ?= $(HOME)

# https://github.com/isazi/utils
UTILS := $(ROOT)/src/utils
# https://github.com/isazi/OpenCL
OPENCL := $(ROOT)/src/OpenCL
# https://github.com/isazi/AstroData
ASTRODATA := $(ROOT)/src/AstroData
# HDF5
HDF5 := $(ROOT)/src/hdf5
# http://psrdada.sourceforge.net/
PSRDADA  := $(ROOT)/src/psrdada

INCLUDES := -I"include" -I"$(ASTRODATA)/include" -I"$(UTILS)/include"
CL_INCLUDES := $(INCLUDES) -I"$(OPENCL)/include"
CL_LIBS := -L"$(OPENCL_LIB)"
HDF5_LIBS := -L"$(HDF5)/lib"

CFLAGS := -std=c++11 -Wall
ifneq ($(debug), 1)
	CFLAGS += -O3 -g0
else
	CFLAGS += -O0 -g3
endif

LDFLAGS := -lm
CL_LDFLAGS := $(LDFLAGS) -L/usr/local/cuda-6.0/targets/x86_64-linux/lib/ -lOpenCL
HDF5_LDFLAGS := -lhdf5 -lhdf5_cpp

CC := g++

# Dependencies
DEPS := $(ASTRODATA)/bin/Observation.o $(UTILS)/bin/ArgumentList.o $(UTILS)/bin/Timer.o $(UTILS)/bin/utils.o bin/Shifts.o bin/Dedispersion.o
CL_DEPS := $(DEPS) $(OPENCL)/bin/Exceptions.o $(OPENCL)/bin/InitializeOpenCL.o $(OPENCL)/bin/Kernel.o
DADA_DEPS := $(PSRDADA)/src/dada_hdu.o $(PSRDADA)/src/ipcbuf.o $(PSRDADA)/src/ipcio.o $(PSRDADA)/src/ipcutil.o $(PSRDADA)/src/ascii_header.o $(PSRDADA)/src/multilog.o $(PSRDADA)/src/tmutil.o


all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning bin/printCode bin/printShifts

bin/Shifts.o: $(ASTRODATA)/bin/Observation.o include/Shifts.hpp src/Shifts.cpp
	$(CC) -o bin/Shifts.o -c src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: $(UTILS)/bin/utils.o bin/Shifts.o $(OPENCL)/include/Bits.hpp include/Dedispersion.hpp src/Dedispersion.cpp
	$(CC) -o bin/Dedispersion.o -c src/Dedispersion.cpp $(CL_INCLUDES) $(CFLAGS)

bin/DedispersionTest: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTest.cpp
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" -I"$(HDF5)/include" $(HDF5_LIBS) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTuning.cpp
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" -I"$(HDF5)/include" $(HDF5_LIBS) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LDFLAGS) $(CFLAGS)

bin/printCode: $(DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/printCode.cpp
	$(CC) -o bin/printCode src/printCode.cpp $(DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" -I"$(HDF5)/include" $(HDF5_LIBS) $(LDFLAGS) $(HDF5_LDFLAGS) $(CFLAGS)

bin/printShifts: $(DEPS) src/printShifts.cpp
	$(CC) -o bin/printShifts src/printShifts.cpp $(DEPS) $(INCLUDES) $(LDFLAGS) $(CFLAGS)

clean:
	-@rm bin/*

