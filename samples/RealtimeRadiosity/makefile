PLATFORM = linux64
DEBUG    =
CPPFLAGS = -I../../include -DRR_STATIC

ifdef DEBUG
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sd.a ../../lib/$(PLATFORM)/libLightsprintGL_sd.a
	CPPFLAGS += -D_DEBUG
else
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sr.a ../../lib/$(PLATFORM)/libLightsprintGL_sr.a
	CPPFLAGS += -O3 -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
	CPPFLAGS += -fopenmp
endif

BIN = 
MAKE = makefile
OBJS = RealtimeRadiosity.o ../Import3DS/Model_3DS.o ../Import3DS/RRObject3DS.o ../Import3DS/RendererOf3DS.o DynamicObject.o

all: RealtimeRadiosity.exe

RealtimeRadiosity.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lGLEW -lfreeimage -lGL -lGLU -lglut -lstdc++ -lgomp -s -o RealtimeRadiosity.exe

clean:
	rm *.o
	rm RealtimeRadiosity.exe
