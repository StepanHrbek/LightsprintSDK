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
	// must stay together, AnimationFrame::blend accesses it as array of floats
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
	rr::RRBuffer* thumbnail;

	// overlay
	char overlayFilename[200];
	float overlaySeconds;
	unsigned overlayMode;
	rr::RRBuffer* overlayMap;

	// volume
	float volume;

	// technique
	unsigned shadowType; // 0=hard 1=soft 2=penumbra
	unsigned indirectType; // 0=none 1=constant 2=realtimeGI 3=precomputedGI
	bool wantsConstantAmbient() const {return indirectType==1;}
	bool wantsVertexColors() const {return indirectType==2;}
	bool wantsLightmaps() const {return indirectType==3;}

	// layerNumber should be unique for whole animation track
	AnimationFrame(unsigned layerNumber);
	AnimationFrame(AnimationFrame& copy);
	~AnimationFrame();

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AnimationFrame* blend(const AnimationFrame& that, float alpha) const;

	// load frame from opened .ani file, return false on failure, true when at least 1 line of data is loaded
	// existing frame is modified
	bool loadOver(FILE* f);

	// load frame from opened .ani file, return NULL on failure
	// new frame is created
	static AnimationFrame* loadNew(FILE* f);

	// validate frame so it has correct number of object positions
	void validate(unsigned numObjects);

	// save frame to opened .ani file
	// saves only lines that differ from prev
	bool save(FILE* f, const AnimationFrame& prev) const;
};

#endif
