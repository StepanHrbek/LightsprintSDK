// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <algorithm> // sort
#include <cstdio>
#include <boost/unordered_map.hpp>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "MultiPass.h"
#include "RendererOfMesh.h"
#include "tmpstr.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMeshCache

class RendererOfMeshCache
{
public:
	RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh)
	{
		Cache::iterator i=cache.find(mesh);
		if (i!=cache.end())
		{
			return i->second;
		}
		else
		{
			return cache[mesh] = new RendererOfMesh();
		}
	}
	~RendererOfMeshCache()
	{
		for (Cache::const_iterator i=cache.begin();i!=cache.end();++i)
			delete i->second;
	}
private:
	typedef boost::unordered_map<const rr::RRMesh*,RendererOfMesh*> Cache;
	Cache cache;
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfOriginalScene

struct PerObjectBuffers
{
	float eyeDistance;
	rr::RRObject* object;
	RendererOfMesh* meshRenderer;
	rr::RRBuffer* diffuseEnvironment;
	rr::RRBuffer* specularEnvironment;
	rr::RRBuffer* lightIndirectBuffer;
	rr::RRBuffer* lightIndirectDetailMap;
	UberProgramSetup objectUberProgramSetup; // only for blended facegroups. everything is set except for MATERIAL_XXX, use enableUsedMaterials()
};

class RendererOfOriginalScene
{
public:
	RendererOfOriginalScene(const char* pathToShaders);
	~RendererOfOriginalScene();

	virtual void render(rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights, const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect, unsigned _lightIndirectLayer, int _lightDetailMapLayer,
		float* _clipPlanes,
		const rr::RRVec4* _brightness, float _gamma);

	RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh)
	{
		return rendererOfMeshCache.getRendererOfMesh(mesh);
	}

	TextureRenderer* getTextureRenderer()
	{
		return textureRenderer;
	}

private:
	// PERMANENT CONTENT
	TextureRenderer* textureRenderer;
	UberProgram* uberProgram;
	RendererOfMeshCache rendererOfMeshCache;

	// PERMANENT ALLOCATION, TEMPORARY CONTENT
	rr::RRObjects multiObjects;
	//! Gathered per-object information.
	rr::RRVector<PerObjectBuffers> perObjectBuffers;
	typedef boost::unordered_map<UberProgramSetup,rr::RRVector<FaceGroupRange>*> ShaderFaceGroups;
	//! Gathered non-blended object information.
	ShaderFaceGroups nonBlendedFaceGroupsMap;
	//! Gathered blended object information.
	rr::RRVector<FaceGroupRange> blendedFaceGroups;
};

static rr::RRVector<PerObjectBuffers>* s_perObjectBuffers = NULL; // used by sort()

bool FaceGroupRange::operator <(const FaceGroupRange& a) const // for sort()
{
	return (*s_perObjectBuffers)[object].eyeDistance>(*s_perObjectBuffers)[a.object].eyeDistance;
}

RendererOfOriginalScene::RendererOfOriginalScene(const char* pathToShaders)
{
	textureRenderer = new TextureRenderer(pathToShaders);
	uberProgram = UberProgram::create(
		tmpstr("%subershader.vs",pathToShaders),
		tmpstr("%subershader.fs",pathToShaders));
}

RendererOfOriginalScene::~RendererOfOriginalScene()
{
	delete uberProgram;
	delete textureRenderer;
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap.begin();i!=nonBlendedFaceGroupsMap.end();++i)
	{
		delete i->second;
	}
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : NULL;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : NULL;
}

