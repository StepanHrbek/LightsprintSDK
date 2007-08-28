CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DGLEW_STATIC
MAKE = makefile

OBJS = \
	PenumbraShadows.o \
	DynamicObject.o \
	../Import3DS/Model_3DS.o \
	../Import3DS/RendererOf3DS.o

LIBS = ../../lib/win32/libLightsprintGL_sr.a ../../lib/win32/libLightsprintCore_sr.a

all: PenumbraShadows.exe

PenumbraShadows.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../bin/win32/glut32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lstdc++ -lwinmm -s -o PenumbraShadows.exe

clean:
	del *.o
	del PenumbraShadows.exe
