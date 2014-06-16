// --------------------------------------------------------------------------
// Renders to given cubemap.
// Copyright (C) 2010-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginCube.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeCube

class PluginRuntimeCube : public PluginRuntime
{
	NamedCounter countCube;
	NamedCounter countCubeResize;

public:

	PluginRuntimeCube(const PluginCreateRuntimeParams& params)
	{
		params.counters =
			countCube.init("cube",
			countCubeResize.init("cube.resize",
			NULL));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		countCube.count++;

		const PluginParamsCube& pp = *dynamic_cast<const PluginParamsCube*>(&_pp);
		
		if (!pp.cubeTexture || !pp.depthTexture)
			return;

		// we need depth texture here.
		// but why is it sent via pp.depthTexture parameter?
		// a) we can create and delete depthTexture in this function (possibly many times in each frame)
		//    but it could be slower
		// b) we can't use single depthTexture owned by us
		//    because PluginRuntimeCube (panorama) calls PluginRuntimeCube (update env maps) with different resolution, at least two depth maps are needed
		// c) YES caller passes pp.depthTexture in parameter
		//    this way two callers manage two depth textures and limited reentrancy works (panorama can update env maps,
		//    although, panorama can't do panorama)

		unsigned size = pp.cubeTexture->getBuffer()->getWidth();

		// resize depth
		if (pp.depthTexture->getBuffer()->getWidth()!=size)
		{
			countCubeResize.count++;
			pp.depthTexture->getBuffer()->reset(rr::BT_2D_TEXTURE,size,size,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
			pp.depthTexture->reset(false,false,false);
		}

		bool scissor = pp.fovDeg<=249; // scissor out parts of cube?

		PreserveFBO p0;
		PreserveViewport p1;
		PreserveFlag p2(GL_SCISSOR_TEST,scissor);
		glViewport(0,0,size,size);
		FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,pp.depthTexture,p0.state);

		// render 6 times
		// TODO: optimize these six renders down to one, with geometry shader
		rr::RRVec3 s_viewAngles[6] = // 6x yawPitchRollRad
		{
			// if we capture empty scene with skybox, we get original skybox:
			rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
			rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
			rr::RRVec3(0,RR_PI/2,0), // BOTTOM
			rr::RRVec3(0,-RR_PI/2,0), // TOP
			rr::RRVec3(RR_PI,0,RR_PI), // BACK
			rr::RRVec3(0,0,RR_PI), // FRONT
			/* if we capture empty scene with skybox, we get sky rotated by 180 degrees:
			rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
			rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
			rr::RRVec3(0,RR_PI/2,RR_PI), // BOTTOM
			rr::RRVec3(0,-RR_PI/2,RR_PI), // TOP
			rr::RRVec3(0,0,RR_PI), // FRONT
			rr::RRVec3(RR_PI,0,RR_PI), // BACK
			// if we capture empty scene with skybox, we get sky rotated by 90 degrees:
			rr::RRVec3(0,0,RR_PI), // FRONT
			rr::RRVec3(RR_PI,0,RR_PI), // BACK
			rr::RRVec3(0,RR_PI/2,RR_PI*3/2), // BOTTOM
			rr::RRVec3(0,-RR_PI/2,RR_PI/2), // TOP
			rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
			rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
			*/
		};
		char s_scissor[6] =
		{
			1,
			4,
			8,
			2,
			15,
			0
		};
		rr::RRCamera cubeCamera = *_sp.camera;
		cubeCamera.setAspect(1);
		cubeCamera.setFieldOfViewVerticalDeg(90);
		cubeCamera.setOrthogonal(false);
		cubeCamera.setScreenCenter(rr::RRVec2(0,0));
		PluginParamsShared sp = _sp;
		sp.camera = &cubeCamera;
		sp.viewport[0] = 0;
		sp.viewport[1] = 0;
		sp.viewport[2] = size;
		sp.viewport[3] = size;
		for (unsigned side=0;side<6;side++)
		{
			// a) six fixed views (world x+, x-, etc)
			//cubeCamera.setYawPitchRollRad(s_viewAngles[side]);
			// b) six views relative to current camera
			cubeCamera.setYawPitchRollRad((rr::RRMatrix3x4(_sp.camera->getViewMatrix(),false) * rr::RRMatrix3x4::rotationByYawPitchRoll(s_viewAngles[side])).getYawPitchRoll());
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,pp.cubeTexture,p0.state);
			RR_ASSERT(FBO::isOk()); // if someone called getTexture(cubeBuffer), texture is compressed and setting it as render target has no effect. here we report problem, at least in debug version
			if (scissor)
				glDisable(GL_SCISSOR_TEST);
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			if (scissor)
				glEnable(GL_SCISSOR_TEST);
			char sc = s_scissor[side];
			if (scissor && ((sc==15 && pp.fovDeg<=249) || (sc && pp.fovDeg<=90)))
				continue;
			if (scissor)
			{
				int reduceby = (int)(size*(249-pp.fovDeg)/159);
				int x0 = (sc&1)?reduceby:0;
				int y0 = (sc&2)?reduceby:0;
				int x1 = size-((sc&4)?reduceby:0);
				int y1 = size-((sc&8)?reduceby:0);
				glScissor(x0,y0,x1-x0,y1-y0);
			}
			_renderer.render(_pp.next,sp);
		}
	}

	virtual ~PluginRuntimeCube()
	{
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsCube

PluginRuntime* PluginParamsCube::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeCube(params);
}

}; // namespace
