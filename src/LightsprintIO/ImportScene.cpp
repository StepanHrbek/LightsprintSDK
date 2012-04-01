// --------------------------------------------------------------------------
// Format independent scene import.
// Copyright (C) 2006-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/IO/ImportScene.h"

#include "supported_formats.h"
#include "ImportImages/ImportFreeImage.h"
#include "ImportDirectShow/RRBufferDirectShow.h"
#include "ImportLightsprint/RRObjectLightsprint.h"
#include "Import3DS/RRObject3DS.h"
#include "ImportAssimp/RRObjectAssimp.h"
#include "ImportGamebryo/RRObjectGamebryo.h"
#include "ImportIsolation/RRSceneIsolation.h"
#include "ImportOpenCollada/RRObjectOpenCollada.h"
#include "ImportFCollada/RRObjectFCollada.h"
#include "ImportQuake3/RRObjectBSP.h"
#include "ImportMGF/RRObjectMGF.h"
#include "ImportOBJ/RRObjectOBJ.h"

void rr_io::registerLoaders()
{
	// if multiple loaders support the same format,
	// register more robust implementations first

#ifdef SUPPORT_IMAGES
	registerLoaderImages();
#endif

#ifdef SUPPORT_DIRECTSHOW
	registerLoaderDirectShow();
#endif

#ifdef SUPPORT_LIGHTSPRINT
	registerLoaderLightsprint();
#endif

#ifdef SUPPORT_GAMEBRYO
	registerLoaderGamebryo();
#endif

#ifdef SUPPORT_QUAKE3
	// not isolated (goes before isolation) because quake's links to textures are .bsp specific, .rr3 loader would not understand them
	// (can be fixed by completing links inside quake loader, before they are passed to file locator)
	registerLoaderQuake3();
#endif

#ifdef SUPPORT_ISOLATION
	// anything registered before stays as is
	// anything registred later becomes isolated
	registerLoaderIsolationStep1();
#endif

#ifdef SUPPORT_OPENCOLLADA
	// is better than FCollada
	registerLoaderOpenCollada();
#endif

#ifdef SUPPORT_FCOLLADA
	// is better than Assimp
	registerLoaderFCollada();
#endif

#ifdef SUPPORT_3DS
	// comparable to assimp, better smoothing, but no lights, no transformations
	registerLoader3DS();
#endif

#ifdef SUPPORT_ASSIMP
	// is better than obj
	registerLoaderAssimp();
#endif

#ifdef SUPPORT_MGF
	registerLoaderMGF();
#endif

#ifdef SUPPORT_OBJ
	registerLoaderOBJ();
#endif
}

void rr_io::isolateSceneLoaders()
{
#ifdef SUPPORT_ISOLATION
	// note that when we are called to do isolated conversion, step 2 converts scene and then exits program
	registerLoaderIsolationStep2();
#endif
}

