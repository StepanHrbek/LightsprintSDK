DEBUG    = 
TMP      = #../../tmp/LightsprintGL/
CPPFLAGS = -I../../include -I../LicGen/include -DSUPPORT_INTERPOL -DRR_STATIC

ifdef DEBUG
	LIB      = ../../lib/win32/libLightsprintGL_sd.a
	CPPFLAGS += -D_DEBUG
else
	LIB      = ../../lib/win32/libLightsprintGL_sr.a
	CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
endif

OBJS  = $(TMP)DemoEngine/AreaLight.o \
	$(TMP)DemoEngine/Camera.o \
	$(TMP)DemoEngine/FBO.o \
	$(TMP)DemoEngine/matrix.o \
	$(TMP)DemoEngine/Program.o \
	$(TMP)DemoEngine/RendererWithCache.o \
	$(TMP)DemoEngine/Shader.o \
	$(TMP)DemoEngine/Texture.o \
	$(TMP)DemoEngine/TextureGL.o \
	$(TMP)DemoEngine/TextureInMemory.o \
	$(TMP)DemoEngine/TextureShadowMap.o \
	$(TMP)DemoEngine/TextureRenderer.o \
	$(TMP)DemoEngine/UberProgram.o \
	$(TMP)DemoEngine/UberProgramSetup.o \
	$(TMP)Integration/LightmapViewer.o \
	$(TMP)Integration/ObjectBuffers.o \
	$(TMP)Integration/RendererOfRRObject.o \
	$(TMP)Integration/RendererOfScene.o \
	$(TMP)Integration/RRDynamicSolverGL.o \
	$(TMP)Integration/RRIlluminationEnvironmentMapInOpenGL.o \
	$(TMP)Integration/RRIlluminationPixelBufferInOpenGL.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	del /s *.o
	del ..\..\lib\win32\libLightsprintGL_sd.a
	del ..\..\lib\win32\libLightsprintGL_sr.a
