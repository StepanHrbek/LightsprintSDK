LIB = ../../lib/libRRIntersect.a
TMP = #../../tmp/RRIntersect/

MACH = 
OPT = -O3 -Iinclude
CPPFLAGS = $(MACH) $(OPT)
CC = gcc

OBJS = $(TMP)bsp.o $(TMP)cache.o $(TMP)geometry.o $(TMP)IntersectBsp.o $(TMP)IntersectKd.o $(TMP)IntersectLinear.o $(TMP)RRIntersect.o $(TMP)sha1.o

all:	$(LIB)

$(LIB):	$(OBJS)
	ar rc $(LIB) $(OBJS)
	-ranlib $(LIB)

clean:
	del -f $(TMP)*.o $(LIB)

$(TMP)bsp.o:	

$(TMP)cache.o $(TMP)RRIntersect.o:	include/RRIntersect.h

$(TMP)geometry.o:	

$(TMP)IntersectLinear.o $(TMP)IntersectBsp.o $(TMP)IntersectKd.o:	IntersectLinear.h

$(TMP)sha1.o:	sha1.h
