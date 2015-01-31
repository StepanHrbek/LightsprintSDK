// --------------------------------------------------------------------------
// Plugin based rendered.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/Plugin.h"
#include "RendererOfMesh.h"
#include <typeindex>
#include <unordered_map>

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
	typedef std::unordered_map<const rr::RRMesh*,RendererOfMesh*> Cache;
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
		counters = NULL;
		textureRenderer = new TextureRenderer(pathToShaders);
	}

	virtual void render(const PluginParams* _pp, const PluginParamsShared& _sp)
	{
		if (_pp)
		{
			bool initialized = pluginRuntimes.find(std::type_index(typeid(*_pp))) != pluginRuntimes.end();
			PluginRuntime*& runtime = pluginRuntimes[std::type_index(typeid(*_pp))];
			if (!initialized)
			{
				NamedCounter** lastCounter = &counters;
				while (*lastCounter) lastCounter = &lastCounter[0]->next;
				PluginCreateRuntimeParams params(*lastCounter);
				params.pathToShaders = pathToShaders;
				params.pathToMaps = pathToMaps;
				runtime = _pp->createRuntime(params);
			}
			if (runtime)
			{
				if (!_sp.viewport[2] && !_sp.viewport[3])
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Did you forget to set PluginParamsShared::viewport? It is empty.\n"));
				if (runtime->rendering<runtime->reentrancy)
				{
					runtime->rendering++;
					runtime->render(*this,*_pp,_sp);
					runtime->rendering--;
				}
				else
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Plugin %s reentrancy limit %d exceeded.\n",typeid(*_pp).name(),runtime->reentrancy));
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
		for (std::unordered_map<std::type_index,PluginRuntime*>::iterator i = pluginRuntimes.begin(); i!=pluginRuntimes.end(); ++i)
			delete i->second;
	}

	virtual NamedCounter* getCounters()
	{
		return counters;
	}

private:
	// PERMANENT CONTENT
	rr::RRString pathToShaders;
	rr::RRString pathToMaps;
	NamedCounter* counters; // linked list of counters exposed by plugins. plugins add counters from ctor, increment from render(). whole list is statically allocated by plugins, no dynamic allocation
	TextureRenderer* textureRenderer;
	//UberProgram* uberProgram;
	RendererOfMeshCache rendererOfMeshCache;
	std::unordered_map<std::type_index,PluginRuntime*> pluginRuntimes;
};

//////////////////////////////////////////////////////////////////////////////
//
// Renderer

Renderer* Renderer::create(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
{
	return new RendererImpl(pathToShaders, pathToMaps);
}

}; // namespace
