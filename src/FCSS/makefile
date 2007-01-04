CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -I../../samples/HelloRealtimeRadiosity -DRR_STATIC -DGLEW_STATIC
#CPPFLAGS += -O1 -DNDEBUG
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = ../../lib/libRRRealtimeRadiosity.a ../../lib/libDemoEngine.a ../../lib/libRRIllumination.a ../../lib/libRRVision.a ../../lib/libRRCollider.a ../../lib/libRRMesh.a

HELLO = ../../samples/HelloRealtimeRadiosity/
BIN = ../../bin/

OBJS = shadowcast.o Bugs.o $(HELLO)3ds2rr.o $(HELLO)DynamicObject.o $(HELLO)RendererOfRRObject.o $(HELLO)RRIlluminationEnvironmentMapInOpenGL.o $(HELLO)RRIlluminationPixelBufferInOpenGL.o

all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile $(LIBS)
	gcc $(OBJS) $(LIBS) -lwinmm -lOpenGL32 -lGLU32 -lGLUT -lstdc++ -s -o $(BIN)fcss.exe

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del ..\..\bin\fcss.exe
