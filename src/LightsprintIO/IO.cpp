// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Registers available importers and exporters.
// --------------------------------------------------------------------------

#include "Lightsprint/IO/IO.h"

#include "supported_formats.h"
#include "FreeImage/ImportFreeImage.h"
#include "SDL_image/ImportSDL_image.h"
#include "FFmpeg/RRBufferFFmpeg.h"
#include "DirectShow/RRBufferDirectShow.h"
#include "Lightsprint/RRObjectLightsprint.h"
#include "3DS/RRObject3DS.h"
#include "Assimp/RRObjectAssimp.h"
#include "Isolation/RRSceneIsolation.h"
#include "OpenCollada/RRObjectOpenCollada.h"
#include "FCollada/RRObjectFCollada.h"
#include "Quake3/RRObjectBSP.h"
#include "MGF/RRObjectMGF.h"
#include "OBJ/RRObjectOBJ.h"
#include "SmallLuxGpu/SmallLuxGpu.h"

void rr_io::registerIO(int argc, char** argv, unsigned phase)
{
if (phase==0 || phase==1)
{
	// if multiple loaders support the same format,
	// register more robust implementations first

#ifdef SUPPORT_FREEIMAGE
	registerLoaderFreeImage();
#endif

#ifdef SUPPORT_SDL_IMAGE
	registerLoaderSDL_image();
#endif

#ifdef SUPPORT_LIGHTSPRINT
	registerLoaderLightsprint();
#endif

#if defined(SUPPORT_FFMPEG) || defined(SUPPORT_LIBAV)
	// should be the last one, because it is probably very slow in rejecting unsupported formats
	registerLoaderFFmpegLibav();
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
	registerIsolationStep1(argc,argv);
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

void rr_io::isolateIO(int argc, char** argv)
{
#ifdef SUPPORT_ISOLATION
	// note that when we are called to do isolated conversion, step 2 converts scene and then exits program
	registerIsolationStep2(argc,argv);
#endif
}

