// --------------------------------------------------------------------------
// List of LightsprintIO supported file formats.
// Copyright 2006-2008 Lightsprint, Stepan Hrbek. All rights reserved.
// --------------------------------------------------------------------------


// What file formats you wish to be supported by LightsprintIO.
// Comment out formats you don't need.

#define SUPPORT_GAMEBRYO  // Gamebryo .gsa
#define SUPPORT_COLLADA   // Collada .dae
#define SUPPORT_QUAKE3    // Quake 3 .bsp
#define SUPPORT_3DS       // 3D Studio .3ds
#define SUPPORT_MGF       // Materials and Geometry Format .mgf
#define SUPPORT_OBJ       // Wavefront .obj
#define SUPPORT_IMAGES    // jpg, png, dds, hdr, exr, tga, tif, pcx, bmp, gif, ico etc



// Actual support depends on your operating system, compiler etc.

// FCollada doesn't support Visual Studio 2003.
#if defined(SUPPORT_COLLADA) && defined(_MSC_VER) && (_MSC_VER < 1400)
	#undef SUPPORT_COLLADA
#endif

// Gamebryo doesn't support Visual Studio 2003, 64-bit code, Linux.
// We don't support Gamebryo in static LightsprintIO (it works, but Gamebryo libs are not automatically linked to samples).
#if defined(SUPPORT_GAMEBRYO) && ( !defined(_MSC_VER) || _MSC_VER<1400 || defined(_M_X64) || defined(RR_IO_STATIC) )
	#undef SUPPORT_GAMEBRYO
#endif

// We haven't tested mgflib under Linux yet.
#if defined(SUPPORT_MGF) && !defined(_WIN32)
	#undef SUPPORT_MGF
#endif
