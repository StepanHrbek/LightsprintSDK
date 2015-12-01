//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRFileLocator.h
//! \brief LightsprintCore | helper for locating files
//----------------------------------------------------------------------------

#ifndef RRFILELOCATOR_H
#define RRFILELOCATOR_H

#include "RRMemory.h"

namespace rr
{

	/////////////////////////////////////////////////////////////////////////////
	//
	//! Provides possible file locations when exact location is not known.
	//
	//! For simple use cases, default implementation that ignores all setXxx() functions
	//! is sufficient. In case of need, use create() for advanced locator.
	//!
	//! Locator supports multiple extension replacements, path relocations, regex replacements etc,
	//! call setXxx(true,xxx1); setXxx(true,xxx2); to add two of them.
	//! Later you can call e.g. setXxx(false,xxx1); to remove first one.
	//!
	//! Thread safe: yes (you can use multiple instances in parallel, multiple getXxx() in parallel,
	//!  just don't call setXxx() when some function already runs on the same instance)
	//
	/////////////////////////////////////////////////////////////////////////////

	class RR_API RRFileLocator
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! If non-empty (e.g. "foo/bar/" or "foo/bar/baz.3ds"), tells locator to look for files also in parent directory ("foo/bar/").
		//! Ignored by default implementation, honoured by create().
		virtual void setParent(bool add, const RRString& parentFilename) {}
		//! If both relocation filenames are non-empty (e.g. "c:/foo/bar/baz.3ds" and "d:/bar/baz.3ds"),
		//! locator calculates where requested file might be after such relocation.
		//! Ignored by default implementation, honoured by create().
		virtual void setRelocation(bool add, const RRString& relocationSourceFilename, const RRString& relocationDestinationFilename) {}
		//! Tells locator to try filename modified with regular expression.
		//! Ignored by default implementation, honoured by create().
		virtual void setRegexReplacement(bool add, const RRString& regex, const RRString& format) {}
		//! Tells locator to try these extensions (e.g. ".jpg;.png;.tga") for files without extension.
		//! Ignored by default implementation, honoured by create().
		virtual void setExtensions(bool add, const RRString& extensions) {}
		//! Makes getLocation(,attemptNumber) return location.
		//! See #AttemptNumber for reasons why you might want it.
		//! Ignored by default implementation, honoured by create().
		virtual void setAttempt(unsigned attemptNumber, const RRString& location) {}

		//! Tests whether this exact file or directory exists.
		//
		//! Only one test is made, without trying other possible locations.
		//! Default implementation is based on boost::filesystem::exists(), you are free to override it.
		virtual bool exists(const RRString& filename) const;

		//! Returns possible file location.
		//
		//! Default implementation returns original filename as the only attempt.
		//! \param originalFilename
		//!  Filename of file we are looking for, e.g. texture filename provided by 3d scene file.
		//!  (When user moves 3d scene to different disk, texture filenames may point to wrong locations, but there's still chance to deduce correct location.)
		//! \param attemptNumber
		//!  Attempt 0 returns the most likely location, 1 less likely etc.
		//! \return
		//!  Possible file location or empty string when attemptNumber is too high.
		virtual RRString getLocation(const RRString& originalFilename, unsigned attemptNumber) const;
		//! Returns file location or fallback string if not found. Uses getLocation(,i) and exists() for i=0,1,2...
		virtual RRString getLocation(const RRString& originalFilename, const RRString& fallbackFilename) const;

		virtual ~RRFileLocator() {}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates advanced file locator that honours all setXxx() functions.
		static RRFileLocator* create();

		enum AttemptNumber
		{
			//! When RRBuffer::load() fails and non-empty stub filename is specified by RRFileLocator::setAttempt(ATTEMPT_STUB,...),
			//! RRBuffer::load() creates stub buffer with original filename and stub content.
			//! This is important when processing and saving scene without textures
			//! (either intentionally, e.g. to speed up scene conversion, or unintentionally, e.g. when disk with textures is unmapped),
			//! stubs preserve paths to textures, even if those textures are not loaded.
			//! Stub content is loaded from location specified by RRFileLocator::setAttempt(ATTEMPT_STUB,...).
			//! If stub location is ":" or load fails, small 2d texture is generated in memory.
			ATTEMPT_STUB = 10000,
			//! Only supported by RRBuffer loaders.
			//! When non-empty stub location is specified by RRFileLocator::setAttempt(ATTEMPT_LOCATE_ONLY,...)
			//! and requested file can be located, given stub is loaded (if stub location is ":" or load fails, small 2d texture is generated)
			//! and its filename is set to the located filename.
			ATTEMPT_LOCATE_ONLY = 10001,
		};
	};

} // namespace

#endif
