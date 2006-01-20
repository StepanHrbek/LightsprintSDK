CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I../RRCollider/include -I../RRVision/include -I../LicGen/include
LIBS = -lz -lm -lmgf -L../../lib -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = 3ds2rr.o GL/glprocs.o ldmgf.o matrix.o mgf2rr.o Model_3DS.o rr2gl.o shadowcast.o tga.o glsl/Camera.o glsl/FrameRate.o glsl/GLSLObject.o glsl/GLSLProgram.o glsl/GLSLShader.o glsl/Light.o glsl/Texture.o


all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile ../../lib/libRRCollider.a ../../lib/libRRVision.a
	gcc $(OBJS) $(LIBS) -o $(BIN)fcss.exe

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del $(BIN)fcss.exe
