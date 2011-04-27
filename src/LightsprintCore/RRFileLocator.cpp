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
// FileLocator

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
		// hack for renderlights
		if (attemptNumber==123456)
		{
			if (relocationFilenames.size())
				return getRelocatedFilename(originalFilename,relocationFilenames[0].first,relocationFilenames[0].second);
			else
				return originalFilename;
		}

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
				return getRelocatedFilename(originalFilename,relocationFilenames[i].first,relocationFilenames[i].second);
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

	// see how oldReference did transform into newReference and apply the same transformation on filename
	static bf::path getRelocatedFilename(const bf::path& filename, const bf::path& oldReference, const bf::path& newReference)
	{
		if (filename.empty())
			return "";

		// relative path or bad inputs: just make it absolute
		if (!filename.has_root_directory() || oldReference.empty() || newReference.empty())
		{
			return bf::system_complete(filename).normalize();
		}

		// absolute path: relocate
		bf::path relativeFilename = relative(filename,bf::path(oldReference).parent_path());
		bf::path relocatedFilename = bf::absolute(relativeFilename,system_complete(bf::path(newReference).parent_path())).normalize();
		return relocatedFilename;
	}

	static bf::path relative(bf::path file, bf::path base)
	{
		file = bf::system_complete(file).relative_path().normalize();
		base = bf::system_complete(base).relative_path().normalize();
		bf::path::const_iterator itBase = base.begin();
		bf::path::const_iterator itFile = file.begin();
		while ((itBase!=base.end()) && (itFile!=file.end()) && (*itBase==*itFile))
		{
			++itBase;
			++itFile;
		}
		bf::path result;
		for (;itBase!=base.end();++itBase)
			result /= "..";
		for (;itFile!=file.end();++itFile)
			result /= *itFile;
		return result;
	}

	static bf::path fixNull(const char* p)
	{
		return p?p:"";
	}

	template<class C>
	static void addOrRemove(bool _add, std::vector<C>& _paths, const C& _path)
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

static bool exists(const char* filename)
{
	FILE* f = fopen(filename,"rb");
	if (!f) return false;
	fclose(f);
	return true;
}

RRString RRFileLocator::getLocation(const char* originalFilename) const
{
	for (unsigned attempt=0;attempt<UINT_MAX;attempt++)
	{
		RRString location = getLocation(originalFilename,attempt);
		if (location.empty())
			return "";
rr::RRReporter::report(rr::INF2," %d%c %s\n",attempt,exists(location.c_str())?'+':'-',location.c_str());
		if (exists(location.c_str()))
		{
			return location;
		}
	}
	return "";
}

RRFileLocator* RRFileLocator::create()
{
	return new FileLocator;
}

} //namespace
