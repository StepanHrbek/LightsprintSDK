LIB = ../../lib/libRRGPUOpenGL.a

CC = gcc
CPPFLAGS = -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
# -DGLEW_STATIC
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse

OBJS = RendererOfRRObject.o RRGPUOpenGL.o RRIlluminationEnvironmentMapInOpenGL.o RRIlluminationPixelBufferInOpenGL.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

%.o: %.cpp makefile

clean:
	del *.o
	del ..\..\lib\libRRGPUOpenGL.a

