# include platform-specific configuration

CFG_DIR = ../configs
-include $(CFG_DIR)/current

ifdef CONFIG
include $(CFG_DIR)/$(CONFIG)
endif

# general project settings

PROJECT = LightsprintCore

ifeq ($(LIB_DIR), lib)
CFG_TYPE = lib
else
CFG_TYPE = dll
endif
TARGET = ../../$(LIB_DIR)/$(CONFIG)/lib$(PROJECT).$(LIB_EXT)

OBJ_DIR = ../../tmp/$(PROJECT)/$(CONFIG)

# project-dependent compiler/preprocessor flags

CPP_FLAGS += -DRR_BUILD
#CPP_FLAGS += -DSUPPORT_EMBREE

# source files

SOURCES = \
RRBuffer/RRBuffer.cpp \
RRBuffer/RRBufferBlend.cpp \
RRBuffer/RRBufferInMemory.cpp \
RRReporter/RRReporter.cpp \
RRReporter/RRReporterFile.cpp \
RRReporter/RRReporterOutputDebugString.cpp \
RRReporter/RRReporterPrintf.cpp \
RRReporter/RRReporterWindow.cpp \
RRReporter/RRReportInterval.cpp \
RRCollider/bsp.cpp \
RRCollider/EmbreeCollider.cpp \
RRCollider/fpcube.cpp \
RRCollider/geometry.cpp \
RRCollider/IntersectBspCompact.cpp \
RRCollider/IntersectBspFast.cpp \
RRCollider/IntersectLinear.cpp \
RRCollider/IntersectVerification.cpp \
RRCollider/pcube.cpp \
RRCollider/RRCollider.cpp \
RRCollider/RRColliderMulti.cpp \
RRCollider/RRRay.cpp \
RRHash/sha1.cpp \
RRHash/RRHash.cpp \
RRMesh/RRMesh.cpp \
RRMesh/RRMeshAccelerated.cpp \
RRMesh/RRMeshArrays.cpp \
RRMesh/RRMeshLessVertices.cpp \
RRObject/RRMaterial.cpp \
RRObject/RRObject.cpp \
RRObject/RRObjects.cpp \
RRPackedSolver/PackedSolverFileBuild.cpp \
RRPackedSolver/RRPackedSolver.cpp \
RRSolver/environmentMap.cpp \
RRSolver/gather.cpp \
RRSolver/lightmap.cpp \
RRSolver/RRSolver.cpp \
RRSolver/vertexBuffer.cpp \
RRStaticSolver/geometry_v.cpp \
RRStaticSolver/interpol.cpp \
RRStaticSolver/pathtracer.cpp \
RRStaticSolver/rrcore.cpp \
RRStaticSolver/RRStaticSolver.cpp \
squish/alpha.cpp \
squish/clusterfit.cpp \
squish/colourblock.cpp \
squish/colourfit.cpp \
squish/colourset.cpp \
squish/maths.cpp \
squish/rangefit.cpp \
squish/singlecolourfit.cpp \
squish/squish.cpp \
Preferences.cpp \
RRCamera.cpp \
RRColorSpace.cpp \
RRFileLocator.cpp \
RRLight.cpp \
RRLightField.cpp \
RRMath.cpp \
RRMemory.cpp \
RRObjectIllumination.cpp \
RRScene.cpp \
RRTime.cpp \
Unwrapper.cpp

# libraries

LIBS += $(CORE_DEPS)

# include directories

INC_DIRS = .
INC_DIRS += ../../include

# library directories

LIB_DIRS = .

# include the core part of the makefile

include $(CFG_DIR)/../makefile.core
