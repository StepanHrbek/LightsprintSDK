LIB = ../../lib/libRREngine.a
TMP = #../../tmp/RREngine/

CPPFLAGS = -Iinclude -I../../include -DSUPPORT_INTERPOL
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse
CC = gcc

OBJS = $(TMP)RREngine.o $(TMP)rrcore.o $(TMP)dynamic.o $(TMP)geometry.o $(TMP)interpol.o $(TMP)intersections.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)
	copy include\*.* ..\..\include

clean:
	del $(TMP)*.o
	del $(LIB)

$(TMP)rrcore.o: makefile

$(TMP)dynamic.o: makefile

$(TMP)geometry.o: makefile

$(TMP)interpol.o: makefile

$(TMP)intersections.o: makefile

$(TMP)RREngine.o: makefile

