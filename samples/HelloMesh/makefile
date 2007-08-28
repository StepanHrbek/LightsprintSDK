CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = HelloMesh.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: HelloMesh.exe

HelloMesh.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lstdc++ -lwinmm -o HelloMesh.exe

clean:
	del *.o
	del HelloMesh.exe
