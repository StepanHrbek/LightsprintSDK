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
//rr::RRReporter::report(rr::INF1,"reloc2: %s\n",oldAbsoluteFilename.string().c_str());
		oldAbsoluteFilename.normalize();
//rr::RRReporter::report(rr::INF1,"reloc3: %s\n",oldAbsoluteFilename.string().c_str());
		oldAbsoluteFilename = oldBasePath.root_name() / oldAbsoluteFilename.root_directory() / oldAbsoluteFilename.relative_path();
//rr::RRReporter::report(rr::INF1,"reloc3+ %s\n",oldAbsoluteFilename.string().c_str());
		bf::path relativeFilename = getRelativeFile(oldAbsoluteFilename,oldBasePath);
//rr::RRReporter::report(rr::INF1,"reloc4: %s\n",relativeFilename.string().c_str());

		// make filename absolute using new reference
		bf::path newBasePath = system_complete(bf::path(newReference).parent_path());
		newBasePath.normalize();
		bf::path newAbsoluteFilename = bf::absolute(relativeFilename,newBasePath);
//rr::RRReporter::report(rr::INF1,"reloc5: %s\n",newAbsoluteFilename.string().c_str());
		newAbsoluteFilename.normalize();
//rr::RRReporter::report(rr::INF1,"reloc6: %s\n",newAbsoluteFilename.string().c_str());
//rr::RRReporter::report(rr::INF1,"\n");

		return newAbsoluteFilename;
	}

private:
	static bf::path getRelativeFile(const bf::path& absoluteFile, const bf::path& basePath)
	{
		bf::path::iterator itFrom = basePath.begin();
		bf::path::iterator itTo = absoluteFile.begin();
		std::string root1 = itFrom->string();
		std::string root2 = itTo->string();
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

bf::path fixNull(const char* p)
{
	return p?p:"";
}

template<class C>
void addOrRemove(bool _add, std::vector<C>& _paths, C& _path)
{
	if (_add)
		_paths.push_back(_path);
	else
	{
		for (size_t i=_paths.size();i--;)
			if (_paths[i]==_path)
			{
				_paths.erase(_paths.begin()+i);
				return;
			}
		RRReporter::report(WARN,"RRFileLocator::setXxx(false,x): attempt to remove x that was not added before.\n");
	}
}

class FileLocator : public RRFileLocator
{
public:
	virtual void setParent(bool _add, const char* _parentFilename)
	{
		addOrRemove(_add,parentFilenames,fixNull(_parentFilename));
	}
	virtual void setRelocation(bool _add, const char* _relocationSourceFilename, const char* _relocationDestinationFilename)
	{
		addOrRemove(_add,relocationFilenames,std::pair<bf::path,bf::path>(fixNull(_relocationSourceFilename),fixNull(_relocationDestinationFilename)));
	}
	virtual void setLibrary(bool _add, const char* _libraryDirectory)
	{
		addOrRemove(_add,libraryDirectories,fixNull(_libraryDirectory));
	}
	virtual void setExtensions(bool _add, const char* _extensions)
	{
		if (_extensions)
		{
			if (_add)
				boost::split(extensions,_extensions,boost::is_any_of(";"));
			else
			{
				std::vector<std::string> tmpExtensions;
				boost::split(tmpExtensions,_extensions,boost::is_any_of(";"));
				for (size_t i=0;i<tmpExtensions.size();i++)
				{
					for (size_t j=0;j<extensions.size();j++)
						if (tmpExtensions[i]==extensions[j])
						{
							extensions.erase(extensions.begin()+j);
							goto erased;
						}
					RRReporter::report(WARN,"RRFileLocator::setExtensions(false,%s): attempt to remove %s that was not added before.\n",_extensions,tmpExtensions[i].c_str());
					erased:;
				}
			}
		}
	}
	virtual RRString getLocation(const char* originalFilename, unsigned attemptNumber) const
	{
		return getLocation(fixNull(originalFilename),attemptNumber).string().c_str();
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
		if ((parentFilenames.empty() || originalFilename.has_root_path()) && !attemptNumber--)
			return originalFilename;

		// relative to parent
		for (size_t i=0;i<parentFilenames.size();i++)
		{
			if (!parentFilenames[i].empty() && !attemptNumber--)
				return parentFilenames[i].parent_path() / originalFilename.relative_path();
			if (!parentFilenames[i].empty() && originalFilename.relative_path().has_parent_path() && !attemptNumber--)
				return parentFilenames[i].parent_path() / originalFilename.filename();
		}

		// relocated
		for (size_t i=0;i<relocationFilenames.size();i++)
		{
			if (!relocationFilenames[i].first.empty() && !relocationFilenames[i].second.empty() && originalFilename.has_root_path() && !attemptNumber--)
				return Relocator::getRelocatedFilename(originalFilename,relocationFilenames[i].first,relocationFilenames[i].second);
		}

		// library
		for (size_t i=0;i<libraryDirectories.size();i++)
		{
			if (!libraryDirectories[i].empty() && !attemptNumber--)
				return libraryDirectories[i].parent_path() / originalFilename.relative_path();
			if (!libraryDirectories[i].empty() && originalFilename.relative_path().has_parent_path() && !attemptNumber--)
				return libraryDirectories[i].parent_path() / originalFilename.filename();
		}

		return "";
	}
	std::vector<bf::path> parentFilenames;
	std::vector<std::pair<bf::path,bf::path> > relocationFilenames;
	std::vector<bf::path> libraryDirectories;
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
