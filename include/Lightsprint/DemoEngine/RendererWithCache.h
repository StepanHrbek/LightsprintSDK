// --------------------------------------------------------------------------
// DemoEngine
// RendererWithCache, Renderer implementation that accelerates other renderer using display list.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RENDERERWITHCACHE_H
#define RENDERERWITHCACHE_H

#include <cmath>
#include <map>
#include "Renderer.h"

namespace de
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererWithCache - filter that adds caching into underlying renderer

//! Renderer that speeds up OpenGL based renderers.
//
//! Renderer implementation that stores actions of other renderer
//! into display lists and plays them from display list when possible.
//! This speeds up OpenGL based renderers that perform
//! only actions stored by OpenGL into display list,
//! see OpenGL for more details.
//! Not suitable for non-OpenGL renderers or OpenGL renderers that
//! execute operations not stored into display list.
class DE_API RendererWithCache : public Renderer
{
public:
	//! Creates cache able to store and replay OpenGL commands of renderer.
	RendererWithCache(Renderer* renderer);
	//! States of cache.
	enum ChannelStatus
	{
		CS_READY_TO_COMPILE, ///< Not compiled, but ready to compile.
		CS_COMPILED,         ///< Already compiled.
		CS_NEVER_COMPILE,    ///< Compilation is forbidden by user.
	};
	//! Sets status of cache.
	void setStatus(ChannelStatus cs);
	//! Renders, possibly using display list to speed up OpenGL calls.
	virtual void render();
	virtual ~RendererWithCache();
private:
	Renderer* renderer;
	struct Key
	{
		unsigned char params[60];
		bool operator <(const Key& key) const
		{
			return memcmp(params,key.params,sizeof(Key))<0;
		};
	};
	struct Info
	{
		Info()
		{
			status = CS_READY_TO_COMPILE;
			displayList = UINT_MAX;
		};
		ChannelStatus status;
		unsigned displayList;
	};
	typedef std::map<Key,Info> Map;
	Map mapa;
	Info& findInfo();
	void setStatus(ChannelStatus cs,Info& info);
};

}; // namespace

#endif
