
debug		:= 0
include		../Makefile.inc


all: clean dedispersionMemoryUsage generator

 
dedispersionMemoryUsage: dedispersionMemoryUsage.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/dedispersionMemoryUsage dedispersionMemoryUsage.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)

generator: generator.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/generator generator.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROJ_BASE)/bin/dedispersionMemoryUsage
	rm -f $(PROJ_BASE)/bin/generator
