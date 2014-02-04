// --------------------------------------------------------------------------
// Modifies PluginScene parameters to show detected direct illumination.
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginShowDDI.h"
#include "Lightsprint/GL/PluginScene.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeShowDDI

class PluginRuntimeShowDDI : public PluginRuntime
{

public:

	PluginRuntimeShowDDI(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsShowDDI& pp = *dynamic_cast<const PluginParamsShowDDI*>(&_pp);

		// locate ppScene in chain
		PluginParamsScene* ppScene = NULL;
		const PluginParams* ppi = &_pp;
		while (ppi && !ppScene)
		{
			ppi = ppi->next;
			ppScene = const_cast<PluginParamsScene*>(dynamic_cast<const PluginParamsScene*>(ppi));
		}
		PluginParamsScene ppSceneBackup(NULL,NULL);

		// change ppScene to show DDI
		if (ppScene)
		{
			// backup
			ppSceneBackup = *ppScene;
			// allocate and fill vertex buffer with DDI illumination
			#define LAYER_DDI 1928374
			rr::RRObject* multiObject = ppScene->solver->getMultiObjectCustom();
			if (multiObject)
			{
				const unsigned* ddi = pp.solver->getDirectIllumination();
				if (ddi)
				{
					unsigned numTriangles = multiObject->getCollider()->getMesh()->getNumTriangles();
					rr::RRBuffer* vbuf = multiObject->illumination.getLayer(LAYER_DDI) = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,3*numTriangles,1,1,rr::BF_RGBA,true,NULL);
					unsigned* vbufData = (unsigned*)vbuf->lock(rr::BL_DISCARD_AND_WRITE);
					if (vbufData)
					{
						for (unsigned i=0;i<numTriangles;i++)
							vbufData[3*i+2] = vbufData[3*i+1] = vbufData[3*i] = ddi[i];
					}
					vbuf->unlock();
					// tell renderer to use our buffer and not to update it
					ppScene->updateLayers = false;
					ppScene->layerLightmap = LAYER_DDI;
					ppScene->layerEnvironment = UINT_MAX;
					ppScene->layerLDM = UINT_MAX;
					ppScene->forceObjectType = 2;
				}
			}
			// change rendermode (I think !indexed VBO doesn't even have uvs, so we must not use textures)
			rr_gl::UberProgramSetup uberProgramSetup;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.MATERIAL_CULLING = ppScene->uberProgramSetup.MATERIAL_CULLING;
			ppScene->uberProgramSetup = uberProgramSetup;
		}

		// render
		_renderer.render(_pp.next,_sp);

		// free vertex buffer with DDI illumination
		if (ppScene)
		{
			rr::RRObject* multiObject = ppScene->solver->getMultiObjectCustom();
			if (multiObject)
				RR_SAFE_DELETE(multiObject->illumination.getLayer(LAYER_DDI));
			// restore
			*ppScene = ppSceneBackup;
		}
	}

	virtual ~PluginRuntimeShowDDI()
	{
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsSSGI

PluginRuntime* PluginParamsShowDDI::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeShowDDI(pathToShaders, pathToMaps);
}

}; // namespace
