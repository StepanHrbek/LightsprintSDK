CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
MAKE = makefile

OBJS = HelloStaticSolver.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: HelloStaticSolver.exe

HelloStaticSolver.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -lwinmm -o HelloStaticSolver.exe

clean:
	del *.o
	del HelloStaticSolver.exe
