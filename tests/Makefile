
debug		:= 0
include		../Makefile.inc


all: clean Dedispersion
 

Dedispersion: Dedispersion.cpp
	rm -f $(PROJ_BASE)/bin/DedispersionTest
	$(LINKER) -o $(PROJ_BASE)/bin/DedispersionTest Dedispersion.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) -mavx $(LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/DedispersionTest
