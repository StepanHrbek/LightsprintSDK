LIB = ../../lib/libRREngine.a
TMP = #../../tmp/RREngine/

CPPFLAGS = -Iinclude -I../../include -DSUPPORT_INTERPOL
CPPFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp --param max-inline-insns-auto=300
#CPPFLAGS += -msse -mfpmath=sse
CC = gcc
CPPFLAGS +=

OBJS = $(TMP)RREngine.o $(TMP)core.o $(TMP)dynamic.o $(TMP)geometry.o $(TMP)interpol.o $(TMP)intersections.o

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

