
debug		:= 0
include		../Makefile.inc


all: clean printCode
 
printCode: printCode.cpp
	$(CC) -o $(PROJ_BASE)/bin/printCode printCode.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/printCode
