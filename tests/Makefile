
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion
 

Dedispersion: Dedispersion.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionTest Dedispersion.cpp $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)

DedispersionAVX: DedispersionAVX.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionAVXTest DedispersionAVX.cpp $(AVX_INCLUDES) $(AVX_LIBS) $(AVX_CFLAGS) $(AVX_LDFLAGS)

DedispersionPhi: DedispersionAVX.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionPhiTest DedispersionAVX.cpp $(PHI_INCLUDES) $(PHI_LIBS) $(PHI_CFLAGS) $(PHI_LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTest
	rm -f $(PROJ_BASE)/bin/DedispersionAVXTest
	rm -f $(PROJ_BASE)/bin/DedispersionPhiTest
