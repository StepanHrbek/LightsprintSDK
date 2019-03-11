// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Renders objects with illumination optionally updated by RRSolver.
// --------------------------------------------------------------------------

#define MIRRORS // enables implementation of mirrors, marks mirror source code
//#define SORT_MATERIALS // [#52] render simpler materials first. adds some (small) CPU overhead
//#define SRGB_CORRECT_BLENDING // transparency looks better without srgb correction

#include <algorithm> // sort
#include <map>
#include <cstdio>
#ifndef SORT_MATERIALS
	#include <unordered_map>
#endif
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PreserveState.h"
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "MultiPass.h"
#include "RendererOfMesh.h"
#include "Shader.h" // s_es
#include "Workaround.h"

#ifndef RR_GL_ES2
	#define OCCLUSION_QUERY // enables mirror optimization; usually helps, but makes CPU wait for GPU, kills all chances for speedup on dual GPU
#endif

namespace rr_gl
{



//////////////////////////////////////////////////////////////////////////////
//
// PerObjectBuffers

struct PerObjectBuffers
{
	float eyeDistance;
	rr::RRObject* object;
	RendererOfMesh* meshRenderer;
	rr::RRBuffer* reflectionEnvMap;
#ifdef MIRRORS
	rr::RRVec4 mirrorPlane;
	rr::RRBuffer* mirrorColorMap;
#endif
	rr::RRBuffer* lightIndirectBuffer;
	rr::RRBuffer* lightIndirectDetailMap;
	UberProgramSetup objectUberProgramSetup; // only for blended facegroups. everything is set except for MATERIAL_XXX, use enableUsedMaterials()
};


//////////////////////////////////////////////////////////////////////////////
//
// misc helpers

#ifndef SORT_MATERIALS
// for unordered_map<UberProgramSetup,>
struct UberProgramSetupHasher
{
	std::size_t operator()(const UberProgramSetup& b) const
	{
		unsigned* p = (unsigned*)&b;
		return p[0]+p[1];
	}
};
#endif

static rr::RRVector<PerObjectBuffers>* s_perObjectBuffers = nullptr; // used by sort()

bool FaceGroupRange::operator <(const FaceGroupRange& a) const // for sort()
{
	return (*s_perObjectBuffers)[object].eyeDistance>(*s_perObjectBuffers)[a.object].eyeDistance;
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : nullptr;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : nullptr;
}
rr::RRBuffer* onlyCube(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE) ? buffer : nullptr;
}

extern float getMipLevel(const rr::RRMaterial* material);

#ifdef MIRRORS
// here we map planes to mirrors, so that two objects with the same plane share mirror
struct PlaneCompare // comparing RRVec4 looks strange, so we do it here rather than in public RRVec4 operator<
{
	bool operator()(const rr::RRVec4& a, const rr::RRVec4& b) const
	{
		return a.x<b.x || (a.x==b.x && (a.y<b.y || (a.y==b.y && (a.z<b.z || (a.z==b.z && a.w<b.w)))));
	}
};
#endif // MIRRORS


//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeScene

//! OpenGL renderer of scene in RRSolver.
//
//! Renders scene from solver.
//! Takes live illumination from solver or computed illumination from layer.
class PluginRuntimeScene : public PluginRuntime
{
	// PERMANENT CONTENT
	UberProgram* uberProgram;
#ifdef MIRRORS
	rr::RRBuffer* mirrorDepthMap;
	rr::RRBuffer* mirrorMaskMap;
#endif

	// PERMANENT ALLOCATION, TEMPORARY CONTENT (used only inside render(), placing it here saves alloc/free in every render(), but makes render() nonreentrant)
	enum { MAX_RECURSION_DEPTH = 4 }; // 1 for normal render, 1 for mirror, 1 for update envmaps, 1 for selection (called at skybox time)
	rr::RRObjects multiObjects; // used in first half of render()
	//! Gathered per-object information.
	rr::RRVector<PerObjectBuffers> perObjectBuffers[MAX_RECURSION_DEPTH]; // used in whole render(), indexed by [recursionDepth]
#ifdef SORT_MATERIALS
	typedef std::map<UberProgramSetup,rr::RRVector<FaceGroupRange>*> ShaderFaceGroups; // sorts by UberShaderSetup::operator<
#else
	typedef std::unordered_map<UberProgramSetup,rr::RRVector<FaceGroupRange>*,UberProgramSetupHasher> ShaderFaceGroups;
#endif
	//! Gathered non-blended object information.
	ShaderFaceGroups nonBlendedFaceGroupsMap[MAX_RECURSION_DEPTH]; // used in whole render(), indexed by [recursionDepth]
	//! Gathered blended object information.
	rr::RRVector<FaceGroupRange> blendedFaceGroups[MAX_RECURSION_DEPTH]; // used in whole render(), indexed by [recursionDepth]
	//! usually 0, can grow to 1 or even 2 when render() calls render() because of mirror or updateEnvironmentMap()
	int recursionDepth;

	NamedCounter countScene;
	NamedCounter countSceneMirror;
	NamedCounter countSceneMirrorPlane;
	NamedCounter countSceneMirrorPlaneVisible;
	NamedCounter countSceneUpdateVbuf;
	NamedCounter countSceneUpdatedVbuf;
	NamedCounter countSceneUpdateCube;
	NamedCounter countSceneUpdatedCube;
	NamedCounter countSceneRenderMeshOpaque;
	NamedCounter countSceneRenderMeshBlended;
public:

