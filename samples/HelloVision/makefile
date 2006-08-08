CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloVision.o
LIBS = ../../lib/libRRMesh.a ../../lib/libRRCollider.a ../../lib/libRRVision.a

all: HelloVision.exe

HelloVision.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -o HelloVision.exe

clean:
	del *.o
	del HelloVision.exe
