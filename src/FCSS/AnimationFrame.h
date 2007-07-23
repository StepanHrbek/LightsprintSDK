#ifndef ANIMATIONFRAME_H
#define ANIMATIONFRAME_H

#include <vector>
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/RRMath.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationFrame, one animation frame in editable form

struct AnimationFrame
{
	// camera and light
	rr_gl::Camera eye;
	rr_gl::Camera light;
	rr::RRVec4 brightness;
	rr::RRReal gamma;
	unsigned projectorIndex;
	// dynamic objects
	struct DynaObjectPosRot
	{
		rr::RRVec3 pos;
		rr::RRVec2 rot; // yz, first y is rotated, then z
		DynaObjectPosRot()
		{
			memset(this,0,sizeof(*this));
		}
	};
	typedef std::vector<DynaObjectPosRot> DynaPosRot;
	DynaPosRot dynaPosRot;
	// timing
	float transitionToNextTime;
	// precomputed layer
	unsigned layerNumber;
	// runtime generated
	rr_gl::Texture* thumbnail;

	// layerNumber should be unique for whole animation track
	AnimationFrame(unsigned layerNumber);
	~AnimationFrame();

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AnimationFrame* blend(const AnimationFrame& that, float alpha) const;

	// load frame from opened .ani file, return NULL on failure
	static AnimationFrame* load(FILE* f);

	// validate frame so it has correct number of object positions
	void validate(unsigned numObjects);

	// save frame to opened .ani file
	bool save(FILE* f) const;

private:
	// load frame from opened .ani file, return false on failure
	bool loadPrivate(FILE* f);
};

#endif
