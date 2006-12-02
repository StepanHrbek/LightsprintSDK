LIB = ../../lib/libRRRealtimeRadiosity.a
TMP = #../../tmp/RRRealtimeRadiosity/

CC = gcc
CPPFLAGS = -I../../include -DRR_STATIC
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse

OBJS = $(TMP)ambientMap.o $(TMP)environmentMap.o $(TMP)RRRealtimeRadiosity.o $(TMP)vertexBuffer.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libRRRealtimeRadiosity.a

