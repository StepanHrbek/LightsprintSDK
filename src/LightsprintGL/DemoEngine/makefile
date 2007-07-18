LIB = ../../lib/libDemoEngine.a
TMP = #../../tmp/RRVision/

CC = gcc
CPPFLAGS = -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse

# -DGLEW_STATIC
#glew32s.lib FreeImage.lib

OBJS = $(TMP)AreaLight.o $(TMP)Camera.o $(TMP)FBO.o $(TMP)matrix.o $(TMP)Model_3DS.o $(TMP)Program.o $(TMP)RendererOf3DS.o $(TMP)RendererWithCache.o $(TMP)Shader.o $(TMP)TextureFromDisk.o $(TMP)TextureGL.o $(TMP)TextureShadowMap.o $(TMP)UberProgram.o $(TMP)UberProgramSetup.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libDemoEngine.a