	PluginRuntimeScene(const PluginCreateRuntimeParams& params)
	{
		reentrancy = MAX_RECURSION_DEPTH;
		uberProgram = UberProgram::create(rr::RRString(0,L"%lsubershader.vs",params.pathToShaders.w_str()),rr::RRString(0,L"%lsubershader.fs",params.pathToShaders.w_str()));

#ifdef MIRRORS
		mirrorDepthMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
		mirrorMaskMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGB,true,RR_GHOST_BUFFER);
#endif
		recursionDepth = -1;
		params.counters =
			countScene.init("scene",
			countSceneMirror.init("scene.mirror",
			countSceneMirrorPlane.init("scene.mirror.plane",
			countSceneMirrorPlaneVisible.init("scene.mirror.plane.visible",
			countSceneUpdateVbuf.init("scene.update.vbuf",
			countSceneUpdatedVbuf.init("scene.updated.vbuf",
			countSceneUpdateCube.init("scene.update.cube",
			countSceneUpdatedCube.init("scene.updated.cube",
			countSceneRenderMeshOpaque.init("scene.rendermesh.opaque",
			countSceneRenderMeshBlended.init("scene.rendermesh.blended",
			nullptr))))))))));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		countScene.count++;

		const PluginParamsScene& pp = *dynamic_cast<const PluginParamsScene*>(&_pp);
		if (!(pp.solver || pp.objects) || !_sp.camera)
		{
			RR_ASSERT(0);
			return;
		}

		// this should already be enforced by reentrancy=...
		RR_ASSERT(recursionDepth+1<MAX_RECURSION_DEPTH);
		if (recursionDepth+1>=MAX_RECURSION_DEPTH)
			return;

		recursionDepth++; // no "return" or throw from this function allowed, we must reach recursionDepth-- at the end

		// copy, so that we can modify some parameters
		PluginParamsScene _ = pp;
		PluginParamsShared sp = _sp;

		// Ensure sRGB correctness.
		if (!Workaround::supportsSRGB())
			sp.srgbCorrect = false;
#ifndef RR_GL_ES2
		PreserveFlag p0(GL_FRAMEBUFFER_SRGB,sp.srgbCorrect);
#endif

#ifdef MIRRORS
		GLint hasAlphaBits = -1; // -1=unknown, 0=no, 1,2,4,8...=yes
		// here we map planes to mirrors, so that two objects with the same plane share mirror
		typedef std::map<rr::RRVec4,rr::RRBuffer*,PlaneCompare> Mirrors;
		Mirrors mirrors;
#endif

		// solvers work with multipliers in linear space, convert them to srgb for rendering
		rr::RRSolver::Multipliers multipliers = pp.multipliers;
		const rr::RRColorSpace* colorSpace = pp.solver ? pp.solver->getColorSpace() : nullptr; // selection plugin call scene with solver=nullptr
		if (colorSpace)
		{
			colorSpace->fromLinear(multipliers.lightMultiplier);
			colorSpace->fromLinear(multipliers.environmentMultiplier);
			colorSpace->fromLinear(multipliers.materialEmittanceMultiplier);
		}

		// Will we render multiobject or individual objects?
		//unsigned lightIndirectVersion = _solver?_solver->getSolutionVersion():0;
		bool needsIndividualStaticObjectsForEverything =
			// optimized render is faster and supports rendering into shadowmaps (this will go away with colored shadows)
			!_.renderingFromThisLight

			// disabled, this forced multiobj even if user expected lightmaps from 1obj to be rendered
			// should be no loss, using multiobj in scenes with 1 object was faster only in old renderer, new renderer makes no difference
			//&& _solver->getStaticObjects().size()>1

			&& (
				// optimized render can't render LDM for more than 1 object
				(_.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _.layerLDM!=UINT_MAX)
				// old: if we are to render provided indirect, take it always from 1objects
				//      ([#12] if we are to update indirect, we update and render it in 1object or multiobject, whatever is faster. so both buffers must be allocated)
				// new (with !_.updateLayerLightmap commented out):
				//      if we are to render indirect, we update it to / take it always from 1objects
				//      indirect in multiobject was broken at least since rev 5000.
				//      it was rarely used (only in Arch solver with no mirrors, no cubemaps, no ldms), so it was not noticed sooner
				|| ((_.uberProgramSetup.LIGHT_INDIRECT_VCOLOR||_.uberProgramSetup.LIGHT_INDIRECT_MAP) && /*!_.updateLayerLightmap &&*/ _.layerLightmap!=UINT_MAX)
				// optimized render would look bad with single cube per-scene (sometimes such cube does not exist at all)
				|| ((_.uberProgramSetup.MATERIAL_SPECULAR && _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) && _.layerEnvironment!=UINT_MAX)
				|| ((_.uberProgramSetup.MATERIAL_DIFFUSE && _.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE) && _.layerEnvironment!=UINT_MAX)
#ifdef MIRRORS
				// optimized render would look bad without mirrors in static parts
				|| (_.uberProgramSetup.MATERIAL_DIFFUSE && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE)
				|| (_.uberProgramSetup.MATERIAL_SPECULAR && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR)
#endif
			);

		if (_.forceObjectType==1) needsIndividualStaticObjectsForEverything = true;
		if (_.forceObjectType==2) needsIndividualStaticObjectsForEverything = false;


		// Will we render opaque parts from multiobject and blended parts from 1objects?
		// It's optimizations, makes render 10x faster in diacor (25k 1objects), compared to rendering everything from 1objects.
		bool needsIndividualStaticObjectsOnlyForBlending =
			!needsIndividualStaticObjectsForEverything
			&& !_.renderingFromThisLight
			&& (
				// optimized render can't sort
				_.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && !_.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT
			);

