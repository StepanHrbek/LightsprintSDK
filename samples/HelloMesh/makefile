CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloMesh.o
LIBS = ../../lib/libRRMesh.a

all: HelloMesh.exe

HelloMesh.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -o HelloMesh.exe

clean:
	del *.o
	del HelloMesh.exe
