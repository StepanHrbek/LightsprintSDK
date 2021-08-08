// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// List of importers and exporters bundled in LightsprintIO.
// --------------------------------------------------------------------------


// What file formats you wish to be supported by LightsprintIO.
// Comment out formats you don't need.

#define SUPPORT_LIGHTSPRINT // Lightsprint .rr3, .rrbuffer, .rrmaterial
#define SUPPORT_OPENCOLLADA // OpenCollada .dae (better than Fcollada and Assimp)
//#define SUPPORT_FCOLLADA  // FCollada .dae (better than Assimp, but obsoleted by OpenCollada)
#define SUPPORT_3DS         // 3D Studio .3ds (sometimes better, sometimes worse than Assimp)
#define SUPPORT_ASSIMP      // Assimp dae 3ds x prj md2 md3 md5 ply mdl ase ask hmp smd vta mdc stl lwo lxo dxf nff enff raw off ac acc ac3d bvh xml irrmesh xml irr q3o q3s b3d ter csm 3d uc lws mot ndo pk3 blend
#define SUPPORT_QUAKE3      // Quake 3 .bsp
#define SUPPORT_MGF         // Materials and Geometry Format .mgf
#define SUPPORT_FREEIMAGE   // jpg, png, dds, hdr, exr, tga, tif, pcx, bmp, gif, ico etc
#define SUPPORT_LIBAV       // avi, wmv, mpg, mp3 etc
//#define SUPPORT_FFMPEG    // avi, wmv, mpg, mp3 etc
#define SUPPORT_DIRECTSHOW  // avi, wmv, mpg, c@pture etc (mostly obsoleted by FFmpeg/libav)
//#define SUPPORT_OBJ       // Wavefront .obj (obsoleted by Assimp)
#define SUPPORT_ISOLATION   // makes scene import run in isolated process
#define SUPPORT_SMALLLUXGPU // SmallLuxGpu .scn, .ply

// Actual support depends on your operating system, compiler etc.

// Assimp does not compile in Visual Studio 2010, 2012.
#if defined(SUPPORT_ASSIMP) && defined(_MSC_VER) && _MSC_VER<1800
	#undef SUPPORT_ASSIMP
#endif

// DirectShow exists only in Windows.
#if defined(SUPPORT_DIRECTSHOW) && !defined(_WIN32)
	#undef SUPPORT_DIRECTSHOW
#endif

// Isolation requires .rr3 support
#if defined(SUPPORT_ISOLATION) && !defined(SUPPORT_LIGHTSPRINT)
	#undef SUPPORT_ISOLATION
#endif

// For now, we include SmallLuxGpu only in Windows.
#if defined(SUPPORT_SMALLLUXGPU) && !defined(_WIN32)
	#undef SUPPORT_SMALLLUXGPU
#endif