		rr::RRReportInterval report(rr::INF3,"Rendering %s%s scene...\n",needsIndividualStaticObjectsForEverything?"1obj":"multiobj",needsIndividualStaticObjectsOnlyForBlending?"+1objblend":"");

		// Preprocess scene, gather information about shader classes. Update cubemaps. Gather mirrors that need update.
		perObjectBuffers[recursionDepth].clear();
		for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
			i->second->clear(); // saves lots of new/delete, but makes cycling over map slower, empty fields make cycle longer. needs benchmarking to make sure it's faster than plain nonBlendedFaceGroupsMap[recursionDepth].clear();
		blendedFaceGroups[recursionDepth].clear();
		multiObjects.clear();
		if (_.solver)
			multiObjects.push_back(_.solver->getMultiObject());
		for (int pass=-1;pass<3;pass++)
		{
			const rr::RRObjects* objects;
			switch (pass)
			{
				case -1:
					if (!_.objects) continue;
					objects = _.objects;
					break;
				case 0:
					if (!_.solver) continue;
					if (needsIndividualStaticObjectsForEverything) continue;
					objects = &multiObjects;
					break;
				case 1:
					if (!_.solver) continue;
					if (!needsIndividualStaticObjectsForEverything && !needsIndividualStaticObjectsOnlyForBlending) continue;
					objects = &_.solver->getStaticObjects();
					break;
				case 2:
					if (!_.solver) continue;
					if (_.uberProgramSetup.FORCE_2D_POSITION) continue;
					objects = &_.solver->getDynamicObjects();
					break;
			}
			for (unsigned i=0;i<objects->size();i++)
			{
				rr::RRObject* object = (*objects)[i];
				if (object && object->enabled)// && !_camera->frustumCull(object))
				{
					const rr::RRMesh* mesh = object->getCollider()->getMesh();
					rr::RRObjectIllumination& illumination = object->illumination;

					PerObjectBuffers objectBuffers;
					objectBuffers.object = object;
					objectBuffers.meshRenderer = _renderer.getMeshRenderer(mesh);
					rr::RRBuffer* lightIndirectVcolor = _.uberProgramSetup.LIGHT_INDIRECT_VCOLOR ? onlyVbuf(illumination.getLayer(_.layerLightmap)) : nullptr;
					rr::RRBuffer* lightIndirectMap = _.uberProgramSetup.LIGHT_INDIRECT_MAP ? onlyLmap(illumination.getLayer(_.layerLightmap)) : nullptr;
					objectBuffers.lightIndirectBuffer = lightIndirectVcolor?lightIndirectVcolor:lightIndirectMap;
					objectBuffers.lightIndirectDetailMap = _.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? onlyLmap(illumination.getLayer(_.layerLDM)) : nullptr;
					objectBuffers.reflectionEnvMap = (_.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) ? onlyCube(illumination.getLayer(_.layerEnvironment)) : nullptr;
#ifdef MIRRORS
					objectBuffers.mirrorColorMap = nullptr;
					objectBuffers.mirrorPlane = rr::RRVec4(0);
					if ((_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE || _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR) && !onlyCube(illumination.getLayer(_.layerEnvironment)))
					{
						rr::RRVec3 mini,maxi;
						mesh->getAABB(&mini,&maxi,nullptr);
						rr::RRVec3 size = maxi-mini;

						// is it planar?
						if (!size.mini() && size.sum()>size.maxi())
						{
							rr::RRVec4 plane = size.x ? ( size.y ? rr::RRVec4(0,0,1,-mini.z) : rr::RRVec4(0,1,0,-mini.y) ) : rr::RRVec4(1,0,0,-mini.x);
							rr::RRVec4 mirrorPlane = object->getWorldMatrixRef().getTransformedPlane(plane);
							{
								// is it reflective?
								const rr::RRObject::FaceGroups& faceGroups = object->faceGroups;
								for (unsigned g=0;g<faceGroups.size();g++)
								{
									const rr::RRMaterial* material = faceGroups[g].material;
									if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom)
										&& ((_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR && material->specularReflectance.color.sum()>0)
											// allocate mirror only because of diffuse reflection?
											// yes, but only if...
											|| (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE && material->diffuseReflectance.color.sum()>0
												// ...we don't have better data,
												&& !objectBuffers.lightIndirectBuffer
												// ...and plane is not small
												&& size.maxi()>=100
											)))
									{
										// does current framebuffer have A?
										if (hasAlphaBits==-1)
										{
											glGetIntegerv(GL_ALPHA_BITS,&hasAlphaBits);
											if (hasAlphaBits<0)
												hasAlphaBits = 0;
											if (!hasAlphaBits)
											{
												RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Mirror reflections disabled, current framebuffer lacks alpha channel.\n"));
											}
										}
										if (hasAlphaBits)
										{
											countSceneMirror.count++;

											// find or allocate 1x1 mirror
											objectBuffers.mirrorPlane = mirrorPlane;
											Mirrors::const_iterator i = mirrors.find(mirrorPlane);
											if (i!=mirrors.end())
												objectBuffers.mirrorColorMap = i->second;
											else
											{
												objectBuffers.mirrorColorMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
												mirrors.insert(std::pair<rr::RRVec4,rr::RRBuffer*>(objectBuffers.mirrorPlane,objectBuffers.mirrorColorMap));
											}

											// calculate optimal resolution for current specularModel+shininess
											unsigned mirrorW;
											unsigned mirrorH;
											if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
											{
												// mipmapped mirror should be power of two
												//    GPU generated npot mipmaps are poor, unstable (pixels slide as resolution changes),
												//    large areas of npot LIGHT_INDIRECT_MIRROR_DIFFUSE could reflect 0/0=undefined,
												//    adding texture2D(lightIndirectMirrorMap, mirrorCenterSmooth)*0.01 inside dividedByAlpha() avoids undefined, but creates perfect mirror, look wrong too
												//    +8.5 increases resolution, protects against shimmering
												float factor = RR_CLAMPED(powf(2.f,getMipLevel(material)+8.5f)/(_sp.viewport[2]+_sp.viewport[3]),0.1f,1);
												mirrorW = 1; while (mirrorW*2<=_sp.viewport[2]*factor) mirrorW *= 2;
												mirrorH = 1; while (mirrorH*2<=_sp.viewport[3]*factor) mirrorH *= 2;
											}
											else
											{
												// non-mipmapped mirror is better npot
												//    bluriness changes smoothly, not in big jumps
												float factor = RR_CLAMPED(powf(2.f,getMipLevel(material)+6.f)/(_sp.viewport[2]+_sp.viewport[3]),0.1f,1); // 0.001 instead of 0.1 would allow lowres mirror for rough surfaces. would look more rough, but suffers from terrible shimmering. it is tax for speed, user can switch to higher quality mipmapped mirrors
												mirrorW = (unsigned)(_sp.viewport[2]*factor);
												mirrorH = (unsigned)(_sp.viewport[3]*factor);
											}

											// adjust mirror resolution to be max of all optimal resolutions
											// (we might get here multiple times, with different mirrorW/H because of different shininess)
											mirrorW = RR_MAX(objectBuffers.mirrorColorMap->getWidth(),mirrorW);
											mirrorH = RR_MAX(objectBuffers.mirrorColorMap->getHeight(),mirrorH);
											//rr::RRReporter::report(rr::INF1,"%d*%d\n",mirrorW,mirrorH);
											objectBuffers.mirrorColorMap->reset(rr::BT_2D_TEXTURE,mirrorW,mirrorH,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);

											// don't break, cycle through all facegroups in plane, they might have higher shininess and increase resolution
										}
									}
								}
							}
						}
					}
#endif

					objectBuffers.objectUberProgramSetup = _.uberProgramSetup;
					if (pass==2 && objectBuffers.objectUberProgramSetup.SHADOW_MAPS>1)
						objectBuffers.objectUberProgramSetup.SHADOW_MAPS = 1; // decrease shadow quality on dynamic objects
					if (pp.lightmapsContainAlsoDirectIllumination && lightIndirectMap)
					{
						// when GI lightmap is found, disable direct lighting&shadowing
						objectBuffers.objectUberProgramSetup.SHADOW_MAPS = 0;
						objectBuffers.objectUberProgramSetup.LIGHT_DIRECT = 0;
					}
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = _.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && objectBuffers.reflectionEnvMap;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR && objectBuffers.reflectionEnvMap;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT = _.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT && objectBuffers.reflectionEnvMap;
#ifdef MIRRORS
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = objectBuffers.mirrorColorMap && !objectBuffers.lightIndirectBuffer && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = objectBuffers.mirrorColorMap!=nullptr;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = objectBuffers.mirrorColorMap && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS;
#else
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = false;
#endif
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_CONST = _.uberProgramSetup.LIGHT_INDIRECT_CONST && !lightIndirectVcolor && !lightIndirectMap && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE; // keep const only if no other indirect diffuse is available
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR = lightIndirectVcolor!=nullptr;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = false;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = lightIndirectVcolor!=nullptr && !lightIndirectVcolor->getScaled();
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP = lightIndirectMap!=nullptr;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP2 = false;
					objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = objectBuffers.lightIndirectDetailMap!=nullptr;
					objectBuffers.objectUberProgramSetup.OBJECT_SPACE = object->getWorldMatrix()!=nullptr;
					// autoset POSTPROCESS_ if not set (for top fps), keep it on if set (for fewer shaders)
					if (sp.brightness!=rr::RRVec4(1))
						objectBuffers.objectUberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
					if (sp.srgbCorrect || sp.gamma!=1) // srgbCorrect means that gamma is not final yet, we are going to adjust it
						objectBuffers.objectUberProgramSetup.POSTPROCESS_GAMMA = true;

					const rr::RRObject::FaceGroups& faceGroups = object->faceGroups;
					bool blendedAlreadyFoundInObject = false;
					unsigned triangleInFgFirst = 0;
					unsigned triangleInFgLastPlus1 = 0;
					bool objectWillBeRendered = false; // solid 1obj won't be rendered if we mix solid from multiobj and blended from 1objs. then it does not need indirect updated
					if (faceGroups.size()>65535) // FaceGroupRange contains 16bit shorts
						RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Object has %" RR_SIZE_T "d facegroups, that's sick (we render only first 65535).\n",faceGroups.size()));
					for (unsigned g=0;g<faceGroups.size();g++)
					{
						triangleInFgLastPlus1 += faceGroups[g].numTriangles;
						const rr::RRMaterial* material = faceGroups[g].material;
						if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom))
						{
							if (material->needsBlending() && _.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && !_.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT)
							{
								// here we process everything that needs sorting, i.e. what is blended in final render
								// blended objects rendered into rgb shadowmap are not processed here, because they don't need sort
								if (!needsIndividualStaticObjectsOnlyForBlending || pass!=0 || _.objects)
								{
									objectWillBeRendered = true;
									blendedFaceGroups[recursionDepth].push_back(FaceGroupRange(perObjectBuffers[recursionDepth].size(),g,triangleInFgFirst,triangleInFgLastPlus1));
									if (!blendedAlreadyFoundInObject)
									{
										blendedAlreadyFoundInObject = true;
										rr::RRVec3 center;
										mesh->getAABB(nullptr,nullptr,&center);
										const rr::RRMatrix3x4* worldMatrix = object->getWorldMatrix();
										if (worldMatrix)
											worldMatrix->transformPosition(center);
										objectBuffers.eyeDistance = (sp.camera->getPosition()-center).length();
									}
								}
							}
							else
							if (!needsIndividualStaticObjectsOnlyForBlending || pass!=1 || _.objects)
							{
								objectWillBeRendered = true;
								UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
								fgUberProgramSetup.enableUsedMaterials(material,dynamic_cast<const rr::RRMeshArrays*>(mesh));
								fgUberProgramSetup.reduceMaterials(_.uberProgramSetup);
								fgUberProgramSetup.validate();
								rr::RRVector<FaceGroupRange>*& nonBlended = nonBlendedFaceGroupsMap[recursionDepth][fgUberProgramSetup];
								if (!nonBlended)
									nonBlended = new rr::RRVector<FaceGroupRange>;
								if (nonBlended->size() && (*nonBlended)[nonBlended->size()-1].object==perObjectBuffers[recursionDepth].size() && (*nonBlended)[nonBlended->size()-1].faceGroupLast==g-1)
								{
									(*nonBlended)[nonBlended->size()-1].faceGroupLast++;
									(*nonBlended)[nonBlended->size()-1].triangleLastPlus1 = triangleInFgLastPlus1;
								}
								else
									nonBlended->push_back(FaceGroupRange(perObjectBuffers[recursionDepth].size(),g,triangleInFgFirst,triangleInFgLastPlus1));
							}
						}
						triangleInFgFirst = triangleInFgLastPlus1;
					}
					perObjectBuffers[recursionDepth].push_back(objectBuffers);

					if (_.updateLayerLightmap && objectWillBeRendered)
					{
						// update vertex buffers
						if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR
							// quit if buffer is already up to date
							&& lightIndirectVcolor->version!=_.solver->getSolutionVersion()
							// quit if rendering arbitrary non-solver objects
							&& !_.objects)
						{
							if (pass==1)
							{
								// updates indexed 1object buffer
								countSceneUpdateVbuf.count++;
								countSceneUpdatedVbuf.count += _.solver->updateLightmap(i,lightIndirectVcolor,nullptr,nullptr,nullptr);
							}
							else
							if (pass==0)
							{
								// -1 = updates indexed multiobject buffer
								countSceneUpdateVbuf.count++;
								countSceneUpdatedVbuf.count += _.solver->updateLightmap(-1,lightIndirectVcolor,nullptr,nullptr,nullptr);
							}
						}
					}
					if (_.updateLayerEnvironment && objectWillBeRendered)
					{
						// update cube maps
						// built-in version check
						if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE||objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
						{
							RR_ASSERT(recursionDepth+1<MAX_RECURSION_DEPTH);
							if (recursionDepth+1<MAX_RECURSION_DEPTH)
							{
								countSceneUpdateCube.count++;
								rr::RRBuffer* cube = illumination.getLayer(_.layerEnvironment);
								if (cube)
									countSceneUpdatedCube.count += (cube->getWidth()<32)
										? _.solver->RRSolver::updateEnvironmentMap(&illumination,_.layerEnvironment,_.uberProgramSetup.LIGHT_DIRECT?UINT_MAX:_.layerLightmap,_.uberProgramSetup.LIGHT_DIRECT?_.layerLightmap:UINT_MAX)
										: _.solver->          updateEnvironmentMap(&illumination,_.layerEnvironment,_.uberProgramSetup.LIGHT_DIRECT?UINT_MAX:_.layerLightmap,_.uberProgramSetup.LIGHT_DIRECT?_.layerLightmap:UINT_MAX);
							}
						}
					}
				}
			}
		}

		PreserveCullFace p1;  // changed by RendererOfMesh and us
		PreserveCullMode p2;  // changed by RendererOfMesh
		PreserveBlend p3;     // changed by RendererOfMesh (in MultiPass) and us
		PreserveColorMask p4; // changed by RendererOfMesh and us
		PreserveDepthMask p5; // changed by RendererOfMesh and us
		//DepthFunc changed by us
		//PolygonMode changed by us

