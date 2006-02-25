LIB = ../../lib/libRRCollider.a
TMP = #../../tmp/RRCollider/

CPPFLAGS = -Iinclude -I../LicGen/include
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp
CPPFLAGS += -msse
CPPFLAGS += -mfpmath=sse
CC = gcc

OBJS = $(TMP)bsp.o $(TMP)cache.o $(TMP)fpcube.o $(TMP)geometry.o $(TMP)IntersectBspCompact.o $(TMP)IntersectBspFast.o $(TMP)IntersectLinear.o $(TMP)IntersectVerification.o $(TMP)RRMath.o $(TMP)pcube.o $(TMP)RRAligned.o $(TMP)RRCollider.o $(TMP)RRRay.o $(TMP)sha1.o $(TMP)MeshImporter/CopyMesh.o $(TMP)MeshImporter/LessVertices.o $(TMP)MeshImporter/Mesh.o $(TMP)MeshImporter/MultiMesh.o
#OBJS = $(TMP)big.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libRRCollider.a

