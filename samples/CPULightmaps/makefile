CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = CPULightmaps.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: CPULightmaps.exe

CPULightmaps.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -lwinmm -o CPULightmaps.exe

clean:
	del *.o
	del CPULightmaps.exe
