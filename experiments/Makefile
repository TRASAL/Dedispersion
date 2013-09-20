
debug		:= 0
include		../Makefile.inc


all: clean scaleDedispersionCPU dedispersionMemoryUsage
 
scaleDedispersionCPU: scaleDedispersionCPU.cpp
	rm -f $(PROJ_BASE)/bin/scaleDedispersionCPU
	$(INTELCXX) -o $(PROJ_BASE)/bin/scaleDedispersionCPU scaleDedispersionCPU.cpp $(UTILS_SRC) $(CPU_INCLUDES) $(CPU_LIBS) $(CPU_CFLAGS) $(CPU_LDFLAGS)

dedispersionMemoryUsage: dedispersionMemoryUsage.cpp
	rm -f $(PROJ_BASE)/bin/dedispersionMemoryUsage
	$(LINKER) -o $(PROJ_BASE)/bin/dedispersionMemoryUsage dedispersionMemoryUsage.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)	

clean:
	rm -f $(PROJ_BASE)/bin/scaleDedispersionCPU
	rm -f $(PROJ_BASE)/bin/dedispersionMemoryUsage
