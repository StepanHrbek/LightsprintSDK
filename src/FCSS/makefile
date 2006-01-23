CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I. -I../RRCollider/include -I../RRVision/include -I../LicGen/include -DGLEW_STATIC
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
CPPFLAGS += -mfpmath=sse
LIBS = -s -lz -lm -lmgf -L../../lib -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = 3ds2rr.o glew.o ldmgf.o matrix.o mgf2rr.o Model_3DS.o rr2gl.o shadowcast.o glsl/Camera.o glsl/FrameRate.o glsl/GLSLObject.o glsl/GLSLProgram.o glsl/GLSLShader.o glsl/Light.o glsl/Texture.o
# GL/glprocs.o 


all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile ../../lib/libRRCollider.a ../../lib/libRRVision.a
	gcc $(OBJS) $(LIBS) -o $(BIN)fcss.exe

../../lib/libRRCollider.a:
	cd ../RRCollider & make

../../lib/libRRVision.a:
	cd ../RRVision & make

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del glsl\*.o
	del $(BIN)fcss.exe
