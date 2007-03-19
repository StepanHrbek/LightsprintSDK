CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
# -DGLEW_STATIC
MAKE = makefile

OBJS = HelloRealtimeRadiosity.o RRObject3DS.o DynamicObject.o
LIBS = ../../lib/libRRGPUOpenGL.a ../../lib/libRRRealtimeRadiosity.a ../../lib/libDemoEngine.a ../../lib/libRRIllumination.a ../../lib/libRRVision.a ../../lib/libRRCollider.a ../../lib/libRRMesh.a

all: HelloRealtimeRadiosity.exe

HelloRealtimeRadiosity.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o HelloRealtimeRadiosity.exe

clean:
	del *.o
	del HelloRealtimeRadiosity.exe
