CPPFLAGS = -Wall -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
#CPPFLAGS += -fopenmp
CPPFLAGS += -ffast-math
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=k8 -msse
CC = gcc
BIN = 
MAKE = makefile

OBJS = BunnyBenchmark.o plymeshreader.o rply.o sphereunitvecpool.o
LIBS = ../../lib/win32/libLightsprintCore_sr.a

all: $(BIN)BunnyBenchmark.exe

$(BIN)BunnyBenchmark.exe: $(OBJS) $(MAKE) $(LIBS)
	g++ $(OBJS) $(LIBS) -lwinmm -lstdc++ -o $(BIN)BunnyBenchmark.exe

clean:
	del *.o
	del $(BIN)BunnyBenchmark.exe
