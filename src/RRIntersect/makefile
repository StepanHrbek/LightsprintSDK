LIB = ../../lib/libRRIntersect.a
TMP = #../../tmp/RRIntersect/

MACH = 
OPT = -O3 -Iinclude
CPPFLAGS = $(MACH) $(OPT)
CPPFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
CPPFLAGS += -march=athlon-xp --param max-inline-insns-auto=300
#CPPFLAGS += -march=athlon-xp -msse -mfpmath=sse --param max-inline-insns-auto=300
#CPPFLAGS += -fomit-frame-pointer
CC = gcc

OBJS = $(TMP)bsp.o $(TMP)cache.o $(TMP)geometry.o $(TMP)IntersectBsp.o $(TMP)IntersectKd.o $(TMP)IntersectLinear.o $(TMP)RRIntersect.o $(TMP)sha1.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	del $(TMP)*.o
	del $(LIB)

$(TMP)bsp.o: makefile

$(TMP)cache.o $(TMP)RRIntersect.o: makefile include/RRIntersect.h

$(TMP)geometry.o: makefile

$(TMP)IntersectLinear.o $(TMP)IntersectBsp.o $(TMP)IntersectKd.o: makefile IntersectLinear.h

$(TMP)sha1.o: makefile sha1.h
