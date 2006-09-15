LIB = ../../lib/libRRIllumination.a
TMP = #../../tmp/RRIllumination/

CC = gcc
CPPFLAGS = -Iinclude -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
CPPFLAGS += -mfpmath=sse

OBJS = $(TMP)RRIlluminationPixelBuffer.o $(TMP)RRIlluminationVertexBufferInMemory.o $(TMP)RRObjectIllumination.o
# $(TMP)RRIlluminationPixelBufferInMemory.o $(TMP)swraster.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libRRIllumination.a

