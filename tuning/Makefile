
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion
 

Dedispersion: Dedispersion.cpp
	$(CC) -o $(PROJ_BASE)/bin/DedispersionTuning Dedispersion.cpp ../src/Dedispersion.cpp $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTuning
