// --------------------------------------------------------------------------
// Plugin based rendered.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/Plugin.h"
#include "RendererOfMesh.h"
#include <typeindex>
#include <boost/unordered_map.hpp>

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
// RendererImpl

class RendererImpl : public Renderer
{
public:
	RendererImpl(const rr::RRString& _pathToShaders, const rr::RRString& _pathToMaps) : pathToShaders(_pathToShaders), pathToMaps(_pathToMaps)
	{
		textureRenderer = new TextureRenderer(pathToShaders);
	}

	virtual void render(const PluginParams* _pp, const PluginParamsShared& _sp)
	{
		if (_pp)
		{
			bool initialized = pluginRuntimes.find(std::type_index(typeid(*_pp))) != pluginRuntimes.end();
			PluginRuntime*& runtime = pluginRuntimes[std::type_index(typeid(*_pp))];
			if (!initialized)
				runtime = _pp->createRuntime(pathToShaders, pathToMaps);
			if (runtime)
			{
				runtime->render(*this,*_pp,_sp);
			}
			else
			{
				RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Plugin %s failed to initialize.\n",typeid(*_pp).name()));
				render(_pp->next,_sp);
			}
		}
	}

	virtual TextureRenderer* getTextureRenderer()
	{
		return textureRenderer;
	}

	virtual RendererOfMesh* getMeshRenderer(const rr::RRMesh* mesh)
	{
		return rendererOfMeshCache.getRendererOfMesh(mesh);
	}

	virtual ~RendererImpl()
	{
		for (boost::unordered_map<std::type_index,PluginRuntime*>::iterator i = pluginRuntimes.begin(); i!=pluginRuntimes.end(); ++i)
			delete i->second;
	}

private:
	// PERMANENT CONTENT
	rr::RRString pathToShaders;
	rr::RRString pathToMaps;
	TextureRenderer* textureRenderer;
	//UberProgram* uberProgram;
	RendererOfMeshCache rendererOfMeshCache;
	boost::unordered_map<std::type_index,PluginRuntime*> pluginRuntimes;
};

//////////////////////////////////////////////////////////////////////////////
//
// Renderer

Renderer* Renderer::create(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
{
	return new RendererImpl(pathToShaders, pathToMaps);
}

}; // namespace
