// --------------------------------------------------------------------------
// DemoEngine
// RendererWithCache, Renderer implementation that accelerates other renderer using display list.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef RENDERERWITHCACHE_H
#define RENDERERWITHCACHE_H

#include "DemoEngine.h"
#include <cmath>
#include <map>
#include "Renderer.h"


//////////////////////////////////////////////////////////////////////////////
//
// RendererWithCache - filter that adds caching into underlying renderer

class RR_API RendererWithCache : public Renderer
{
public:
	RendererWithCache(Renderer* renderer);
	enum ChannelStatus
	{
		CS_READY_TO_COMPILE,
		CS_COMPILED,
		CS_NEVER_COMPILE,
	};
	void setStatus(ChannelStatus cs);
	virtual void render();
	virtual ~RendererWithCache();
private:
	Renderer* renderer;
	struct Key
	{
		unsigned char params[24];
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

#endif
