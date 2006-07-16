CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloCollider.o

all: HelloCollider.exe

$(BIN)HelloCollider.exe: $(OBJS) $(MAKE) ../../lib/libRRMesh.a ../../lib/libRRCollider.a
	gcc $(OBJS) ../../lib/libRRMesh.a ../../lib/libRRCollider.a -lstdc++ -o HelloCollider.exe

clean:
	del *.o
	del HelloCollider.exe
