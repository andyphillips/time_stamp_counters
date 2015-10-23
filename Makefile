#
# makefile
PROJECT=tsc
SRCS=tsc.c
INCL=tsc.h
CFLAGS=-g -Wall
LIBS=-lrt -lm -lpthread

all: $(PROJECT) 

$(PROJECT): $(SRCS) $(INCL)
	gcc $(CFLAGS) -DTEST_TSC -DTSC_VERBOSE  -o $(PROJECT)  $(SRCS) $(LIBS)

clean: 
	rm -f $(PROJECT) *~ *.o 
	