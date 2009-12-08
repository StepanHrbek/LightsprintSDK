# include platform-specific configuration

CFG_DIR = ../configs
-include $(CFG_DIR)/current

ifdef CONFIG
include $(CFG_DIR)/$(CONFIG)
endif

# general project settings

PROJECT = LightsprintCore

ifdef STATIC
CFG_TYPE = lib
TARGET = ../../lib/$(CONFIG)/lib$(PROJECT).a
else
CFG_TYPE = dll
TARGET = ../../bin/$(CONFIG)/lib$(PROJECT).so
endif

OBJ_DIR = ../../tmp/$(PROJECT)/$(CONFIG)

# project-dependent compiler/preprocessor flags

CPP_FLAGS += -DSUPPORT_INTERPOL
CPP_FLAGS += -DSUPPORT_OFFLINE
CPP_FLAGS += -DSUPPORT_FIREBALL
CPP_FLAGS += -DSUPPORT_ARCHITECT
CPP_FLAGS += -DSUPPORT_LIGHTFIELD
CPP_FLAGS += -DRR_STATIC

# source files

SOURCES = \
RRBuffer/RRBuffer.cpp \
RRBuffer/RRBufferInMemory.cpp \
RRReporter/RRReporter.cpp \
RRReporter/RRReporterFile.cpp \
RRReporter/RRReporterOutputDebugString.cpp \
RRReporter/RRReporterPrintf.cpp \
RRReporter/RRReporterWindow.cpp \
RRReporter/RRReportInterval.cpp \
RRCollider/bsp.cpp \
RRCollider/fpcube.cpp \
RRCollider/geometry.cpp \
RRCollider/IntersectBspCompact.cpp \
RRCollider/IntersectBspFast.cpp \
RRCollider/IntersectLinear.cpp \
RRCollider/IntersectVerification.cpp \
RRCollider/pcube.cpp \
RRCollider/RRCollider.cpp \
RRCollider/RRRay.cpp \
RRDynamicSolver/environmentMap.cpp \
RRDynamicSolver/gather.cpp \
RRDynamicSolver/Interpolator.cpp \
RRDynamicSolver/lightmap.cpp \
RRDynamicSolver/LightmapFilter.cpp \
RRDynamicSolver/RRDynamicSolver.cpp \
RRDynamicSolver/vertexBuffer.cpp \
RRHash/sha1.cpp \
RRHash/RRHash.cpp \
RRMesh/RRMesh.cpp \
RRMesh/RRMeshAccelerated.cpp \
RRMesh/RRMeshArrays.cpp \
RRMesh/RRMeshLessVertices.cpp \
RRObject/RRCollisionHandler.cpp \
RRObject/RRMaterial.cpp \
RRObject/RRObject.cpp \
RRObject/RRObjects.cpp \
RRObject/RRObjectWithPhysicalMaterials.cpp \
RRObject/RRScaler.cpp \
RRPackedSolver/PackedSolverFileBuild.cpp \
RRPackedSolver/RRPackedSolver.cpp \
RRStaticSolver/exceptions.cpp \
RRStaticSolver/gatherer.cpp \
RRStaticSolver/geometry_v.cpp \
RRStaticSolver/interpol.cpp \
RRStaticSolver/rrcore.cpp \
RRStaticSolver/RRStaticSolver.cpp \
RRStaticSolver/RRVision.cpp \
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
RRLight.cpp \
RRLightField.cpp \
RRMath.cpp \
RRMemory.cpp \
RRObjectIllumination.cpp \
RRScene.cpp

# libraries

LIBS += -lgomp

# include directories

INC_DIRS = .
INC_DIRS += ../../include
INC_DIRS += ../LicGen/include

# library directories

LIB_DIRS = .

# include the core part of the makefile

include $(CFG_DIR)/../makefile.core
