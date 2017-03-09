SOURCE_ROOT ?= $(HOME)

# https://github.com/isazi/utils
UTILS := $(SOURCE_ROOT)/utils
# https://github.com/isazi/OpenCL
OPENCL := $(SOURCE_ROOT)/OpenCL
# https://github.com/isazi/AstroData
ASTRODATA := $(SOURCE_ROOT)/AstroData

# HDF5
HDF5_INCLUDE ?= -I/usr/include
HDF5_LIBS ?= -L/usr/lib
HDF5_LDFLAGS ?= -lhdf5 -lhdf5_cpp -lz

INCLUDES := -I"include" -I"$(ASTRODATA)/include" -I"$(UTILS)/include"
CL_INCLUDES := $(INCLUDES) -I"$(OPENCL)/include"
CL_LIBS := -L"$(OPENCL_LIB)"

CFLAGS := -std=c++11 -Wall
ifneq ($(debug), 1)
	CFLAGS += -O3 -g0
else
	CFLAGS += -O0 -g3
endif

LDFLAGS := -lm
CL_LDFLAGS := $(LDFLAGS) -lOpenCL

CC := g++

# Dependencies
DEPS := $(ASTRODATA)/bin/Observation.o $(UTILS)/bin/ArgumentList.o $(UTILS)/bin/Timer.o $(UTILS)/bin/utils.o bin/Shifts.o bin/Dedispersion.o
CL_DEPS := $(DEPS) $(OPENCL)/bin/Exceptions.o $(OPENCL)/bin/InitializeOpenCL.o $(OPENCL)/bin/Kernel.o

# http://psrdada.sourceforge.net/
ifdef PSRDADA
	DADA_DEPS := $(PSRDADA)/src/dada_hdu.o $(PSRDADA)/src/ipcbuf.o $(PSRDADA)/src/ipcio.o $(PSRDADA)/src/ipcutil.o $(PSRDADA)/src/ascii_header.o $(PSRDADA)/src/multilog.o $(PSRDADA)/src/tmutil.o
	CFLAGS += -DHAVE_PSRDADA
else
	DADA_DEPS :=
endif

all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning

bin/Shifts.o: $(ASTRODATA)/bin/Observation.o include/Shifts.hpp src/Shifts.cpp
	-@mkdir -p bin
	$(CC) -o bin/Shifts.o -c src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: $(UTILS)/bin/utils.o bin/Shifts.o $(OPENCL)/include/Bits.hpp include/Dedispersion.hpp src/Dedispersion.cpp
	-@mkdir -p bin
	$(CC) -o bin/Dedispersion.o -c src/Dedispersion.cpp $(CL_INCLUDES) $(CFLAGS)

bin/DedispersionTest: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTest.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" $(HDF5_INCLUDE) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LIBS) $(HDF5_LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: $(CL_DEPS) $(DADA_DEPS) $(ASTRODATA)/include/ReadData.hpp $(ASTRODATA)/bin/ReadData.o include/configuration.hpp src/DedispersionTuning.cpp
	-@mkdir -p bin
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(CL_DEPS) $(ASTRODATA)/bin/ReadData.o $(DADA_DEPS) $(CL_INCLUDES) -I"$(PSRDADA)/src" $(HDF5_INCLUDE) $(CL_LIBS) $(CL_LDFLAGS) $(HDF5_LIBS) $(HDF5_LDFLAGS) $(CFLAGS)

test: bin/DedispersionTest
	touch empty
	./bin/DedispersionTest -opencl_platform 0 -opencl_device 0 -input_bits 32 -padding 128 -vector 32 -zapped_channels empty -single_step -threadsD0 32 -threadsD1 4 -itemsD0 2 -itemsD1 4 -unroll 4 -channels 16 -min_freq 52.5 -channel_bandwidth 5 -samples 1024 -dms 32 -dm_first 1.1 -dm_step 5.5 
	rm empty

tune: bin/DedispersionTuning
	touch empty
	./bin/DedispersionTuning -opencl_platform 0 -opencl_device 0 -input_bits 32 -padding 128 -vector 32 -single_step -zapped_channels empty -min_threads 8 -max_threads 1024 -max_rows 128 -max_columns 128 -max_items 255 -max_sample_items 32 -max_dm_items 32 -max_unroll 4 -channels 16 -min_freq 52.5 -channel_bandwidth 5 -samples 1024 -dms 32 -dm_first 1.1 -dm_step 5.5 -iterations 3 -beams 2 -synthesized_beams 4
	rm empty

clean:
	-@rm bin/*

