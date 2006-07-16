CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloVision.o

all: HelloVision.exe

$(BIN)HelloVision.exe: $(OBJS) $(MAKE) ../../lib/libRRMesh.a ../../lib/libRRCollider.a ../../lib/libRRVision.a
	gcc $(OBJS) ../../lib/libRRMesh.a ../../lib/libRRCollider.a ../../lib/libRRVision.a -lstdc++ -o HelloVision.exe

clean:
	del *.o
	del HelloVision.exe
