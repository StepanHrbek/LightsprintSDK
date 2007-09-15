CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DRR_STATIC
# -DGLEW_STATIC
MAKE = makefile

OBJS = Lightmaps.o ../Import3DS/RRObject3DS.o ../RealtimeRadiosity/DynamicObject.o
LIBS = ../../lib/libLightsprintCore.a ../../lib/libLightsprintGL.a

all: Lightmaps.exe

Lightmaps.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o Lightmaps.exe

clean:
	del *.o
	del Lightmaps.exe
