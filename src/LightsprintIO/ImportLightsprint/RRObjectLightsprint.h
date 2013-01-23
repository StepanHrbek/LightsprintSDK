// --------------------------------------------------------------------------
// Lightsprint adapters for Lightsprint .rr3 and .rrbuffer formats.
// Copyright (C) 2010-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTLIGHTSPRINT_H
#define RROBJECTLIGHTSPRINT_H

//! Makes it possible to load and save following file types
//! - .rr3 scenes, using rr::RRScene::RRScene() and rr::RRScene::save()
//! - .rrbuffer 2d textures, cube textures and vertex buffers, using rr::RRBuffer::load() and rr::RRBuffer::save()
void registerLoaderLightsprint();

#endif
