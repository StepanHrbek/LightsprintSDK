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
	virtual void setParent(bool _add, const RRString& _parentFilename)
	{
		if (!_parentFilename.empty())
			addOrRemove(_add,parentFilenames,bf::path(RR_RR2PATH(_parentFilename)));
	}
	virtual void setRelocation(bool _add, const RRString& _relocationSourceFilename, const RRString& _relocationDestinationFilename)
	{
		if (!_relocationSourceFilename.empty() && !_relocationDestinationFilename.empty())
			addOrRemove(_add,relocationFilenames,std::pair<bf::path,bf::path>(RR_RR2PATH(_relocationSourceFilename),RR_RR2PATH(_relocationDestinationFilename)));
	}
	virtual void setLibrary(bool _add, const RRString& _libraryDirectory)
	{
		if (!_libraryDirectory.empty())
			addOrRemove(_add,libraryDirectories,bf::path(RR_RR2PATH(_libraryDirectory)));
	}
	virtual void setExtensions(bool _add, const RRString& _extensions)
	{
		const char* tmpExtensions2 = _extensions.c_str();
		if (_add)
			boost::split(extensions,tmpExtensions2,boost::is_any_of(";"));
		else
		{
			std::vector<std::string> tmpExtensions;
			boost::split(tmpExtensions,tmpExtensions2,boost::is_any_of(";"));
			for (size_t i=0;i<tmpExtensions.size();i++)
			{
				for (size_t j=0;j<extensions.size();j++)
					if (tmpExtensions[i]==extensions[j])
					{
						extensions.erase(extensions.begin()+j);
						goto erased;
					}
				RRReporter::report(WARN,"RRFileLocator::setExtensions(false,%s): attempt to remove %s that was not added before.\n",_extensions.c_str(),tmpExtensions[i].c_str());
				erased:;
			}
		}
	}
	virtual RRString getLocation(const RRString& originalFilename, unsigned attemptNumber) const
	{
		return RR_PATH2RR(getLocation(bf::path(RR_RR2PATH(originalFilename)),attemptNumber));
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
			if (!attemptNumber--)
				return parentFilenames[i].parent_path() / originalFilename.relative_path();
			if (originalFilename.relative_path().has_parent_path() && !attemptNumber--)
				return parentFilenames[i].parent_path() / originalFilename.filename();
		}

		// relocated
		for (size_t i=0;i<relocationFilenames.size();i++)
		{
			if (!attemptNumber--)
				return getRelocatedFilename(originalFilename,relocationFilenames[i].first,relocationFilenames[i].second);
		}

		// library
		for (size_t i=0;i<libraryDirectories.size();i++)
		{
			if (!attemptNumber--)
				return libraryDirectories[i].parent_path() / originalFilename.relative_path();
			if (originalFilename.relative_path().has_parent_path() && !attemptNumber--)
				return libraryDirectories[i].parent_path() / originalFilename.filename();
		}

		return "";
	}

	// see how oldReference did transform into newReference and apply the same transformation on filename
	static bf::path getRelocatedFilename(const bf::path& filename, const bf::path& oldReference, const bf::path& newReference)
	{
		if (filename.empty())
			return "";

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

RRString RRFileLocator::getLocation(const RRString& originalFilename, unsigned attemptNumber) const
{
	return attemptNumber ? "" : originalFilename;
}

RRString RRFileLocator::getLocation(const RRString& originalFilename, const RRString& fallbackFilename) const
{
	for (unsigned attempt=0;attempt<UINT_MAX;attempt++)
	{
		RRString location = getLocation(originalFilename,attempt);
		if (location.empty())
			return fallbackFilename;
		bool exists = bf::exists(RR_RR2PATH(location));
		rr::RRReporter::report(rr::INF3," %d%c %s\n",attempt,exists?'+':'-',location.c_str());
		if (exists)
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
