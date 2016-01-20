// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Video load & play using FFmpeg/libav.
// --------------------------------------------------------------------------

#ifndef RRBUFFERFFMPEG_H
#define RRBUFFERFFMPEG_H

//! Makes it possible to play videos in texture.
//
//! Video is like static image
//! - opened via rr::RRBuffer::load()
//! - converted to texture via rr_gl::getTexture()
//! - pixels are available via rr::RRBuffer::lock(), getElement()
//! - can be used in material, environment, projected by light
//!
//! Video is not like static image
//! - video can't be reset() and reload()
void registerLoaderFFmpegLibav();

#endif
