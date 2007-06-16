LIB = ../../lib/libRRMesh.a
TMP = #../../tmp/RRMesh/

CC = gcc
CPPFLAGS = -Iinclude -I../../include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG -DRR_STATIC
#CPPFLAGS += -ffast-math # zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
CPPFLAGS += -mfpmath=sse
#CPPFLAGS += -ftree-vectorize -ftree-vectorizer-verbose=5
#CPPFLAGS += -funsafe-loop-optimizations

OBJS = $(TMP)RRChanneledData.o $(TMP)RRMesh.o $(TMP)RRMeshCopy.o $(TMP)RRMeshLessVertices.o $(TMP)RRMeshMulti.o $(TMP)RRReporter.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libRRMesh.a

