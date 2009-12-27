// --------------------------------------------------------------------------
// Format independent scene import.
// Copyright (C) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/IO/ImportScene.h"

#include "supported_formats.h"
#include "ImportImages/ImportFreeImage.h"
#include "Import3DS/RRObject3DS.h"
#include "ImportAssimp/RRObjectAssimp.h"
#include "ImportGamebryo/RRObjectGamebryo.h"
#include "ImportCollada/RRObjectCollada.h"
#include "ImportQuake3/RRObjectBSP.h"
#include "ImportMGF/RRObjectMGF.h"
#include "ImportOBJ/RRObjectOBJ.h"

void rr_io::registerLoaders()
{
	// if multiple loaders support the same format,
	// register more robust implementations first
	// (so Assimp goes after Collada but before obj)

#ifdef SUPPORT_IMAGES
	registerLoaderImages();
#endif

#ifdef SUPPORT_GAMEBRYO
	registerLoaderGamebryo();
#endif

#ifdef SUPPORT_COLLADA
	registerLoaderCollada();
#endif

#ifdef SUPPORT_QUAKE3
	registerLoaderQuake3();
#endif

#ifdef SUPPORT_ASSIMP
	registerLoaderAssimp();
#endif

#ifdef SUPPORT_3DS
	registerLoader3DS();
#endif

#ifdef SUPPORT_MGF
	registerLoaderMGF();
#endif

#ifdef SUPPORT_OBJ
	registerLoaderOBJ();
#endif
}

