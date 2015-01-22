
# https://github.com/isazi/utils
UTILS := $(HOME)/src/utils
# https://github.com/isazi/OpenCL
OPENCL := $(HOME)/src/OpenCL
# https://github.com/isazi/AstroData
ASTRODATA := $(HOME)/src/AstroData

CPU_INCLUDES := -I"include" -I"$(ASTRODATA)/include" -I"$(UTILS)/include"
CL_INCLUDES := $(CPU_INCLUDES) -I"$(OPENCL)/include"
CL_LIBS := -L"$(OPENCL_LIB)"

CFLAGS := -std=c++11 -Wall
ifneq ($(debug), 1)
	CFLAGS += -O3 -g0
else
	CFLAGS += -O0 -3g
endif

CPU_LDFLAGS := -lm
CL_LDFLAGS := $(CPU_LDFLAGS) -lOpenCL

CC := g++

# Dependencies
CPU_DEPS := $(ASTRODATA)/bin/Observation.o $(UTILS)/bin/ArgumentList.o $(UTILS)/bin/Timer.o $(UTILS)/bin/utils.o bin/Dedispersion.o
CL_DEPS := $(CPU_DEPS) $(OPENCL)/bin/Exceptions.o $(OPENCL)/bin/InitializeOpenCL.o $(OPENCL)/bin/Kernel.o 


all: Dedispersion.o DedispersionTest DedispersionTuning printCode printShifts

Dedispersion.o: $(UTILS)/bin/utils.o include/Shifts.hpp include/Dedispersion.hpp src/Dedispersion.cpp
	$(CC) -o bin/Dedispersion.o -c src/Dedispersion.cpp $(CL_INCLUDES) $(CFLAGS)

DedispersionTest: $(CL_DEPS) src/DedispersionTest.cpp
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp $(CL_DEPS) $(CL_INCLUDES) $(CL_LIBS) $(CL_LDFLAGS) $(CFLAGS)

DedispersionTuning: $(CL_DEPS) src/DedispersionTuning.cpp
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(CL_DEPS) $(CL_INCLUDES) $(CL_LIBS) $(CL_LDFLAGS) $(CFLAGS)

printCode: $(CPU_DEPS) src/printCode.cpp
	$(CC) -o bin/printCode src/printCode.cpp $(CPU_DEPS) $(CPU_INCLUDES) $(CPU_LDFLAGS) $(CFLAGS)

printShifts: $(CPU_DEPS) src/printShifts.cpp
	$(CC) -o bin/printShifts src/printShifts.cpp $(CPU_DEPS) $(CPU_INCLUDES) $(CPU_LDFLAGS) $(CFLAGS)

clean:
	rm bin/*

