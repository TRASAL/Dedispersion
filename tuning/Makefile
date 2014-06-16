
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion DedispersionAVX
 

Dedispersion: Dedispersion.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/DedispersionTuning Dedispersion.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS) 

DedispersionAVX: DedispersionAVX.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionAVXTuning DedispersionAVX.cpp $(UTILS_SRC) $(INCLUDES_CPU) $(CFLAGS_CPU) $(LDFLAGS_CPU)

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTuning
	rm -f $(PROJ_BASE)/bin/DedispersionAVXTuning
