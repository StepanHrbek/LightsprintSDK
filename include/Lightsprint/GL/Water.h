//----------------------------------------------------------------------------
//! \file Water.h
//! \brief LightsprintGL | water with planar reflection, fresnel, waves
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef WATER_H
#define WATER_H

#include "Program.h"
#include "Texture.h"
#include "Camera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Water

//! Water effect with waves, reflection, fresnel.
//
//! To render water
//! - call updateReflectionInit()
//! - render whole scene while culling pixels below water (when using uber shader, enable CLIP_PLANE. this removes everything below 0 in world space)
//! - call updateReflectionDone()
//! - render whole scene
//! - call render()
class RR_GL_API Water
{
public:
	//! \param withFresnel
	//!  True = fresnel term is used to calculate refraction intensity.
	//! \param withDirlight
	//!  Enables refraction from one directional light specified in render().
	Water(const char* pathToShaders, bool withFresnel, bool withDirlight);
	~Water();

	//! Prepares pipeline for rendering into reflection map.
	//! \param reflWidth
	//!  Width of reflection texture. Typically 1/2 or 1/4 of main viewport width.
	//! \param reflHeight
	//!  Height of reflection texture. Typically 1/2 or 1/4 of main viewport height.
	//! \param eye
	//!  Camera used for rendering scene. It will be modified
	//! \param altitude
	//!  Altitude of water surface in world. When using CLIP_PLANE in ubershader, this must be 0.
	void updateReflectionInit(unsigned reflWidth, unsigned reflHeight, Camera* eye, float altitude=0);

	//! Finishes rendering into reflection map, restores camera settings.
	void updateReflectionDone();

	//! Renders water quad with reflection.
	//
	//! If you don't want simple quad, call render(0) and issue your own
	//! water primitives, reflection shader is still active and will be used.
	//! \param size
	//!  Size of water quad in center of world (-size..size).
	//! \param center
	//!  Center of quad in world space (0=center of world). y is ignored, altitude from updateReflectionInit() is used instead.
	//! \param waterColor
	//!  Color of water.
	//!  If withFresnel=false, final color is computed in GLSL as mix(reflection,waterColor,waterColor.a).
	//! \param lightDirection
	//!  Direction of directional light. Does not have to be normalized.
	//!  Only used if withDirlight was enabled in constructor.
	//! \param lightColor
	//!  Color of directional light.
	//!  Only used if withDirlight was enabled in constructor.
	void render(float size, rr::RRVec3 center, rr::RRVec4 waterColor, rr::RRVec3 lightDirection, rr::RRVec3 lightColor);

protected:
	Texture* normalMap;
	Texture* mirrorMap;
	Texture* mirrorDepth;
	Program* mirrorProgram;
	Camera*  eye;
	float    altitude;
	GLint    viewport[4];
	bool     fresnel;
	bool     dirlight;
};

}; // namespace

#endif
