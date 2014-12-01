
debug		:= 0
include		../Makefile.inc


all: clean generatorSIMD printCode
 
generatorSIMD: generatorSIMD.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/generatorSIMD generatorSIMD.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

printCode: printCode.cpp
	$(INTELCXX) -o $(PROJ_BASE)/bin/printCode printCode.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/generatorSIMD
	rm -f $(PROJ_BASE)/bin/printCode
