// --------------------------------------------------------------------------
// Helper for locating files.
// Copyright (c) 2006-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRFileLocator.h"
#include "Lightsprint/RRDebug.h"

#include <vector>
#include <map>
#include <boost/algorithm/string.hpp> // split, is_any_of
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>
namespace bf = boost::filesystem;

// helper for rr_io,
// single global variable shared by all libraries that include RRSerialization.h
RR_API rr::RRFileLocator* g_textureLocator = NULL;

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
	virtual void setRegexReplacement(bool _add, const RRString& _regex, const RRString& _format)
	{
		addOrRemove(_add,regexReplacements,std::pair<std::wstring,std::wstring>(RR_RR2STDW(_regex),RR_RR2STDW(_format)));
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
	virtual void setAttempt(unsigned attemptNumber, const RRString& location)
	{
		specialAttempts[attemptNumber] = location;
	}
	virtual RRString getLocation(const RRString& originalFilename, unsigned attemptNumber) const
	{
		// special attempts
		if (specialAttempts.find(attemptNumber)!=specialAttempts.end())
			return specialAttempts.find(attemptNumber)->second;

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

		// regex replaced
		for (size_t i=0;i<regexReplacements.size();i++)
		{
			bf::path regexReplaced = getRegexReplacedFilename(originalFilename,regexReplacements[i].first,regexReplacements[i].second);
			if (!regexReplaced.empty() && !attemptNumber--)
				return regexReplaced;
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

//		RRReporter::report(INF3," relocation:\n");
//		RRReporter::report(INF3,"   oldRef=%s\n",oldReference.string().c_str());
//		RRReporter::report(INF3,"   newRef=%s\n",newReference.string().c_str());
//		RRReporter::report(INF3,"   filename          =%s\n",filename.string().c_str());
		bf::path relativeFilename = relative(filename,oldReference.parent_path());
		bf::path relocatedFilename = bf::absolute(relativeFilename,system_complete(newReference.parent_path())).normalize();
//		RRReporter::report(INF3,"   relativeFilename  =%s\n",relativeFilename.string().c_str());
//		RRReporter::report(INF3,"   step1             =%s\n",newReference.parent_path().string().c_str());
//		RRReporter::report(INF3,"   step2             =%s\n",system_complete(newReference.parent_path()).string().c_str());
//		RRReporter::report(INF3,"   relocatedFilename =%s\n",relocatedFilename.string().c_str());
		return relocatedFilename;
	}

	static bf::path relative(bf::path file, bf::path base)
	{
//		RRReporter::report(INF3,"     file            =%s\n",file.string().c_str());
//		RRReporter::report(INF3,"     base            =%s\n",base.string().c_str());
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
//		RRReporter::report(INF3,"     file            =%s\n",file.string().c_str());
//		RRReporter::report(INF3,"     base            =%s\n",base.string().c_str());
//		RRReporter::report(INF3,"     mid.result      =%s\n",result.string().c_str());
		for (;itFile!=file.end();++itFile)
			result /= *itFile;
		return result;
	}

	// use regex to replace parts of filename
	static bf::path getRegexReplacedFilename(const bf::path& filename, const std::wstring& regex, const std::wstring& format)
	{
		boost::wregex re(regex);
		std::wstring output = boost::regex_replace(filename.wstring(),re,format,boost::regex_constants::format_default|boost::regex_constants::format_first_only|boost::regex_constants::format_no_copy);
//		RRReporter::report(INF3,"     regex: %s -> %ls\n",filename.string().c_str(),output.c_str());
		return bf::path(output);
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
	std::vector<std::pair<std::wstring,std::wstring> > regexReplacements;
	std::vector<bf::path> libraryDirectories;
	std::vector<std::string> extensions;
	std::map<unsigned,RRString> specialAttempts;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRFileLocator

bool RRFileLocator::exists(const RRString& filename) const
{
	if (filename=="c@pture")
		return true;

	boost::system::error_code ec;
	return bf::exists(RR_RR2PATH(filename),ec);
}

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
		bool exists = this->exists(location);
		RRReporter::report(INF3," %d%c %s\n",attempt,exists?'+':'-',location.c_str());
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
