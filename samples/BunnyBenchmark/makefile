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

all: $(BIN)BunnyBenchmark.exe

$(BIN)BunnyBenchmark.exe: $(OBJS) $(MAKE) ../../lib/libRRMesh.a ../../lib/libRRCollider.a
	gcc $(OBJS) ../../lib/libRRMesh.a ../../lib/libRRCollider.a -lstdc++ -o $(BIN)BunnyBenchmark.exe

clean:
	del *.o
	del $(BIN)BunnyBenchmark.exe
