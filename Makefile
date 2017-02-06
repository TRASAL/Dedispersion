
SOURCE_ROOT ?= $(HOME)

# https://github.com/isazi/utils
UTILS := $(SOURCE_ROOT)/src/utils
# https://github.com/isazi/OpenCL
OPENCL := $(SOURCE_ROOT)/src/OpenCL
# https://github.com/isazi/AstroData
ASTRODATA := $(SOURCE_ROOT)/src/AstroData
# HDF5
HDF5 := $(SOURCE_ROOT)/src/hdf5
# http://psrdada.sourceforge.net/
PSRDADA  := $(SOURCE_ROOT)/src/psrdada

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


all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning

bin/Shifts.o: $(ASTRODATA)/bin/Observation.o include/Shifts.hpp src/Shifts.cpp
	-mkdir -p bin
	$(CC) -o bin/Shifts.o -c src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: $(UTILS)/bin/utils.o bin/Shifts.o $(OPENCL)/include/Bits.hpp include/Dedispersion.hpp src/Dedispersion.cpp
	-mkdir -p bin
	$(CC) -o bin/Dedispersion.o -c src/Dedispersion.cpp $(CL_INCLUDES) $(CFLAGS)

bin/DedispersionTest: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTest.cpp
	-mkdir -p bin
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" -I"$(HDF5)/include" $(HDF5_LIBS) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTuning.cpp
	-mkdir -p bin
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" -I"$(HDF5)/include" $(HDF5_LIBS) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LDFLAGS) $(CFLAGS)

test: bin/DedispersionTest
	touch empty
	./bin/DedispersionTest -opencl_platform 0 -opencl_device 0 -input_bits 32 -padding 32 -vector 32 -zapped_channels empty -threads0 4 -threads1 4 -items0 4 -items1 4 -unroll 4 -channels 16 -min_freq 52.5 -channel_bandwidth 5 -samples 1024 -dms 16 -dm_first 1.1 -dm_step 5.5 
	rm empty

tune: bin/DedispersionTuning
	touch empty
	./bin/DedispersionTuning -opencl_platform 0 -opencl_device 0 -input_bits 32 -padding 32 -vector 32 -zapped_channels empty -min_threads 4 -max_threads 1024 -max_items 255 -max_unroll 4 -channels 16 -min_freq 52.5 -channel_bandwidth 5 -samples 1024 -dms 16 -dm_first 1.1 -dm_step 5.5 -max_loopsize 512 -max_rows 16 -max_columns 16 -iterations 3
	rm empty

clean:
	-@rm bin/*

