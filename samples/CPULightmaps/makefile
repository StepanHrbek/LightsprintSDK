PLATFORM = linux64
DEBUG    =
CPPFLAGS = -I../../include -DRR_STATIC

ifdef DEBUG
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sd.a
	CPPFLAGS += -D_DEBUG
else
	LIBS     = ../../lib/$(PLATFORM)/libLightsprintCore_sr.a
	CPPFLAGS += -O3 -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
	CPPFLAGS += -fopenmp
endif

MAKE = makefile
OBJS = CPULightmaps.o

all: CPULightmaps.exe

CPULightmaps.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lstdc++ -o CPULightmaps.exe

clean:
	rm *.o
	rm CPULightmaps.exe