#ifndef RR_GL_ES2
		if (pp.wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

		// Render non-sorted facegroups (update and apply mirrors).
		for (unsigned applyingMirrors=0;applyingMirrors<2;applyingMirrors++) // 2 passes, LIGHT_INDIRECT_MIRROR should go _after_ !LIGHT_INDIRECT_MIRROR to increase speed and reduce light leaking under walls
		{
			for (unsigned transparencyToRGB=0;transparencyToRGB<2;transparencyToRGB++) // 2 passes, MATERIAL_TRANSPARENCY_TO_RGB(e.g. window) must go _after_ !MATERIAL_TRANSPARENCY_TO_RGB(e.g. wall), otherwise in "light - wall - window" scene, window would paint to rgb shadowmap (wall does not write to rgb) and cast rgb shadow on wall
			for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
			if (i->first.MATERIAL_TRANSPARENCY_TO_RGB == (transparencyToRGB!=0))
			if ((i->first.LIGHT_INDIRECT_MIRROR_DIFFUSE||i->first.LIGHT_INDIRECT_MIRROR_SPECULAR) == (applyingMirrors!=0))
			{
				rr::RRVector<FaceGroupRange>*& nonBlendedFaceGroups = i->second;
				if (nonBlendedFaceGroups && nonBlendedFaceGroups->size())
				{
					// [#59] decide between 1sided and 2sided shadows in fast path (slow path is always 2sided)
					if (_.renderingFromThisLight && _.renderingFromThisLight->oneSidedShadows)
					{
						glEnable(GL_CULL_FACE);
						glCullFace(GL_BACK);
					}

					const UberProgramSetup& classUberProgramSetup = i->first;
					if (_.uberProgramSetup.MATERIAL_CULLING && !classUberProgramSetup.MATERIAL_CULLING)
					{
						// we are rendering with culling, but it was disabled in this class because front=back
						// setup culling at the beginning
						glDisable(GL_CULL_FACE);
					}
					MultiPass multiPass(*sp.camera,_.lights,_.renderingFromThisLight,classUberProgramSetup,uberProgram,multipliers.lightMultiplier,&_.clipPlanes,sp.srgbCorrect,&sp.brightness,sp.gamma*(sp.srgbCorrect?2.2f:1.f));
					UberProgramSetup passUberProgramSetup;
					RealtimeLight* light;
					Program* program;
					while ((program = multiPass.getNextPass(passUberProgramSetup,light)))
					{
						for (unsigned j=0;j<nonBlendedFaceGroups->size();)
						{
							PerObjectBuffers& objectBuffers = perObjectBuffers[recursionDepth][(*nonBlendedFaceGroups)[j].object];
							rr::RRObject* object = objectBuffers.object;

							// set transformation
							passUberProgramSetup.useWorldMatrix(program,object);

							// set envmaps
							passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_.layerEnvironment));
#ifdef MIRRORS
							// set mirror
							passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorColorMap);
#endif
							// how many ranges can we render at once (the same mesh)?
							unsigned numRanges = 1;
							while (j+numRanges<nonBlendedFaceGroups->size() && (*nonBlendedFaceGroups)[j+numRanges].object==(*nonBlendedFaceGroups)[j].object) numRanges++;

							// render
							countSceneRenderMeshOpaque.count++;
							objectBuffers.meshRenderer->renderMesh(
								program,
								object,
								&(*nonBlendedFaceGroups)[j],numRanges,
								passUberProgramSetup,
								_.renderingFromThisLight?true:false,
								objectBuffers.lightIndirectBuffer,
								objectBuffers.lightIndirectDetailMap,
								multipliers.materialEmittanceMultiplier,
								_.animationTime);

							j += numRanges;
#ifdef MIRRORS
							//if (objectBuffers.mirrorColorMap)
							//{
							//	glDisable(GL_BLEND);
							//	_renderer.getTextureRenderer()->render2D(getTexture(objectBuffers.mirrorColorMap,false,false),nullptr,0,0,0.5f,0.5f);
							//}
#endif
						}
					}
				}
			}
