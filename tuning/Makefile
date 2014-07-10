
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion DedispersionAVX DedispersionPhi
 

Dedispersion: Dedispersion.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionTuning Dedispersion.cpp $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS) 

DedispersionAVX: DedispersionSIMD.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionAVXTuning DedispersionSIMD.cpp $(CPU_INCLUDES) $(CPU_CFLAGS) $(CPU_LDFLAGS)

DedispersionPhi: DedispersionSIMD.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionPhiTuning DedispersionSIMD.cpp $(PHI_INCLUDES) $(PHI_CFLAGS) $(PHI_LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTuning
	rm -f $(PROJ_BASE)/bin/DedispersionAVXTuning
	rm -f $(PROJ_BASE)/bin/DedispersionPhiTuning
