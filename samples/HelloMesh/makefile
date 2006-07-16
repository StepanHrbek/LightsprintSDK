CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloMesh.o

all: HelloMesh.exe

$(BIN)HelloMesh.exe: $(OBJS) $(MAKE) ../../lib/libRRMesh.a
	gcc $(OBJS) ../../lib/libRRMesh.a -lstdc++ -o HelloMesh.exe

clean:
	del *.o
	del HelloMesh.exe
