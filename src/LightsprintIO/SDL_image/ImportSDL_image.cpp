// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// 2d image load using SDL_image.
// Use it when building Lightsprint SDK for web, as emscripten likes SDL.
// Outside web, our integration with FreeImage is much more complete.
// --------------------------------------------------------------------------

#include "../supported_formats.h"

#ifdef SUPPORT_SDL_IMAGE

#include <unordered_map>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "ImportSDL_image.h"
#include <SDL2/SDL_image.h>

using namespace rr;

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer load

static RRBuffer* loadSDL_image(const RRString& filename)
{
	SDL_Surface* surface = IMG_Load(filename.c_str());
	if (!surface)
	{
		RRReporter::report(WARN,"Image %s failed to load.\n", RR_RR2CHAR(filename));
		return nullptr;
	}
	const std::unordered_map<int, RRBufferFormat> map =
	{
		// this is very incomplete
		{SDL_PIXELFORMAT_RGB888, BF_RGB},
		{SDL_PIXELFORMAT_RGB24, BF_RGB},
		{SDL_PIXELFORMAT_BGR888, BF_BGR},
		{SDL_PIXELFORMAT_BGR24, BF_BGR},
		{SDL_PIXELFORMAT_RGBA8888, BF_RGBA},
		{SDL_PIXELFORMAT_RGBA32, BF_RGBA},
	};
	auto format = map.find(surface->format->format);
	if (format == map.end())
	{
		// unknown format
		RRReporter::report(WARN,"Image %s has unknown format %d.\n", RR_RR2CHAR(filename), surface->format->format);
		SDL_FreeSurface(surface);
		return nullptr;
	}
	RRBuffer* buffer = RRBuffer::create(BT_2D_TEXTURE, surface->w, surface->h, 1, format->second, true, (const unsigned char*)surface->pixels);
	if (buffer)
	{
		buffer->filename = filename; // [#36] exact filename we just opened or failed to open (we don't have a locator -> no attempts to open similar names)
	}
	SDL_FreeSurface(surface);
	return buffer;
}


/////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderSDL_image()
{
	if ((IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG) == (IMG_INIT_JPG|IMG_INIT_PNG)))
	{
		RRBuffer::registerLoader("*.jpg;*.png", loadSDL_image);
	}
	else
	{
		RRReporter::report(WARN,"Could not initialize sdl2_image: %s\n", IMG_GetError());
	}

}

#endif // SUPPORT_SDL_IMAGE
