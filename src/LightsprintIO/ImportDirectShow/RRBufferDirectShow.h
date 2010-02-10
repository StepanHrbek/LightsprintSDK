// --------------------------------------------------------------------------
// Video load & play using DirectShow.
// Copyright (C) 2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRBUFFERDIRECTSHOW_H
#define RRBUFFERDIRECTSHOW_H

//! Makes it possible to play videos in texture.
//
//! Video is like static image
//! - opened via rr::RRBuffer::load()
//! - converted to texture via rr_gl::getTexture()
//! - pixels are available via rr::RRBuffer::lock(), getElement(), setElement()
//! - can be used in material, projected by light
//!
//! Video is not like static image
//! - video can't be reset() and reload()
void registerLoaderDirectShow();

#endif
