DEBUG    = 
TMP      = #../../tmp/LightsprintCore/
CPPFLAGS = -I../../include -I../LicGen/include -DSUPPORT_INTERPOL -DRR_STATIC

ifdef DEBUG
	LIB      = ../../lib/win32/libLightsprintCore_sd.a
	CPPFLAGS += -D_DEBUG
else
	LIB      = ../../lib/win32/libLightsprintCore_sr.a
	CPPFLAGS += -O3 -DNDEBUG
	CPPFLAGS += -ffast-math
	CPPFLAGS += -fomit-frame-pointer
	CPPFLAGS += -march=k8 -msse
	CPPFLAGS += -mfpmath=sse
endif

OBJS  = $(TMP)RRMemory.o $(TMP)RRReporter.o
OBJS += $(TMP)RRMesh/RRChanneledData.o $(TMP)RRMesh/RRMesh.o $(TMP)RRMesh/RRMeshCopy.o $(TMP)RRMesh/RRMeshLessVertices.o $(TMP)RRMesh/RRMeshMulti.o
OBJS += $(TMP)RRCollider/bsp.o $(TMP)RRCollider/cache.o $(TMP)RRCollider/fpcube.o $(TMP)RRCollider/geometry.o $(TMP)RRCollider/IntersectBspCompact.o $(TMP)RRCollider/IntersectBspFast.o $(TMP)RRCollider/IntersectLinear.o $(TMP)RRCollider/IntersectVerification.o $(TMP)RRCollider/pcube.o $(TMP)RRCollider/RRCollider.o $(TMP)RRCollider/RRRay.o $(TMP)RRCollider/sha1.o
OBJS += $(TMP)RRObject/RRCollisionHandler.o $(TMP)RRObject/RRMaterial.o $(TMP)RRObject/RRMatrix3x4.o $(TMP)RRObject/RRObject.o $(TMP)RRObject/RRObjectWithPhysicalMaterials.o $(TMP)RRObject/RRScaler.o
OBJS += $(TMP)RRIllumination/RRIlluminationEnvironmentMapInMemory.o $(TMP)RRIllumination/RRIlluminationPixelBuffer.o $(TMP)RRIllumination/RRIlluminationVertexBufferInMemory.o $(TMP)RRIllumination/RRObjectIllumination.o
OBJS += $(TMP)RRStaticSolver/clusters.o $(TMP)RRStaticSolver/gatherer.o $(TMP)RRStaticSolver/geometry_v.o $(TMP)RRStaticSolver/interpol.o $(TMP)RRStaticSolver/rrcore.o $(TMP)RRStaticSolver/RRStaticSolver.o $(TMP)RRStaticSolver/RRVision.o
OBJS += $(TMP)RRDynamicSolver/environmentMap.o $(TMP)RRDynamicSolver/Interpolator.o $(TMP)RRDynamicSolver/lightmap.o $(TMP)RRDynamicSolver/RRLight.o $(TMP)RRDynamicSolver/RRDynamicSolver.o $(TMP)RRDynamicSolver/vertexBuffer.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	del /s *.o
	del ..\..\lib\win32\libLightsprintCore_sd.a
	del ..\..\lib\win32\libLightsprintCore_sr.a
