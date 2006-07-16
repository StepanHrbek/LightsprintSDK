DEBUG = 


LIB = ../../lib/libWorld.a
TMP = #../../tmp/World/

CC = gcc
CPPFLAGS = -Iinclude
ifndef DEBUG
CPPFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse
endif

OBJS = $(TMP)spline.o $(TMP)LoadWorld.o $(TMP)LoadWorldR3d.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	del $(TMP)*.o
	del ..\..\lib\libWorld.a

$(TMP)spline.o $(TMP)LoadWorld.o $(TMP)LoadWorldR3d.o: makefile include/spline.h include/World.h
