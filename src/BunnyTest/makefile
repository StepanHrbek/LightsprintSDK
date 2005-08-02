CC = gcc
CPP = gcc
CFLAGS = -Wall -I../../include
CFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
CFLAGS += -fomit-frame-pointer
CFLAGS += -march=athlon-xp -msse
CPPFLAGS = $(CFLAGS)
BIN = ./
#../../bin/
MAKE = makefile

OBJS = BunnyTest.o plymeshreader.o rply.o sphereunitvecpool.o

all: $(BIN)BunnyTest.exe

$(BIN)BunnyTest.exe: $(OBJS) $(MAKE) ../../lib/libRRIntersect.a
	gcc $(OBJS) ../../lib/libRRIntersect.a -lstdc++ -o $(BIN)BunnyTest.exe

BunnyTest.o: $(MAKE)

plymeshreader.o: $(MAKE)

rply.o: $(MAKE)

sphereunitvecpool.o:	$(MAKE)

clean:
	del *.o
	del $(BIN)BunnyTest.exe
