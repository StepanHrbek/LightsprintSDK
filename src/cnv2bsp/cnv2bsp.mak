OBJS = viewer.o cnv2bsp.o load_3ds.o load_mgf.o

CC=gcc
CFLAGS=-O6 -fno-exceptions -ffast-math -finline -fomit-frame-pointer -D__OPTIMIZE__
#CFLAGS=-g -Wall
LIBS=-lopengl32 -lglu32 -lgdi32 -lmgf -lm

all: cnv2bsp

viewer.o: viewer.cpp cnv2bsp.h
	gcc $(CFLAGS) -c viewer.cpp -o $@
cnv2bsp.o: cnv2bsp.c cnv2bsp.h
	gcc $(CFLAGS) -c cnv2bsp.c -o $@
load_3ds.o: load_3ds.c cnv2bsp.h
	gcc $(CFLAGS) -c load_3ds.c -o $@
load_mgf.o: load_mgf.c cnv2bsp.h parser.h
	gcc $(CFLAGS) -c load_mgf.c -o $@

cnv2bsp: $(OBJS)
	gcc -s -o cnv2bsp.exe $(OBJS) $(LIBS)