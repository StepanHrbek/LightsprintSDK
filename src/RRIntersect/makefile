LIB = ../../lib/libRRIntersect.a
TMP = #../../tmp/RRIntersect/

CPPFLAGS = -Iinclude
CPPFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse --param max-inline-insns-auto=300
#CPPFLAGS += -mfpmath=sse
CC = gcc

OBJS = $(TMP)bsp.o $(TMP)cache.o $(TMP)geometry.o $(TMP)IntersectBspFast.o $(TMP)IntersectBspCompact.o $(TMP)IntersectLinear.o $(TMP)RRIntersect.o $(TMP)sha1.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)
	copy include\*.* ..\..\include

clean:
	del $(TMP)*.o
	del $(LIB)

$(TMP)bsp.o: makefile

$(TMP)cache.o $(TMP)RRIntersect.o: makefile include/RRIntersect.h

$(TMP)geometry.o: makefile

$(TMP)IntersectLinear.o $(TMP)IntersectBspFast.o $(TMP)IntersectBspCompact.o: makefile IntersectLinear.h

$(TMP)sha1.o: makefile sha1.h
