CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = HelloCollider.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: HelloCollider.exe

HelloCollider.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lstdc++ -lwinmm -o HelloCollider.exe

clean:
	del *.o
	del HelloCollider.exe