#ifdef MIRRORS
			// Update mirrors (after rendering all non-mirrors).
			if (!applyingMirrors && mirrors.size())
			{
				RR_ASSERT(recursionDepth+1<MAX_RECURSION_DEPTH);
				if (recursionDepth+1<MAX_RECURSION_DEPTH)
				{

				PluginParamsScene mirror = _;
				mirror.wireframe = false;
				mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false; // Don't use mirror in mirror, to prevent update in update (infinite recursion).
				mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
				mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = false;
				mirror.uberProgramSetup.CLIP_PLANE = true;
				FBO oldState = FBO::getState();
				for (Mirrors::iterator i=mirrors.begin();i!=mirrors.end();++i)
				{
					glDisable(GL_BLEND);
					glEnable(GL_DEPTH_TEST);

					// clear A
					glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
					glClear(GL_COLOR_BUFFER_BIT);

#ifdef OCCLUSION_QUERY
					// occlusion query optimization: phase 1
					if (pp.mirrorOcclusionQuery)
						glBeginQuery(GL_SAMPLES_PASSED,1);
#endif
					countSceneMirrorPlane.count++;

					// render shape of visible mirror pixels into A
					glDepthMask(GL_FALSE);
					UberProgramSetup mirrorMaskUberProgramSetup;
					mirrorMaskUberProgramSetup.MATERIAL_DIFFUSE = true;
					mirrorMaskUberProgramSetup.MATERIAL_CULLING = true;
					mirrorMaskUberProgramSetup.OBJECT_SPACE = true;
					Program* mirrorMaskProgram = mirrorMaskUberProgramSetup.getProgram(uberProgram);
					if (!mirrorMaskProgram)
					{
#ifdef OCCLUSION_QUERY
					skip_mirror:
#endif
						// mirror is completely occluded, don't render mirrorColorMap, delete it
						// mirror might still be rendered later in final render, but mirrorColorMap will be nullptr
						for (unsigned j=0;j<perObjectBuffers[0].size();j++)
							if (perObjectBuffers[0][j].mirrorColorMap==i->second)
								perObjectBuffers[0][j].mirrorColorMap = nullptr;
						RR_SAFE_DELETE(i->second);
						continue;
					}
					mirrorMaskProgram->useIt();
					mirrorMaskUberProgramSetup.useCamera(mirrorMaskProgram,sp.camera);
					for (unsigned j=0;j<perObjectBuffers[0].size();j++)
					{
						PerObjectBuffers& objectBuffers = perObjectBuffers[0][j];
						if (objectBuffers.mirrorPlane==i->first)
						{
							mirrorMaskUberProgramSetup.useWorldMatrix(mirrorMaskProgram,objectBuffers.object);
							FaceGroupRange fgrange(0, // does not have to be filled for RendererOfMesh
								0,objectBuffers.object->faceGroups.size()-1,0,objectBuffers.object->getCollider()->getMesh()->getNumTriangles());
							objectBuffers.meshRenderer->renderMesh(
								mirrorMaskProgram,
								objectBuffers.object,
								&fgrange,1,
								mirrorMaskUberProgramSetup,
								false,
								nullptr,
								nullptr,
								multipliers.materialEmittanceMultiplier,
								_.animationTime);
						}
					}

#ifdef OCCLUSION_QUERY
					// occlusion query optimization: phase 2
					if (pp.mirrorOcclusionQuery)
					{
						GLuint mirrorVisible = 0;
						glEndQuery(GL_SAMPLES_PASSED);
						glGetQueryObjectuiv(1,GL_QUERY_RESULT,&mirrorVisible);
						if (!mirrorVisible)
							goto skip_mirror;
					}
#endif
					countSceneMirrorPlaneVisible.count++;

					// copy A to mirrorMaskMap.A
					getTexture(mirrorMaskMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE)->bindTexture();
					glCopyTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3],0);

					// clear mirrorDepthMap=0, mirrorColorMap=0
					rr::RRBuffer* mirrorColorMap = i->second;
					mirrorDepthMap->reset(rr::BT_2D_TEXTURE,mirrorColorMap->getWidth(),mirrorColorMap->getHeight(),1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
					FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,getTexture(mirrorDepthMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE),oldState);
					Texture* mirrorColorTex = new Texture(mirrorColorMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE); // new Texture instead of getTexture makes our texture deletable at the end of render()
					FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,mirrorColorTex,oldState);
					PreserveFlag p0(GL_SCISSOR_TEST,false);
					glViewport(0,0,mirrorColorMap->getWidth(),mirrorColorMap->getHeight());
					glDepthMask(GL_TRUE);
					glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
					// there seems to be precision problem in GF220, 285.62
					// steps to reproduce:
					//   run "sceneviewer.exe koupelna4-spectest.rr3" in window with all SV panels closed
					// what's wrong:
					//   some regions of mirror reflection (rendered in render() 20 lines below) don't receive light from second pass
					//   in other words, depth test GL_LEQUAL sometimes fails on equal Z
					//   error disappears when at least one SV panel is open
					// workaround 1:
					//   glClearDepth(0.5) (or more, not less) instead of 0
					//   -risk of culling fragments very close to near
					// workaround 2:
					//   use "gl_FragDepth = step(0.51,tex.a);" directly on uncleared buffer. this code was removed in revision 5498 to get ES compatibility
					//   -incompatible with vanilla OpenGL ES 2.0
