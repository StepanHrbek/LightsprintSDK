PLATFORM = linux64
DEBUG    =
CPPFLAGS = -I../../include -DRR_STATIC

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
OBJS = PenumbraShadows.o ../Import/ImportFreeImage.o ../Import3DS/Model_3DS.o ../Import3DS/RRObject3DS.o ../Import3DS/RendererOf3DS.o DynamicObject.o

all: PenumbraShadows.exe

PenumbraShadows.exe: $(OBJS) $(MAKE)
	gcc $(OBJS) $(LIBS) -lGLEW -lfreeimage -lGL -lGLU -lglut -lstdc++ -lgomp -o PenumbraShadows.exe

clean:
	rm *.o
	rm ../Import/ImportFreeImage.o
	rm ../Import3DS/*.o
	rm PenumbraShadows.exe
