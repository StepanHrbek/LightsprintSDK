PLATFORM = linux64
DEBUG    = 1
TMP      = #../../tmp/LightsprintGL/
CPPFLAGS = -I../../include -DRR_STATIC

ifdef DEBUG
	LIB      = ../../lib/$(PLATFORM)/libLightsprintGL_sd.a
	CPPFLAGS += -g -D_DEBUG
else
	LIB      = ../../lib/$(PLATFORM)/libLightsprintGL_sr.a
	CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
	CPPFLAGS += -fopenmp
endif

OBJS  = $(TMP)Camera.o \
	$(TMP)FBO.o \
	$(TMP)LightmapViewer.o \
	$(TMP)MultiPass.o \
	$(TMP)ObjectBuffers.o \
	$(TMP)Program.o \
	$(TMP)RRDynamicSolverGL.o \
	$(TMP)RealtimeLight.o \
	$(TMP)RendererOfRRObject.o \
	$(TMP)RendererOfScene.o \
	$(TMP)RendererWithCache.o \
	$(TMP)SceneViewer.o \
	$(TMP)Shader.o \
	$(TMP)Texture.o \
	$(TMP)TextureRenderer.o \
	$(TMP)UberProgram.o \
	$(TMP)UberProgramSetup.o \
	$(TMP)matrix.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	rm *.o ../../lib/$(PLATFORM)/libLightsprintGL_sd.a ../../lib/$(PLATFORM)/libLightsprintGL_sr.a
	del *.o
	del ..\..\lib\$(PLATFORM)\libLightsprintGL_sd.a
	del ..\..\lib\$(PLATFORM)\libLightsprintGL_sr.a
