// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for .rr3, .rrbuffer and .rrmaterial formats.
// --------------------------------------------------------------------------

#ifndef RROBJECTLIGHTSPRINT_H
#define RROBJECTLIGHTSPRINT_H

//! Makes it possible to load and save following file types
//! - .rr3 scenes, using rr::RRScene::RRScene() and rr::RRScene::save()
//! - .rrbuffer 2d textures, cube textures and vertex buffers, using rr::RRBuffer::load() and rr::RRBuffer::save()
void registerLoaderLightsprint();

#endif
