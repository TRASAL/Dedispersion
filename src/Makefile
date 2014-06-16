
debug		:= 0
include		../Makefile.inc


all: clean generatorSIMD
 

generatorSIMD: generatorSIMD.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/generatorSIMD generatorSIMD.cpp $(UTILS_SRC) $(INCLUDES) $(LIBS) $(CFLAGS) $(LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/generatorSIMD
