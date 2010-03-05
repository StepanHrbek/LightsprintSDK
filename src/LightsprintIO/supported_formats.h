// --------------------------------------------------------------------------
// List of LightsprintIO supported file formats.
// Copyright (C) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


// What file formats you wish to be supported by LightsprintIO.
// Comment out formats you don't need.

#define SUPPORT_GAMEBRYO   // Gamebryo .gsa
#define SUPPORT_OPENCOLLADA// OpenCollada .dae (better than Fcollada)
#define SUPPORT_FCOLLADA   // FCollada .dae (better than Assimp)
#define SUPPORT_ASSIMP     // Assimp dae 3ds x prj md2 md3 md5 ply mdl ase ask hmp smd vta mdc stl lwo lxo dxf nff enff raw off ac acc ac3d bvh xml irrmesh xml irr q3o q3s b3d ter csm 3d uc lws mot
#define SUPPORT_QUAKE3     // Quake 3 .bsp
#define SUPPORT_MGF        // Materials and Geometry Format .mgf
#define SUPPORT_IMAGES     // jpg, png, dds, hdr, exr, tga, tif, pcx, bmp, gif, ico etc
#define SUPPORT_DIRECTSHOW // avi, wmv, mpg etc
#define SUPPORT_GOOGLEEARTH// Google Earth .kmz

// Old loaders obsoleted by Assimp.
#define SUPPORT_3DS       // 3D Studio .3ds
#define SUPPORT_OBJ       // Wavefront .obj



// Actual support depends on your operating system, compiler etc.

// FCollada doesn't support Visual Studio 2003.
#if defined(SUPPORT_FCOLLADA) && defined(_MSC_VER) && (_MSC_VER < 1400)
	#undef SUPPORT_FCOLLADA
#endif

// Gamebryo doesn't support Visual Studio 2003, 64-bit code, Linux.
// We don't support Gamebryo in static LightsprintIO (it works, but Gamebryo libs are not automatically linked to samples).
#if defined(SUPPORT_GAMEBRYO) && ( !defined(_MSC_VER) || _MSC_VER<1400 || defined(_M_X64) || defined(RR_IO_STATIC) )
	#undef SUPPORT_GAMEBRYO
#endif

// mgf should work everywhere, but we tested it only under Windows.
#if defined(SUPPORT_MGF) && !defined(_WIN32)
	#undef SUPPORT_MGF
#endif

// DirectShow exists only in Windows.
#if defined(SUPPORT_DIRECTSHOW) && !defined(_WIN32)
	#undef SUPPORT_DIRECTSHOW
#endif

#if defined SUPPORT_GOOGLEEARTH && ( !defined(_WIN32) || !defined(_MSC_VER) || (_MSC_VER<1500) )
	#undef SUPPORT_GOOGLEEARTH
#endif
