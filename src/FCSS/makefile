CC = gcc
CPP = gcc
CPPFLAGS = -Wall -I. -I../../include -DRR_STATIC -DGLEW_STATIC
#CPPFLAGS += -O1 -DNDEBUG
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
 #CPPFLAGS += -ffast-math // zpusobuje crash v soda
 #CPPFLAGS += -fomit-frame-pointer // zpusobuje crash
 CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
#CPPFLAGS += -mfpmath=sse
LIBS = -s -lz -lm -L../../lib -lDemoEngine -lRRRealtimeRadiosity -lRRIllumination -lRRMesh -lRRCollider -lRRVision -lstdc++
LIBS += -lopengl32 -lglu32 -lglut32
BIN = ../../bin/

OBJS = shadowcast.o Bugs.o

all: $(BIN)fcss.exe

$(BIN)fcss.exe: $(OBJS) makefile ../../lib/libDemoEngine.a ../../lib/libRRCollider.a ../../lib/libRRVision.a ../../lib/libRRIllumination.a
	gcc $(OBJS) $(LIBS) -o $(BIN)fcss.exe

$(TMP)%.o: %.cpp makefile

$(TMP)%.o: %.c makefile

clean:
	del *.o
	del ..\..\bin\fcss.exe
