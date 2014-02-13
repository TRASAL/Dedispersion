
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion
 

Dedispersion: Dedispersion.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/DedispersionTuning Dedispersion.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTuning
