
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion
 

Dedispersion: Dedispersion.cpp
	rm -f $(PROJ_BASE)/bin/DedispersionTest
	$(LINKER) -o $(PROJ_BASE)/bin/DedispersionTest Dedispersion.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)

DedispersionCPU: DedispersionCPU.cpp
	rm -f $(PROJ_BASE)/bin/DedispersionCPUTest
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionCPUTest DedispersionCPU.cpp $(UTILS_SRC) $(CPU_INCLUDES) $(CPU_LIBS) $(CPU_CFLAGS) $(CPU_LDFLAGS)

DedispersionPhi: DedispersionCPU.cpp
	rm -f $(PROJ_BASE)/bin/DedispersionPhiTest
	$(INTELCXX) -o $(PROJ_BASE)/bin/DedispersionPhiTest DedispersionCPU.cpp $(UTILS_SRC) $(PHI_INCLUDES) $(PHI_LIBS) $(PHI_CFLAGS) $(PHI_LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTest
	rm -f $(PROJ_BASE)/bin/DedispersionCPUTest
	rm -f $(PROJ_BASE)/bin/DedispersionPhiTest
