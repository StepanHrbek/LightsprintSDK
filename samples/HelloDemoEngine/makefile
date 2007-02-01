CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DGLEW_STATIC
MAKE = makefile

OBJS = HelloDemoEngine.o DynamicObject.o
LIBS = ../../lib/libDemoEngine.a ../../lib/libRRGPUOpenGL.a

all: HelloDemoEngine.exe

HelloDemoEngine.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o HelloDemoEngine.exe

clean:
	del *.o
	del HelloDemoEngine.exe