#ifdef RR_GL_ES2
					glClearDepthf(0);
#else
					if (s_es) glClearDepthf(0); else glClearDepth(0);
#endif
					glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
#ifdef RR_GL_ES2
					glClearDepthf(1);
#else
					if (s_es) glClearDepthf(1); else glClearDepth(1);
#endif

					// write mirrorDepthMap=1 for pixels with mirrorMaskMap>0.7
					// we clear to 0 and then overwrite some pixels to 1 (rather than writing both in one pass) because vanilla OpenGL ES 2.0 does not have gl_FragDepth
					glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
					glDepthFunc(GL_ALWAYS); // depth test must stay enabled, otherwise depth would not be written
					glDisable(GL_CULL_FACE);
					_renderer.getTextureRenderer()->render2D(getTexture(mirrorMaskMap),nullptr,1,0,-1,1,1,"#define MIRROR_MASK_DEPTH\n"); // keeps depth test enabled
					glDepthFunc(GL_LEQUAL);

					// render scene into mirrorDepthMap, mirrorColorMap.rgb
					glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
					rr::RRVec4 mirrorPlane = i->first;
					bool cameraInFrontOfMirror = mirrorPlane.planePointDistance(sp.camera->getPosition())>0;
					if (!cameraInFrontOfMirror) mirrorPlane = -mirrorPlane;
					mirrorPlane.w -= mirrorPlane.RRVec3::length()*sp.camera->getFar()*1e-5f; // add bias, clip face in clipping plane, avoid reflecting mirror in itself
					rr::RRCamera mirrorCamera = *sp.camera;
					mirrorCamera.mirror(mirrorPlane);
					mirrorCamera.setFar(mirrorCamera.getFar()*2); // should be far enough in majority of situations
					mirror.clipPlanes.clipPlane = mirrorPlane;
					PluginParamsShared mirrorShared;
					mirrorShared.camera = &mirrorCamera;
					mirrorShared.viewport[2] = mirrorColorMap->getWidth();
					mirrorShared.viewport[3] = mirrorColorMap->getHeight();
					mirrorShared.srgbCorrect = sp.srgbCorrect;
					_renderer.render(&mirror,mirrorShared);

					// copy mirrorMaskMap to mirrorColorMap.A
					//if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
					{
						glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
						_renderer.getTextureRenderer()->render2D(getTexture(mirrorMaskMap),nullptr,1,0,-1,1,-1,"#define MIRROR_MASK_ALPHA\n"); // disables depth test
					}

					oldState.restore();
					glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);

					// build mirror mipmaps
					// must go after oldState.restore();, HD2400+Catalyst 12.1 sometimes (in RealtimeLights, not in SceneViewer) crashes when generating mipmaps for render target
					// mipmaps generated on GF220 are ugly, but GL_NICEST does not help (the only thing that helps is making mirror power of two)
					//glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
					if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
					{
						glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
						mirrorColorTex->bindTexture();
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						glGenerateMipmap(GL_TEXTURE_2D); // exists in GL2+ARB_framebuffer_object, GL3, ES2
					}

					glDisable(GL_BLEND);
					// debug: render textures to backbuffer with z=-1 to pass z-test
					//        z-write is disabled, so everything rendered later (mirrors and glasses) overlaps them
					//_renderer.getTextureRenderer()->render2D(getTexture(mirrorMaskMap,false,false),nullptr,0.7f,0.7f,0.3f,0.3f,-1,"#define MIRROR_MASK_DEBUG\n"); // rendered up, MIRROR_MASK_DEBUG is necessary to show alpha as rgb
					//_renderer.getTextureRenderer()->render2D(getTexture(mirrorDepthMap,false,false),nullptr,1,0.35f,-0.3f,0.3f,-1);
					//_renderer.getTextureRenderer()->render2D(getTexture(mirrorColorMap,false,false),nullptr,1,0.0f,-0.3f,0.3f,-1); // rendered down
				}
				glDepthMask(GL_TRUE);
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

				}
			}
