// --------------------------------------------------------------------------
// Lightmaps sample
//
// This is a viewer of .3DS and Collada .DAE scenes
// with realtime global illumination
// and ability to precompute/render/save/load
// higher quality texture based illumination.
//
// Unlimited options:
// - Ambient maps (indirect illumination) are precomputed here,
//   but you can tweak parameters of updateLightmaps()
//   to create any type of textures, including direct/indirect/global illumination.
// - Use setEnvironment() and capture illumination from skybox.
// - Use setLights() and capture illumination from arbitrary 
//   point/spot/dir/area/programmable lights.
// - Tweak materials and capture illumination from emissive materials.
// - Everything is HDR internally, custom scale externally.
//   Tweak setScaler() and/or RRIlluminationPixelBuffer
//   to set your scale and get HDR textures.
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//  spacebar = toggle between vertex based and ambient map based render
//  p = Precompute higher quality maps
//      alt-tab to console to see progress (takes several minutes)
//  s = Save maps to disk (alt-tab to console to see filenames)
//  l = Load maps from disk, stop realtime global illumination
//
// Remarks:
// - Collada is enabled by uncommenting #define COLLADA
// - To increase ambient map quality, you can
//    - provide better unwrap texcoords for meshes
//      (see getTriangleMapping or save unwrap into Collada document)
//    - call updateLightmaps with higher quality
//    - increase ambient map resolution (see newPixelBuffer)
// - To generate maps faster
//    - provide better unwrap texcoords for meshes
//      (see getTriangleMapping or save unwrap into Collada document)
//      and decrease map resolution (see newPixelBuffer)
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#define COLLADA
// loads Collada .DAE scene instead of .3DS scene
// requires free FCollada library installed
// requires #if 1 at the beginning of RRObjectCollada.cpp

//#ifdef COLLADA
#include "FCollada.h" // must be included before demoengine because of fcollada SAFE_DELETE macro
#include "FCDocument/FCDocument.h"
#include "../../samples/ImportCollada/RRObjectCollada.h"
