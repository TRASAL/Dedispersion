
# https://github.com/isazi/utils
UTILS := $(HOME)/src/utils
# https://github.com/isazi/OpenCL
OPENCL := $(HOME)/src/OpenCL
# https://github.com/isazi/AstroData
ASTRODATA := $(HOME)/src/AstroData

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


all: bin/Shifts.o bin/Dedispersion.o bin/DedispersionTest bin/DedispersionTuning bin/printCode bin/printShifts

bin/Shifts.o: $(ASTRODATA)/bin/Observation.o include/Shifts.hpp src/Shifts.cpp
	$(CC) -o bin/Shifts.o -c src/Shifts.cpp $(INCLUDES) $(CFLAGS)

bin/Dedispersion.o: $(UTILS)/bin/utils.o bin/Shifts.o include/Dedispersion.hpp src/Dedispersion.cpp
	$(CC) -o bin/Dedispersion.o -c src/Dedispersion.cpp $(CL_INCLUDES) $(CFLAGS)

bin/DedispersionTest: $(CL_DEPS) src/DedispersionTest.cpp
	$(CC) -o bin/DedispersionTest src/DedispersionTest.cpp $(CL_DEPS) $(CL_INCLUDES) $(CL_LIBS) $(CL_LDFLAGS) $(CFLAGS)

bin/DedispersionTuning: $(CL_DEPS) src/DedispersionTuning.cpp
	$(CC) -o bin/DedispersionTuning src/DedispersionTuning.cpp $(CL_DEPS) $(CL_INCLUDES) $(CL_LIBS) $(CL_LDFLAGS) $(CFLAGS)

bin/printCode: $(DEPS) src/printCode.cpp
	$(CC) -o bin/printCode src/printCode.cpp $(DEPS) $(CL_INCLUDES) $(LDFLAGS) $(CFLAGS)

bin/printShifts: $(DEPS) src/printShifts.cpp
	$(CC) -o bin/printShifts src/printShifts.cpp $(DEPS) $(INCLUDES) $(LDFLAGS) $(CFLAGS)

clean:
	-@rm bin/*