#endif // MIRRORS
		}

#ifndef RR_GL_ES2
		if (pp.wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		// Render skybox.
		_renderer.render(_pp.next,sp);

		// Render sorted facegroups.
		if (blendedFaceGroups[recursionDepth].size())
		{
#ifndef RR_GL_ES2
			if (pp.wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

#ifndef SRGB_CORRECT_BLENDING
			// We want to blend in custom scale, therefore we have to disable GL_FRAMEBUFFER_SRGB while blending
			// (otherwise framebuffer color is linearized, then blended, then converted back to sRGB, result would look more transparent).
			// Other option would be to keep GL_FRAMEBUFFER_SRGB and convert blending factor to physical scale in shader
			// (i.e. compensate for linearization and delinearization), but it would take more lines of code and results would be less accurate,
			//  because we can't accurately compensate unknown function hardcoded in HW.
			p0.change(false);
#endif
#endif

			// Sort blended objects.
			s_perObjectBuffers = &perObjectBuffers[recursionDepth];
			std::sort(&blendedFaceGroups[recursionDepth][0],&blendedFaceGroups[recursionDepth][0]+blendedFaceGroups[recursionDepth].size());

			// Render them by facegroup, it's usually small number.
			for (unsigned i=0;i<blendedFaceGroups[recursionDepth].size();i++)
			{
				const PerObjectBuffers& objectBuffers = perObjectBuffers[recursionDepth][blendedFaceGroups[recursionDepth][i].object];
				rr::RRObject* object = objectBuffers.object;
				rr::RRMaterial* material = object->faceGroups[blendedFaceGroups[recursionDepth][i].faceGroupFirst].material;
				UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
				fgUberProgramSetup.enableUsedMaterials(material,dynamic_cast<const rr::RRMeshArrays*>(object->getCollider()->getMesh()));
				fgUberProgramSetup.reduceMaterials(_.uberProgramSetup);
				fgUberProgramSetup.validate();
#ifdef SRGB_CORRECT_BLENDING
				MultiPass multiPass(*sp.camera,_.lights,_.renderingFromThisLight,fgUberProgramSetup,uberProgram,multipliers.lightMultiplier,&_.clipPlanes,sp.srgbCorrect,&sp.brightness,sp.gamma*(sp.srgbCorrect?2.2f:1.f));
#else
				MultiPass multiPass(*sp.camera,_.lights,_.renderingFromThisLight,fgUberProgramSetup,uberProgram,multipliers.lightMultiplier,&_.clipPlanes,false,&sp.brightness,sp.gamma);
#endif
				UberProgramSetup passUberProgramSetup;
				RealtimeLight* light;
				Program* program;
				while ((program = multiPass.getNextPass(passUberProgramSetup,light)))
				{
					// set transformation
					passUberProgramSetup.useWorldMatrix(program,object);

					// set envmaps
					passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_.layerEnvironment));
#ifdef MIRRORS
					// set mirror
					passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorColorMap);
#endif
					// render
					countSceneRenderMeshBlended.count++;
					objectBuffers.meshRenderer->renderMesh(
						program,
						object,
						&blendedFaceGroups[recursionDepth][i],1,
						passUberProgramSetup,
						_.renderingFromThisLight?true:false,
						objectBuffers.lightIndirectBuffer,
						objectBuffers.lightIndirectDetailMap,
						multipliers.materialEmittanceMultiplier,
						_.animationTime);
				}
			}

#ifndef RR_GL_ES2
			if (pp.wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
		}

#ifdef MIRRORS
		// Delete mirrors.
		for (Mirrors::const_iterator i=mirrors.begin();i!=mirrors.end();++i)
		{
			delete getTexture(i->second,false,false);
			delete i->second;
		}
#endif

		recursionDepth--;
	}

	virtual ~PluginRuntimeScene()
	{
#ifdef MIRRORS
		delete mirrorMaskMap;
		delete mirrorDepthMap;
#endif

		delete uberProgram;
		for (recursionDepth=0;recursionDepth<MAX_RECURSION_DEPTH;recursionDepth++)
			for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
				delete i->second;
	};
};


//////////////////////////////////////////////////////////////////////////////
//
// PluginParamsScene

PluginRuntime* PluginParamsScene::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeScene(params);
}

}; // namespace
