LIB = ../../lib/libDemoEngine.a
TMP = #../../tmp/RRVision/

CC = gcc
CPPFLAGS = -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DGLEW_STATIC
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse

OBJS = $(TMP)3ds2rr.o $(TMP)camera.o $(TMP)glew.o $(TMP)matrix.o $(TMP)Model_3DS.o $(TMP)Program.o $(TMP)RendererOfRRObject.o $(TMP)RendererWithCache.o $(TMP)Shader.o $(TMP)Texture.o $(TMP)TextureShadowMap.o $(TMP)UberProgram.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libDemoEngine.a

