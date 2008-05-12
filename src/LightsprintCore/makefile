PLATFORM = linux64
DEBUG    = 1
TMP      = #../../tmp/LightsprintCore/
CPPFLAGS = -I../../include -I../LicGen/include -DSUPPORT_INTERPOL -DSUPPORT_OFFLINE -DSUPPORT_FIREBALL -DSUPPORT_ARCHITECT -DRR_STATIC

ifdef DEBUG
	LIB      = ../../lib/$(PLATFORM)/libLightsprintCore_sd.a
	CPPFLAGS += -g -D_DEBUG
else
	LIB      = ../../lib/$(PLATFORM)/libLightsprintCore_sr.a
	CPPFLAGS += -O3 -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
	CPPFLAGS += -fopenmp
endif

OBJS  = $(TMP)RRLight.o $(TMP)RRMemory.o $(TMP)RRLightField.o $(TMP)RRObjectIllumination.o $(TMP)RRReporter.o
OBJS += $(TMP)RRBuffer/RRBuffer.o $(TMP)RRBuffer/RRBufferInMemory.o
OBJS += $(TMP)RRCollider/bsp.o $(TMP)RRCollider/cache.o $(TMP)RRCollider/fpcube.o $(TMP)RRCollider/geometry.o $(TMP)RRCollider/IntersectBspCompact.o $(TMP)RRCollider/IntersectBspFast.o $(TMP)RRCollider/IntersectLinear.o $(TMP)RRCollider/IntersectVerification.o $(TMP)RRCollider/pcube.o $(TMP)RRCollider/RRCollider.o $(TMP)RRCollider/RRRay.o $(TMP)RRCollider/sha1.o
OBJS += $(TMP)RRDynamicSolver/environmentMap.o $(TMP)RRDynamicSolver/gather.o $(TMP)RRDynamicSolver/Interpolator.o $(TMP)RRDynamicSolver/lightmap.o $(TMP)RRDynamicSolver/LightmapFilter.o $(TMP)RRDynamicSolver/RRDynamicSolver.o $(TMP)RRDynamicSolver/vertexBuffer.o
OBJS += $(TMP)RRMesh/RRChanneledData.o $(TMP)RRMesh/RRMatrix3x4.o $(TMP)RRMesh/RRMesh.o $(TMP)RRMesh/RRMeshAccelerated.o $(TMP)RRMesh/RRMeshCopy.o $(TMP)RRMesh/RRMeshLessVertices.o
OBJS += $(TMP)RRObject/RRCollisionHandler.o $(TMP)RRObject/RRMaterial.o $(TMP)RRObject/RRObject.o $(TMP)RRObject/RRObjectWithPhysicalMaterials.o $(TMP)RRObject/RRScaler.o
OBJS += $(TMP)RRPackedSolver/PackedSolverFileBuild.o $(TMP)RRPackedSolver/RRPackedSolver.o
OBJS += $(TMP)RRStaticSolver/exceptions.o $(TMP)RRStaticSolver/gatherer.o $(TMP)RRStaticSolver/geometry_v.o $(TMP)RRStaticSolver/interpol.o $(TMP)RRStaticSolver/rrcore.o $(TMP)RRStaticSolver/RRStaticSolver.o $(TMP)RRStaticSolver/RRVision.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	rm -f *.o RRBuffer/*.o RRCollider/*.o RRDynamicSolver/*.o RRMesh/*.o RRObject/*.o RRPackedSolver/*.o RRStaticSolver/*.o ../../lib/$(PLATFORM)/libLightsprintCore_sd.a ../../lib/$(PLATFORM)/libLightsprintCore_sr.a

cleanwin:
	del /s *.o
	del ..\..\lib\$(PLATFORM)\libLightsprintCore_sd.a
	del ..\..\lib\$(PLATFORM)\libLightsprintCore_sr.a
