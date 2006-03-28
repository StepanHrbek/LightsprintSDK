#
# SCCSid "@(#)Makefile 1.9 2/8/96 LBL"
# Makefile for MGF parser library
#
# Use -DNOPROTO flag to get back K&R compatibility.
# Use -DBSD flag if your UNIX has a strong BSD flavor.
# Use '-DMEM_PTR=char *' if your malloc return type is not 'void *'.
MACH = 
OPT = -O3 -Dregister=
CFLAGS = $(MACH) $(OPT)
CC = gcc

OBJS = parser.o context.o xf.o object.o lookup.o badarg.o \
words.o fvect.o

all:	libmgf.a mgfilt mgf2inv

mgfilt:	libmgf.a mgfilt.o
	$(CC) $(CFLAGS) -o mgfilt mgfilt.o libmgf.a -lm

libmgf.a:	$(OBJS)
	ar rc libmgf.a $(OBJS)
	-ranlib libmgf.a

mgf2inv:	libmgf.a mgf2inv.o cvrgb.o
	$(CC) $(CFLAGS) -o mgf2inv mgf2inv.o cvrgb.o libmgf.a -lm

3ds2mgf:	3ds2mgf.o rayopt.o vect.o
	$(CC) $(CFLAGS) -o 3ds2mgf 3ds2mgf.o rayopt.o vect.o -lm

clean:
	set nonomatch ; rm -f *.o libmgf.a mgfilt mgf2inv 3ds2mgf

parser.o context.o xf.o object.o fvect.o mgf2inv.o cvrgb.o:	mgfparser.h

parser.o context.o lookup.o:	lookup.h

parser.o:	messages.h

mgfilt.o:	mgfparser.h

3ds2mgf.o rayopt.o:	rayopt.h

3ds2mgf.o rayopt.o vect.o:	vect.h
