CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -DGLEW_STATIC -DRR_STATIC
#CPPFLAGS += -O1 -DNDEBUG
#CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG // zpusobuje crash po pouziti upx
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 #CPPFLAGS += -march=athlon-xp
#CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = -s -lz -lm -L../../lib -lRRIllumCalculator -lRRMesh -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = 3ds2rr.o Model_3DS.o rr2gl.o shadowcast.o gl/Camera.o gl/glew.o gl/matrix.o gl/Program.o gl/Shader.o gl/Texture.o gl/TextureShadowmap.o gl/UberProgram.o

all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile ../../lib/libRRCollider.a ../../lib/libRRVision.a
	gcc $(OBJS) $(LIBS) -o $(BIN)fcss.exe

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del gl\*.o
	del ..\..\bin\fcss.exe
