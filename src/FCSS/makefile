CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -I../../samples/HelloRealtimeRadiosity -DRR_STATIC
# -DGLEW_STATIC
#CPPFLAGS += -O1 -DNDEBUG
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = ../../lib/libRRGPUOpenGL.a ../../lib/libRRRealtimeRadiosity.a ../../lib/libDemoEngine.a ../../lib/libRRIllumination.a ../../lib/libRRVision.a ../../lib/libRRCollider.a ../../lib/libRRMesh.a

HELLO = ../../samples/HelloRealtimeRadiosity/
BIN = ../../bin/

OBJS = shadowcast.o Bugs.o $(HELLO)3ds2rr.o $(HELLO)DynamicObject.o

all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile $(LIBS)
	gcc $(OBJS) $(LIBS) ../../bin/win32/glew32.dll ../../lib/win32/FreeImage.a -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o $(BIN)fcss.exe

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del ..\..\samples\HelloRealtimeRadiosity\*.o
	del ..\..\bin\fcss.exe
