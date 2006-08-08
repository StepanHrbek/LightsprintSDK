CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
CPPFLAGS = $(CFLAGS)
MAKE = makefile

OBJS = HelloCollider.o
LIBS = ../../lib/libRRMesh.a ../../lib/libRRCollider.a

all: HelloCollider.exe

HelloCollider.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -o HelloCollider.exe

clean:
	del *.o
	del HelloCollider.exe
