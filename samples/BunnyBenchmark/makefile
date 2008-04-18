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

BIN = 
MAKE = makefile

OBJS = BunnyBenchmark.o plymeshreader.o rply.o sphereunitvecpool.o

all: $(BIN)BunnyBenchmark.exe

$(BIN)BunnyBenchmark.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lstdc++ -lgomp -o $(BIN)BunnyBenchmark.exe

clean:
	rm *.o
	rm $(BIN)BunnyBenchmark.exe
