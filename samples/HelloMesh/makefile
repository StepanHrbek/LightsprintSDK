PLATFORM = linux64
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = HelloMesh.o
LIBS = ../../lib/$(PLATFORM)/libLightsprintCore_sr.a

all: HelloMesh.exe

HelloMesh.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lstdc++ -o HelloMesh.exe

clean:
	rm *.o
	rm HelloMesh.exe
