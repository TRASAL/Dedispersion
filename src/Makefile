
debug		:= 0
include		../Makefile.inc


all: clean printCode printShifts
 
printCode: printCode.cpp
	$(CC) -o $(PROJ_BASE)/bin/printCode printCode.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

printShifts: printShifts.cpp
	$(CC) -o $(PROJ_BASE)/bin/printShifts printShifts.cpp $(CPU_INCLUDES) $(CFLAGS) $(CPU_LDFLAGS) 

clean:
	rm -f $(PROJ_BASE)/bin/printCode
	rm -f $(PROJ_BASE)/bin/printShifts
