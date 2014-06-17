
debug		:= 0
include		../Makefile.inc


all: clean generatorSIMD
 

generatorSIMD: generatorSIMD.cpp
	$(LINKER) -o $(PROJ_BASE)/bin/generatorSIMD generatorSIMD.cpp $(UTILS_SRC) $(CPU_INCLUDES) $(CPU_CFLAGS) $(CPU_LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/generatorSIMD