void RendererOfOriginalScene::render(
		rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect,
		unsigned _lightIndirectLayer,
		int _lightDetailMapLayer,
		float* _clipPlanes,
		const rr::RRVec4* _brightness, float _gamma)
{
	if (!_solver)
	{
		RR_ASSERT(0);
		return;
	}

	// Get current camera for culling and distance-sorting.
	const Camera* eye = Camera::getRenderCamera();
	if (!eye)
	{
		RR_ASSERT(0); // eye not set
		return;
	}

	// Will we render multiobject or individual objects?
	//unsigned lightIndirectVersion = _solver?_solver->getSolutionVersion():0;
	bool needsIndividualStaticObjectsForEverything =
		// optimized render is faster and supports rendering into shadowmaps (this will go away with colored shadows)
		!_renderingFromThisLight

		// disabled, this forced multiobj even if user expected lightmaps from 1obj to be rendered
		// should be no loss, using multiobj in scenes with 1 object was faster only in old renderer, new renderer makes no difference
		//&& _solver->getStaticObjects().size()>1

		&& (
			// optimized render can't render LDM for more than 1 object
			((_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP || _uberProgramSetup.LIGHT_INDIRECT_auto) && _lightDetailMapLayer!=-1)
			// if we are to use provided indirect, take it always from 1objects
			// (if we are to update indirect, we update and render it in 1object or multiobject, whatever is faster. so both buffers must be allocated)
			|| ((_uberProgramSetup.LIGHT_INDIRECT_VCOLOR||_uberProgramSetup.LIGHT_INDIRECT_MAP||_uberProgramSetup.LIGHT_INDIRECT_auto) && !_updateLightIndirect && _lightIndirectLayer!=UINT_MAX)
			// optimized render looks bad with single specular cube per-scene
			|| (_uberProgramSetup.MATERIAL_SPECULAR && _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
		);

	// Will we render opaque parts from multiobject and blended parts from 1objects?
	// It's optimizations, makes render 10x faster in diacor (25k 1objects), compared to rendering everything from 1objects.
	bool needsIndividualStaticObjectsOnlyForBlending =
		!needsIndividualStaticObjectsForEverything
		&& !_renderingFromThisLight
		&& (
			// optimized render can't sort
			_uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND
		);

	rr::RRReportInterval report(rr::INF3,"Rendering %s%s scene...\n",needsIndividualStaticObjectsForEverything?"1obj":"multiobj",needsIndividualStaticObjectsOnlyForBlending?"+1objblend":"");

	// Preprocess scene, gather information about shader classes.
	perObjectBuffers.clear();
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap.begin();i!=nonBlendedFaceGroupsMap.end();++i)
		i->second->clear();
	blendedFaceGroups.clear();
	multiObjects.clear();
	multiObjects.push_back(_solver->getMultiObjectCustom());
	for (unsigned pass=0;pass<3;pass++)
	{
		const rr::RRObjects* objects;
		switch (pass)
		{
			case 0:
				if (needsIndividualStaticObjectsForEverything) continue;
				objects = &multiObjects;
				break;
			case 1:
				if (!needsIndividualStaticObjectsForEverything && !needsIndividualStaticObjectsOnlyForBlending) continue;
				objects = &_solver->getStaticObjects();
				break;
			case 2:
				if (_uberProgramSetup.FORCE_2D_POSITION) continue;
				objects = &_solver->getDynamicObjects();
				break;
		}
		for (unsigned i=0;i<objects->size();i++)
		{
			rr::RRObject* object = (*objects)[i];
			if (object)// && !eye->frustumCull(object))
			{
				const rr::RRMesh* mesh = object->getCollider()->getMesh();
				rr::RRObjectIllumination& illumination = object->illumination;

				PerObjectBuffers objectBuffers;
				objectBuffers.object = object;
				objectBuffers.meshRenderer = rendererOfMeshCache.getRendererOfMesh(mesh);
				objectBuffers.diffuseEnvironment = _uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE ? illumination.diffuseEnvMap : NULL;
				objectBuffers.specularEnvironment = _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR ? illumination.specularEnvMap : NULL;
				rr::RRBuffer* lightIndirectVcolor = (_uberProgramSetup.LIGHT_INDIRECT_auto||_uberProgramSetup.LIGHT_INDIRECT_VCOLOR) ? onlyVbuf(illumination.getLayer(_lightIndirectLayer)) : NULL;
				rr::RRBuffer* lightIndirectMap = (_uberProgramSetup.LIGHT_INDIRECT_auto||_uberProgramSetup.LIGHT_INDIRECT_MAP) ? onlyLmap(illumination.getLayer(_lightIndirectLayer)) : NULL;
				objectBuffers.lightIndirectBuffer = lightIndirectVcolor?lightIndirectVcolor:lightIndirectMap;
				objectBuffers.lightIndirectDetailMap = (_uberProgramSetup.LIGHT_INDIRECT_auto||_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP) ? onlyLmap(illumination.getLayer(_lightDetailMapLayer)) : NULL;

				objectBuffers.objectUberProgramSetup = _uberProgramSetup;
				if (pass==2 && objectBuffers.objectUberProgramSetup.SHADOW_MAPS>1)
					objectBuffers.objectUberProgramSetup.SHADOW_MAPS = 1; // decrease shadow quality on dynamic objects
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = objectBuffers.diffuseEnvironment!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = objectBuffers.specularEnvironment!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR = lightIndirectVcolor!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP = lightIndirectMap!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = objectBuffers.lightIndirectDetailMap!=NULL;
				objectBuffers.objectUberProgramSetup.OBJECT_SPACE = object->getWorldMatrix()!=NULL;

				const rr::RRObject::FaceGroups& faceGroups = object->faceGroups;
				bool blendedAlreadyFoundInObject = false;
				unsigned triangleInFgFirst = 0;
				unsigned triangleInFgLastPlus1 = 0;
				bool objectWillBeRendered = true; // solid 1obj won't be rendered if we mix solid from multiobj and blended from 1objs. then it does not need indirect updated
				for (unsigned g=0;g<faceGroups.size();g++)
				{
					triangleInFgLastPlus1 += faceGroups[g].numTriangles;
					const rr::RRMaterial* material = faceGroups[g].material;
					if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom))
					{
						if (material->needsBlending() && _uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
						{
							if (!needsIndividualStaticObjectsOnlyForBlending || pass!=0)
							{
								objectWillBeRendered = true;
								blendedFaceGroups.push_back(FaceGroupRange(perObjectBuffers.size(),g,triangleInFgFirst,triangleInFgLastPlus1));
								if (!blendedAlreadyFoundInObject)
								{
									blendedAlreadyFoundInObject = true;
									rr::RRVec3 center;
									mesh->getAABB(NULL,NULL,&center);
									const rr::RRMatrix3x4* worldMatrix = object->getWorldMatrix();
									if (worldMatrix)
										worldMatrix->transformPosition(center);
									objectBuffers.eyeDistance = (eye->pos-center).length();
								}
							}
						}
						else
						if (!needsIndividualStaticObjectsOnlyForBlending || pass!=1)
						{
							objectWillBeRendered = true;
							UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
							fgUberProgramSetup.enableUsedMaterials(material);
							fgUberProgramSetup.reduceMaterials(_uberProgramSetup);
							fgUberProgramSetup.validate();
							rr::RRVector<FaceGroupRange>*& nonBlended = nonBlendedFaceGroupsMap[fgUberProgramSetup];
							if (!nonBlended)
								nonBlended = new rr::RRVector<FaceGroupRange>;
							if (nonBlended->size() && (*nonBlended)[nonBlended->size()-1].object==perObjectBuffers.size() && (*nonBlended)[nonBlended->size()-1].faceGroupLast==g-1)
							{
								(*nonBlended)[nonBlended->size()-1].faceGroupLast++;
								(*nonBlended)[nonBlended->size()-1].triangleLastPlus1 = triangleInFgLastPlus1;
							}
							else
								nonBlended->push_back(FaceGroupRange(perObjectBuffers.size(),g,triangleInFgFirst,triangleInFgLastPlus1));
						}
					}
					triangleInFgFirst = triangleInFgLastPlus1;
				}
				perObjectBuffers.push_back(objectBuffers);

				if (_updateLightIndirect && objectWillBeRendered)
				{
					// update vertex buffers
					if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR
						// quit if buffer is already up to date
						&& lightIndirectVcolor->version!=_solver->getSolutionVersion())
					{
						if (pass==1)
						{
							// updates indexed 1object buffer
							_solver->updateLightmap(i,lightIndirectVcolor,NULL,NULL,NULL);
						}
						else
						if (pass==0)
						{
							// -1 = updates indexed multiobject buffer
							_solver->updateLightmap(-1,lightIndirectVcolor,NULL,NULL,NULL);
						}
					}
					// update cube maps
					// built-in version check
					if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE||objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
					{
						_solver->updateEnvironmentMap(&illumination);
					}
				}
			}
		}
	}

	PreserveCullFace p1;
	PreserveCullMode p2;
	PreserveBlend p3;

	// Render non-blended facegroups.
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap.begin();i!=nonBlendedFaceGroupsMap.end();++i)
	{
		rr::RRVector<FaceGroupRange>*& nonBlendedFaceGroups = i->second;
		if (nonBlendedFaceGroups && nonBlendedFaceGroups->size())
		{
			const UberProgramSetup& classUberProgramSetup = i->first;
			if (_uberProgramSetup.MATERIAL_CULLING && !classUberProgramSetup.MATERIAL_CULLING)
			{
				// we are rendering with culling, but it was disabled in this class because front=back
				// setup culling at the beginning
				glDisable(GL_CULL_FACE);
			}
			MultiPass multiPass(_lights,classUberProgramSetup,uberProgram,_brightness,_gamma,_clipPlanes);
			UberProgramSetup passUberProgramSetup;
			RealtimeLight* light;
			Program* program;
			while (program = multiPass.getNextPass(passUberProgramSetup,light))
			{
				for (unsigned j=0;j<nonBlendedFaceGroups->size();)
				{
					PerObjectBuffers& objectBuffers = perObjectBuffers[(*nonBlendedFaceGroups)[j].object];
					rr::RRObject* object = objectBuffers.object;

					// set transformation
					passUberProgramSetup.useWorldMatrix(program,object);

					// set envmaps
					passUberProgramSetup.useIlluminationEnvMaps(program,&object->illumination);

					// how many ranges can we render at once (the same mesh)?
					unsigned numRanges = 1;
					while (j+numRanges<nonBlendedFaceGroups->size() && (*nonBlendedFaceGroups)[j+numRanges].object==(*nonBlendedFaceGroups)[j].object) numRanges++;

					// render
					objectBuffers.meshRenderer->render(
						program,
						object,
						&(*nonBlendedFaceGroups)[j],numRanges,
						passUberProgramSetup,
						objectBuffers.lightIndirectBuffer,
						objectBuffers.lightIndirectDetailMap);

					j += numRanges;
				}
			}
		}
	}

	// Render skybox.
 	if (!_renderingFromThisLight && !_uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env0 = _solver->getEnvironment(0);
		if (textureRenderer && env0)
		{
			const rr::RRBuffer* env1 = _solver->getEnvironment(1);
			float blendFactor = _solver->getEnvironmentBlendFactor();
			Texture* texture0 = (env0->getWidth()>2)
				? getTexture(env0,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env0,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				;
			Texture* texture1 = env1 ? ( (env1->getWidth()>2)
				? getTexture(env1,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env1,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				) : NULL;
			textureRenderer->renderEnvironment(texture0,texture1,blendFactor,_brightness,_gamma,true);
		}
	}

	// Render water.
	//...

	// Render blended facegroups.
	if (blendedFaceGroups.size())
	{
		// Sort blended objects.
		s_perObjectBuffers = &perObjectBuffers;
		std::sort(&blendedFaceGroups[0],&blendedFaceGroups[0]+blendedFaceGroups.size());

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);

		// Render them by facegroup, it's usually small number.
		for (unsigned i=0;i<blendedFaceGroups.size();i++)
		{
			const PerObjectBuffers& objectBuffers = perObjectBuffers[blendedFaceGroups[i].object];
			rr::RRObject* object = objectBuffers.object;
			rr::RRMaterial* material = object->faceGroups[blendedFaceGroups[i].faceGroupFirst].material;
			UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
			fgUberProgramSetup.enableUsedMaterials(material);
			fgUberProgramSetup.reduceMaterials(_uberProgramSetup);
			fgUberProgramSetup.validate();
			MultiPass multiPass(_lights,fgUberProgramSetup,uberProgram,_brightness,_gamma,_clipPlanes);
			UberProgramSetup passUberProgramSetup;
			RealtimeLight* light;
			Program* program;
			while (program = multiPass.getNextPass(passUberProgramSetup,light))
			{
				// set transformation
				passUberProgramSetup.useWorldMatrix(program,object);

				// set envmaps
				passUberProgramSetup.useIlluminationEnvMaps(program,&object->illumination);

				// render
				objectBuffers.meshRenderer->render(
					program,
					object,
					&blendedFaceGroups[i],1,
					passUberProgramSetup,
					objectBuffers.lightIndirectBuffer,
					objectBuffers.lightIndirectDetailMap);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

RendererOfScene::RendererOfScene(const char* pathToShaders)
{
	renderer = new RendererOfOriginalScene(pathToShaders);
}

RendererOfScene::~RendererOfScene()
{
	delete renderer;
}

void RendererOfScene::render(rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights, const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect, unsigned _lightIndirectLayer, int _lightDetailMapLayer,
		float* _clipPlanes,
		const rr::RRVec4* _brightness, float _gamma)
{
	renderer->render(
		_solver,
		_uberProgramSetup,
		_lights,
		_renderingFromThisLight,
		_updateLightIndirect,
		_lightIndirectLayer,
		_lightDetailMapLayer,
		_clipPlanes,
		_brightness,
		_gamma);
}

RendererOfMesh* RendererOfScene::getRendererOfMesh(const rr::RRMesh* _mesh)
{
	return renderer->getRendererOfMesh(_mesh);
}

TextureRenderer* RendererOfScene::getTextureRenderer()
{
	return renderer->getTextureRenderer();
}

}; // namespace
