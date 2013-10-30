
debug		:= 0
include		../Makefile.inc


all: clean scaleDedispersionCPU
 
scaleDedispersionCPU: scaleDedispersionCPU.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/scaleDedispersionCPU scaleDedispersionCPU.cpp $(UTILS_SRC) $(CPU_INCLUDES) $(CPU_LIBS) $(CPU_CFLAGS) $(CPU_LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/scaleDedispersionCPU
