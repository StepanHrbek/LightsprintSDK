#ifndef ANIMATIONFRAME_H
#define ANIMATIONFRAME_H

#include <vector>
#include "Lightsprint/DemoEngine/Camera.h"
#include "Lightsprint/DemoEngine/Texture.h"
#include "Lightsprint/RRMath.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationFrame, one animation frame in editable form

struct AnimationFrame
{
	// camera and light
	de::Camera eyeLight[2];
	// dynamic objects
	typedef std::vector<rr::RRVec4> DynaPosRot;
	DynaPosRot dynaPosRot;
	// runtime generated
	de::Texture* thumbnail;

	AnimationFrame();

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AnimationFrame* blend(const AnimationFrame* that, float alpha) const;

	// load frame from opened .ani file
	bool load(FILE* f);

	// save frame to opened .ani file
	bool save(FILE* f) const;
};

#endif
