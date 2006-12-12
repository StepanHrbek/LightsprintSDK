CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include
CFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
CFLAGS += -ffast-math # vypnuto protoze bunny pada.. nicmene to nepomohlo, pada dal
CFLAGS += -fomit-frame-pointer
CFLAGS += -march=athlon-xp -msse
CPPFLAGS = $(CFLAGS)
BIN = 
MAKE = makefile

OBJS = BunnyBenchmark.o plymeshreader.o rply.o sphereunitvecpool.o
LIBS = ../../lib/libRRMesh.a ../../lib/libRRCollider.a

all: $(BIN)BunnyBenchmark.exe

$(BIN)BunnyBenchmark.exe: $(OBJS) $(MAKE) $(LIBS)
	gcc $(OBJS) $(LIBS) -lwinmm -lstdc++ -o $(BIN)BunnyBenchmark.exe

clean:
	del *.o
	del $(BIN)BunnyBenchmark.exe
