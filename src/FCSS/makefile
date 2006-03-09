CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -I../RRCollider/include -I../RRVision/include -I../LicGen/include -I../../3rd/mgflib -DGLEW_STATIC -DRRCOLLIDER_STATIC -DRRVISION_STATIC
CPPFLAGS += -O1 -DNDEBUG
#CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG // zpusobuje crash po pouziti upx
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 #CPPFLAGS += -march=athlon-xp
#CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = -s -lz -lm -lmgf -L../../lib -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = 3ds2rr.o glew.o ldmgf.o matrix.o mgf2rr.o Model_3DS.o rr2gl.o shadowcast.o glsl/Camera.o glsl/FrameRate.o glsl/GLSLObject.o glsl/GLSLProgram.o glsl/GLSLShader.o glsl/Texture.o

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
	del ..\..\bin\fcss.exe
