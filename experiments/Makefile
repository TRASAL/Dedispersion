
debug		:= 0
include		../Makefile.inc


all: clean scaleDedispersionCPU
 
scaleDedispersionCPU: scaleDedispersionCPU.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/scaleDedispersionCPU scaleDedispersionCPU.cpp $(UTILS_SRC) $(CPU_INCLUDES) $(CPU_LIBS) $(CPU_CFLAGS) $(CPU_LDFLAGS)

scaleDedispersionPhi: scaleDedispersionCPU.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/scaleDedispersionPhi scaleDedispersionCPU.cpp $(UTILS_SRC) $(PHI_INCLUDES) $(PHI_LIBS) $(PHI_CFLAGS) $(PHI_LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/scaleDedispersionCPU
	rm -f $(PROJ_BASE)/bin/scaleDedispersionPhi
