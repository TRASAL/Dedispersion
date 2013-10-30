
debug		:= 0
include		../Makefile.inc


all: clean dedispersionMemoryUsage

 
dedispersionMemoryUsage: dedispersionMemoryUsage.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/dedispersionMemoryUsage dedispersionMemoryUsage.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/dedispersionMemoryUsage
