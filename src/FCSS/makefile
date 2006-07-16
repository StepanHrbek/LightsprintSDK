CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -I../../3rd/mgflib -DGLEW_STATIC -DRR_STATIC
CPPFLAGS += -O1 -DNDEBUG
#CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG // zpusobuje crash po pouziti upx
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 #CPPFLAGS += -march=athlon-xp
#CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = -s -lz -lm -lmgf -L../../lib -lRRMesh -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = 3ds2rr.o Model_3DS.o rr2gl.o shadowcast.o gl/Camera.o gl/glew.o glsl/matrix.o glsl/Program.o glsl/Shader.o glsl/Texture.o glsl/UberProgram.o

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
	del gl\*.o
	del ..\..\bin\fcss.exe
