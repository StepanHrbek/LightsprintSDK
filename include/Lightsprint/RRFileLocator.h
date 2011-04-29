#ifndef RRFILELOCATOR_H
#define RRFILELOCATOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRFileLocator.h
//! \brief LightsprintCore | helper for locating files
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMemory.h"

namespace rr
{

	/////////////////////////////////////////////////////////////////////////////
	//
	//! Provides possible file locations when exact location is not known.
	//
	//! All inputs and outputs are in local charset (may change in future).
	//! \n Inputs don't have to be permanent, internal copies are created.
	//!
	//! Locator supports multiple libraries, parents etc,
	//! call setLibrary(true,lib1); setLibrary(true,lib2); to add two libraries.
	//! Call setLibrary(false,lib1); to remove one of libraries.
	//!
	//! Default implementation ignores all setXxx() functions,
	//! use create() for advanced locator.
	//!
	//! Thread safe: yes.
	//
	/////////////////////////////////////////////////////////////////////////////

	class RR_API RRFileLocator
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! If non-empty (e.g. "foo/bar/baz.3ds"), tells locator to look for files also in parent directory ("foo/bar/").
		//! Ignored by default implementation, honoured by create().
		virtual void setParent(bool add, const RRString& parentFilename) {}
		//! If both relocation filenames are non-empty (e.g. "c:/foo/bar/baz.3ds" and "d:/bar/baz.3ds"),
		//! locator calculates where requested file might be after such relocation.
		//! Ignored by default implementation, honoured by create().
		virtual void setRelocation(bool add, const RRString& relocationSourceFilename, const RRString& relocationDestinationFilename) {}
		//! Tells locator to look for files also in this directory (e.g. "foo/bar/").
		//! Ignored by default implementation, honoured by create().
		virtual void setLibrary(bool add, const RRString& libraryDirectory) {}
		//! Tells locator to try these extensions (e.g. ".jpg;.png;.tga") for files without extension.
		virtual void setExtensions(bool add, const RRString& extensions) {}

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
		//! Returns file location or fallback string if not found. Uses getLocation(,i) and boost::filesystem::exists() for i=0,1,2...
		virtual RRString getLocation(const RRString& originalFilename, const RRString& fallbackFilename) const;

		virtual ~RRFileLocator() {}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates advanced file locator that honours all setXxx() functions.
		static RRFileLocator* create();
	};

} // namespace

#endif
