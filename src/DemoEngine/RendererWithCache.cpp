#include <cassert>
#include "DemoEngine/RendererWithCache.h"
#include <GL/glew.h>

bool  COMPILE=1;


RendererWithCache::RendererWithCache(Renderer* arenderer)
{
	renderer = arenderer;
}

RendererWithCache::~RendererWithCache()
{
	for(Map::iterator i=mapa.begin();i!=mapa.end();i++)
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
	if(length)
	{
		if(length>sizeof(key))
		{
			assert(0);
			length = sizeof(key); //!!! params delsi nez 16 jsou oriznuty
		}
		memcpy(&key,params,length);
	}
//	Map::iterator i = mapa.find(key);
//	if(i!=mapa.end())
//		return (Info&)i->second;
	return mapa[key];
}

void RendererWithCache::setStatus(ChannelStatus cs,RendererWithCache::Info& info)
{
	if(info.status==CS_COMPILED && cs!=CS_COMPILED)
	{
		assert(info.displayList!=UINT_MAX);
		glDeleteLists(info.displayList,1);
		info.displayList = UINT_MAX;
	}
	if(info.status!=CS_COMPILED && cs==CS_COMPILED)
	{
		assert(info.displayList==UINT_MAX);
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
		if(!COMPILE) goto never;
		assert(info.displayList==UINT_MAX);
		info.displayList = glGenLists(1);
		glNewList(info.displayList,GL_COMPILE);
		renderer->render();
		glEndList();
		info.status = CS_COMPILED;
		// intentionally no break
	case CS_COMPILED:
		assert(info.displayList!=UINT_MAX);
		glCallList(info.displayList);
		break;
	case CS_NEVER_COMPILE:
never:
		assert(info.displayList==UINT_MAX);
		renderer->render();
		break;
	default:
		assert(0);
	}
}
