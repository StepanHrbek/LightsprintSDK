PLATFORM = linux64
DEBUG    = 1
CPPFLAGS = -I../../include -I../../samples/RealtimeRadiosity -DRR_STATIC

ifdef DEBUG
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sd.a ../../lib/$(PLATFORM)/libLightsprintGL_sd.a
	CPPFLAGS += -g -D_DEBUG
else
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sr.a ../../lib/$(PLATFORM)/libLightsprintGL_sr.a -s
	CPPFLAGS += -O3 -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
	CPPFLAGS += -fopenmp
endif

BIN = 
MAKE = makefile
OBJS = AnimationEditor.o AnimationFrame.o AnimationScene.o Autopilot.o DemoPlayer.o DynamicObjects.o Level.o Music.o shadowcast.o ../../samples/Import/ImportFreeImage.o ../../samples/ImportQuake3/Q3Loader.o ../../samples/ImportQuake3/RRObjectBSP.o ../../samples/RealtimeRadiosity/DynamicObject.o ../../samples/Import3DS/Model_3DS.o ../../samples/Import3DS/RendererOf3DS.o ../../samples/Import3DS/RRObject3DS.o

all: fcss.exe

fcss.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lfmodex64 -lGLEW -lfreeimage -lGL -lGLU -lglut -lstdc++ -lgomp -o fcss.exe

clean:
	rm -f *.o ../../samples/Import/*.o ../../samples/Import3DS/*.o ../../samples/ImportQuake3/*.o ../../samples/RealtimeRadiosity/*.o fcss.exe
