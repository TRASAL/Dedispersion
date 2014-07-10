
debug		:= 0
include		../Makefile.inc


all: clean generatorSIMD
 

generatorSIMD: generatorSIMD.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/generatorSIMD generatorSIMD.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/generatorSIMD
