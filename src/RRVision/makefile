DEBUG = 


LIB = ../../lib/libRRVision.a
TMP = #../../tmp/RRVision/

CC = gcc
CPPFLAGS = -Iinclude -I../../include -I../RRCollider/include -I../LicGen/include -DSUPPORT_INTERPOL -DRR_STATIC
ifndef DEBUG
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse
endif

OBJS  = $(TMP)RRObject/RRMaterial.o $(TMP)RRObject/RRObject.o $(TMP)RRObject/RRObjectWithPhysicalMaterials.o $(TMP)RRObject/RRScaler.o
OBJS += $(TMP)RRIllumination/RRIlluminationEnvironmentMapInMemory.o $(TMP)RRIllumination/RRIlluminationPixelBuffer.o $(TMP)RRIllumination/RRIlluminationVertexBufferInMemory.o $(TMP)RRIllumination/RRObjectIllumination.o
OBJS += $(TMP)RRStaticSolver/clusters.o $(TMP)RRStaticSolver/geometry.o $(TMP)RRStaticSolver/interpol.o $(TMP)RRStaticSolver/intersections.o $(TMP)RRStaticSolver/rrcore.o $(TMP)RRStaticSolver/RRStaticSolver.o $(TMP)RRStaticSolver/RRVision.o
OBJS += $(TMP)RRDynamicSolver/environmentMap.o $(TMP)RRDynamicSolver/Interpolator.o $(TMP)RRDynamicSolver/lightmap.o $(TMP)RRDynamicSolver/RRLight.o $(TMP)RRDynamicSolver/RRDynamicSolver.o $(TMP)RRDynamicSolver/vertexBuffer.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)RRObject\*.o
	del $(TMP)RRIllumination\*.o
	del $(TMP)RRStaticSolver\*.o
	del $(TMP)RRDynamicSolver\*.o
	del ..\..\lib\libRRVision.a

