#LIB =  ../../lib/
#OBJS = ../../tmp/RRIntersect/*.o
#../../tmp/RRIntersect/sha1.o: gcc 

MACH = 
OPT = -O3
CFLAGS = $(MACH) $(OPT)
CC = gcc

OBJS = bsp.o cache.o geometry.o IntersectBsp.o IntersectKd.o IntersectLinear.o RRIntersect.o sha1.o

all:	libRRIntersect.a

libRRIntersect.a:	$(OBJS)
	ar rc libRRIntersect.a $(OBJS)
	-ranlib libRRIntersect.a

clean:
	set nonomatch ; rm -f *.o libRRIntersect.a

bsp.o:	

cache.o:	RRIntersect.h

geometry.o:	

IntersectLinear.o IntersectBsp.o IntersectKd.o:	IntersectLinear.h

RRIntersect.o:	RRIntersect.h

sha1.o:	sha1.h
