LIB = ../../lib/libRREngine.a
TMP = #../../tmp/RREngine/

MACH = 
OPT = -O3 -Iinclude -I../../include
CPPFLAGS = $(MACH) $(OPT)
CPPFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
CPPFLAGS += -march=athlon-xp --param max-inline-insns-auto=300
#CPPFLAGS += -march=athlon-xp -msse -mfpmath=sse --param max-inline-insns-auto=300
#CPPFLAGS += -fomit-frame-pointer
CC = gcc
CPPFLAGS += -DSUPPORT_INTERPOL

OBJS = $(TMP)core.o $(TMP)dynamic.o $(TMP)geometry.o $(TMP)interpol.o $(TMP)intersections.o $(TMP)RREngine.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	del $(TMP)*.o
	del $(LIB)

$(TMP)core.o: makefile

$(TMP)dynamic.o: makefile

$(TMP)geometry.o: makefile

$(TMP)interpol.o: makefile

$(TMP)intersections.o: makefile

$(TMP)RREngine.o: makefile
