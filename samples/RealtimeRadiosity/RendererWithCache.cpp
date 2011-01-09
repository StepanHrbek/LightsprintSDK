// --------------------------------------------------------------------------
// RendererWithCache, Renderer implementation that accelerates other renderer using display list.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/RRDebug.h"
#include "RendererWithCache.h"


//////////////////////////////////////////////////////////////////////////////
//
// RendererWithCache

bool  COMPILE = 1; // 0 would disable display lists


RendererWithCache::RendererWithCache(Renderer* arenderer)
{
	renderer = arenderer;
}

RendererWithCache::~RendererWithCache()
{
	for (Map::iterator i=mapa.begin();i!=mapa.end();i++)
	{
		setStatus(CS_NEVER_COMPILE,i->second);
	}
}

RendererWithCache::Info& RendererWithCache::findInfo()
{
	Key key;
	memset(&key,0,sizeof(key));
	unsigned length;
	const void* params = renderer->getParams(length);
	if (length)
	{
		if (length>sizeof(key))
		{
			RR_ASSERT(0);
			length = sizeof(key); //!!! params delsi nez 16 jsou oriznuty
		}
		memcpy(&key,params,length);
	}
	return mapa[key];
}

void RendererWithCache::setStatus(ChannelStatus cs,RendererWithCache::Info& info)
{
	if (info.status==CS_COMPILED && cs!=CS_COMPILED)
	{
		RR_ASSERT(info.displayList!=UINT_MAX);
		glDeleteLists(info.displayList,1);
		info.displayList = UINT_MAX;
	}
	if (info.status!=CS_COMPILED && cs==CS_COMPILED)
	{
		RR_ASSERT(info.displayList==UINT_MAX);
		cs = CS_READY_TO_COMPILE;
	}
	info.status = cs;
}

void RendererWithCache::setStatus(ChannelStatus cs)
{
	setStatus(cs,findInfo());
}

void RendererWithCache::render()
{
	RendererWithCache::Info& info = findInfo();
	switch(info.status)
	{
	case CS_READY_TO_COMPILE:
		if (!COMPILE) goto never;
		RR_ASSERT(info.displayList==UINT_MAX);
		info.displayList = glGenLists(1);
		glNewList(info.displayList,GL_COMPILE);
		renderer->render();
		glEndList();
		info.status = CS_COMPILED;
		// intentionally no break
	case CS_COMPILED:
		RR_ASSERT(info.displayList!=UINT_MAX);
		glCallList(info.displayList);
		break;
	case CS_NEVER_COMPILE:
never:
		RR_ASSERT(info.displayList==UINT_MAX);
		renderer->render();
		break;
	default:
		RR_ASSERT(0);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// Renderer

Renderer* Renderer::createDisplayList()
{
	if (!this) return NULL;
	return new RendererWithCache(this);
}
