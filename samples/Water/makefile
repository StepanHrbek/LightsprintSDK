CPP = gcc
CPPFLAGS = -Wall -I../../include -O3 -DNDEBUG -DGLEW_STATIC
MAKE = makefile

OBJS = Water.o
LIBS = ../../lib/libDemoEngine.a

all: Water.exe

Water.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o Water.exe

clean:
	del *.o
	del Water.exe
