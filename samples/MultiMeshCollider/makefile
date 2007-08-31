CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = MultiMeshCollider.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: MultiMeshCollider.exe

MultiMeshCollider.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lstdc++ -lwinmm -o MultiMeshCollider.exe

clean:
	del *.o
	del MultiMeshCollider.exe
