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
RRCollider/bsp.cpp \
RRCollider/cache.cpp \
RRCollider/fpcube.cpp \
RRCollider/geometry.cpp \
RRCollider/IntersectBspCompact.cpp \
RRCollider/IntersectBspFast.cpp \
RRCollider/IntersectLinear.cpp \
RRCollider/IntersectVerification.cpp \
RRCollider/pcube.cpp \
RRCollider/RRCollider.cpp \
RRCollider/RRRay.cpp \
RRCollider/sha1.cpp \
RRDynamicSolver/environmentMap.cpp \
RRDynamicSolver/gather.cpp \
RRDynamicSolver/Interpolator.cpp \
RRDynamicSolver/lightmap.cpp \
RRDynamicSolver/LightmapFilter.cpp \
RRDynamicSolver/RRDynamicSolver.cpp \
RRDynamicSolver/vertexBuffer.cpp \
RRMesh/RRChanneledData.cpp \
RRMesh/RRMatrix3x4.cpp \
RRMesh/RRMesh.cpp \
RRMesh/RRMeshAccelerated.cpp \
RRMesh/RRMeshCopy.cpp \
RRMesh/RRMeshLessVertices.cpp \
RRObject/RRCollisionHandler.cpp \
RRObject/RRMaterial.cpp \
RRObject/RRObject.cpp \
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
RRLight.cpp \
RRLightField.cpp \
RRMemory.cpp \
RRObjectIllumination.cpp \
RRReporter.cpp

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
