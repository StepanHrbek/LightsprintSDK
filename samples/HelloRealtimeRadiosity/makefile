CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC -DGLEW_STATIC
MAKE = makefile

OBJS = HelloRealtimeRadiosity.o
LIBS = ../../lib/libRRRealtimeRadiosity.a ../../lib/libDemoEngine.a ../../lib/libRRIllumination.a ../../lib/libRRVision.a ../../lib/libRRCollider.a ../../lib/libRRMesh.a

all: HelloRealtimeRadiosity.exe

HelloRealtimeRadiosity.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -o HelloRealtimeRadiosity.exe

clean:
	del *.o
	del HelloRealtimeRadiosity.exe
