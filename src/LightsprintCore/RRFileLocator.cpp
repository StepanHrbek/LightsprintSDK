// --------------------------------------------------------------------------
// Helper for locating files.
// Copyright (c) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRFileLocator.h"
#include "Lightsprint/RRDebug.h"

#include <boost/algorithm/string.hpp> // split, is_any_of
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Relocator - helper for relocating absolute paths

class Relocator
{
public:
	// use before saving path
	static bf::path getAbsoluteFilename(const bf::path& filename)
	{
		return bf::system_complete(filename).normalize();
	}

	// use after loading path
	// see how oldReference did transform into newReference and apply the same transformation on filename
	static bf::path getRelocatedFilename(const bf::path& filename, const bf::path& oldReference, const bf::path& newReference)
	{
//rr::RRReporter::report(rr::INF1,"reloc0: %s (%s -> %s)\n",filename.c_str(),oldReference.c_str(),newReference.c_str());
		if (filename.empty())
			return "";

		if (!filename.has_root_directory())
		{
			// relative path: just make it absolute
			return getAbsoluteFilename(filename);
		}

		// absolute path: relocate

		// make filename relative using old reference
//rr::RRReporter::report(rr::INF1,"reloc1: %s\n",filename.c_str());
		bf::path oldBasePath = system_complete(bf::path(oldReference).parent_path());
		oldBasePath.normalize();
		bf::path oldAbsoluteFilename = bf::system_complete(filename);
//rr::RRReporter::report(rr::INF1,"reloc2: %s\n",oldAbsoluteFilename.file_string().c_str());
		oldAbsoluteFilename.normalize();
//rr::RRReporter::report(rr::INF1,"reloc3: %s\n",oldAbsoluteFilename.file_string().c_str());
		if (oldAbsoluteFilename.root_name()!=oldBasePath.root_name())
			oldAbsoluteFilename = (oldBasePath.root_name()+oldAbsoluteFilename.root_directory())/oldAbsoluteFilename.relative_path();
//rr::RRReporter::report(rr::INF1,"reloc3+ %s\n",oldAbsoluteFilename.file_string().c_str());
		bf::path relativeFilename = getRelativeFile(oldAbsoluteFilename,oldBasePath);
//rr::RRReporter::report(rr::INF1,"reloc4: %s\n",relativeFilename.file_string().c_str());

		// make filename absolute using new reference
		bf::path newBasePath = system_complete(bf::path(newReference).parent_path());
		newBasePath.normalize();
		bf::path newAbsoluteFilename = bf::complete(relativeFilename,newBasePath);
//rr::RRReporter::report(rr::INF1,"reloc5: %s\n",newAbsoluteFilename.file_string().c_str());
		newAbsoluteFilename.normalize();
//rr::RRReporter::report(rr::INF1,"reloc6: %s\n",newAbsoluteFilename.file_string().c_str());
//rr::RRReporter::report(rr::INF1,"\n");

		return newAbsoluteFilename;
	}

private:
	static bf::path getRelativeFile(const bf::path& absoluteFile, const bf::path& basePath)
	{
		bf::path::iterator itFrom = basePath.begin();
		bf::path::iterator itTo = absoluteFile.begin();
		std::string root1 = *itFrom;
		std::string root2 = *itTo;
		if (root1!=root2)
		{
	#ifdef _WIN32
			// windows hack: skip first elements if they are c: and C:
			// (windows file selector returns C:, boost system_complete returns c:)
			if (!_stricmp(root1.c_str(),root2.c_str()))
			{
				++itFrom;
				++itTo;
			}
			else
	#endif
			// first elements really differ, panic
			{
				RR_ASSERT(*itFrom == *itTo );
				return absoluteFile;
			}
		}
		while ( (itFrom != basePath.end()) && (itTo != absoluteFile.end()) && (*itFrom == *itTo) )
		{
			++itFrom;
			++itTo;
		}
		bf::path relPath;
		for ( ;itFrom != basePath.end(); ++itFrom )
		{
			relPath /= "..";
		}
		for ( ;itTo != absoluteFile.end(); ++itTo)
		{
			relPath /= *itTo;
		}
		return relPath;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// FileLocator

#define REPLACE_NULL(s) ((s)?(s):"")

class FileLocator : public RRFileLocator
{
public:
	virtual void setParent(const char* _parentFilename)
	{
		parentFilename = REPLACE_NULL(_parentFilename);
	}
	virtual void setRelocation(const char* _relocationSourceFilename, const char* _relocationDestinationFilename)
	{
		relocationSourceFilename = REPLACE_NULL(_relocationSourceFilename);
		relocationDestinationFilename = REPLACE_NULL(_relocationDestinationFilename);
	}
	virtual void setLibrary(const char* _libraryDirectory)
	{
		libraryDirectory = REPLACE_NULL(_libraryDirectory);
	}
	virtual void setExtensions(const char* _extensions)
	{
		extensions.clear();
		if (_extensions)
			boost::split(extensions,_extensions,boost::is_any_of(";"));
	}
	virtual RRString getLocation(const char* originalFilename, unsigned attemptNumber) const
	{
		return getLocation(bf::path(REPLACE_NULL(originalFilename)),attemptNumber).file_string().c_str();
	}
protected:
	bf::path getLocation(bf::path originalFilename, unsigned attemptNumber) const
	{
		// extensions
		if (originalFilename.extension().empty() && extensions.size())
		{
			if (attemptNumber>=1000000)
			{
				attemptNumber -= 1000000;
			}
			else
			{
				bf::path result = getLocation(originalFilename,(unsigned)(attemptNumber/extensions.size()+1000000));
				return result.empty() ? result : result.replace_extension(extensions[attemptNumber%extensions.size()]);
			}
		}

		// original
		if ((parentFilename.empty() || originalFilename.has_root_path()) && !attemptNumber--)
			return originalFilename;

		// relative to parent
		if (!parentFilename.empty() && !attemptNumber--)
			return parentFilename.parent_path() / originalFilename.relative_path();
		if (!parentFilename.empty() && originalFilename.relative_path().has_parent_path() && !attemptNumber--)
			return parentFilename.parent_path() / originalFilename.filename();

		// relocated
		if (!relocationSourceFilename.empty() && !relocationDestinationFilename.empty() && originalFilename.has_root_path() && !attemptNumber--)
			return Relocator::getRelocatedFilename(originalFilename,relocationSourceFilename,relocationDestinationFilename);

		// library
		if (!libraryDirectory.empty() && !attemptNumber--)
			return libraryDirectory.parent_path() / originalFilename.relative_path();
		if (!libraryDirectory.empty() && originalFilename.relative_path().has_parent_path() && !attemptNumber--)
			return libraryDirectory.parent_path() / originalFilename.filename();

		return "";
	}
	bf::path parentFilename;
	bf::path relocationSourceFilename;
	bf::path relocationDestinationFilename;
	bf::path libraryDirectory;
	std::vector<std::string> extensions;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRFileLocator

RRString RRFileLocator::getLocation(const char* originalFilename, unsigned attemptNumber) const
{
	return attemptNumber ? NULL : originalFilename;
}

RRFileLocator* RRFileLocator::create()
{
	return new FileLocator;
}

} //namespace
