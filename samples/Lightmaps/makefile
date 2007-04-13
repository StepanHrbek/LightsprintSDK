CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
# -DGLEW_STATIC
MAKE = makefile

OBJS = Lightmaps.o ../Import3DS/RRObject3DS.o ../HelloRealtimeRadiosity/DynamicObject.o
LIBS = ../../lib/libRRGPUOpenGL.a ../../lib/libDemoEngine.a ../../lib/libRRIllumination.a ../../lib/libRRVision.a ../../lib/libRRCollider.a ../../lib/libRRMesh.a

all: Lightmaps.exe

Lightmaps.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o HelloRealtimeRadiosity.exe

clean:
	del *.o
	del Lightmaps.exe
