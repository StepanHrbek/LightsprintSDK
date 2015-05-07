// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Registers available importers and exporters.
// --------------------------------------------------------------------------

#include "Lightsprint/IO/IO.h"

#include "supported_formats.h"
#include "ImportImages/ImportFreeImage.h"
#include "ImportFFmpeg/RRBufferFFmpeg.h"
#include "ImportDirectShow/RRBufferDirectShow.h"
#include "ImportLightsprint/RRObjectLightsprint.h"
#include "Import3DS/RRObject3DS.h"
#include "ImportAssimp/RRObjectAssimp.h"
#include "ImportIsolation/RRSceneIsolation.h"
#include "ImportOpenCollada/RRObjectOpenCollada.h"
#include "ImportFCollada/RRObjectFCollada.h"
#include "ImportQuake3/RRObjectBSP.h"
#include "ImportMGF/RRObjectMGF.h"
#include "ImportOBJ/RRObjectOBJ.h"
#include "SmallLuxGpu/SmallLuxGpu.h"

void rr_io::registerLoaders(int argc, char** argv, unsigned phase)
{
if (phase==0 || phase==1)
{
	// if multiple loaders support the same format,
	// register more robust implementations first

#ifdef SUPPORT_IMAGES
	registerLoaderImages();
#endif

#ifdef SUPPORT_LIGHTSPRINT
	registerLoaderLightsprint();
#endif

#ifdef SUPPORT_FFMPEG
	// should be the last one, because it is probably very slow in rejecting unsupported formats
	registerLoaderFFmpeg();
#endif

#ifdef SUPPORT_DIRECTSHOW
	// should be the last one, because it is probably very slow in rejecting unsupported formats
	registerLoaderDirectShow();
#endif

#ifdef SUPPORT_QUAKE3
	// not isolated (goes before isolation) because quake's links to textures are .bsp specific, .rr3 loader would not understand them
	// (can be fixed by completing links inside quake loader, before they are passed to file locator)
	registerLoaderQuake3();
#endif

#ifdef SUPPORT_ISOLATION
	// anything registered before stays as is
	// anything registred later becomes isolated
	registerLoaderIsolationStep1(argc,argv);
#endif

}
if (phase==0 || phase==2)
{

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

#ifdef SUPPORT_SMALLLUXGPU
	registerSmallLuxGpu();
#endif

}
}

void rr_io::isolateSceneLoaders(int argc, char** argv)
{
#ifdef SUPPORT_ISOLATION
	// note that when we are called to do isolated conversion, step 2 converts scene and then exits program
	registerLoaderIsolationStep2(argc,argv);
#endif
}

