// --------------------------------------------------------------------------
// Support for relocating files with absolute paths.
// Copyright (C) 2009-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRRELOCATION_H
#define RRRELOCATION_H

#include <string>
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

//! Helper for relocating absolute paths.
class RRRelocator
{
public:
	std::string oldReference;
	std::string newReference;

	void relocateFilename(std::string& filename)
	{
		relocateFilename(filename,oldReference,newReference);
	}

	std::string relocatedFilename(const std::string& filename)
	{
		std::string s = filename;
		relocateFilename(s);
		return s;
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

	// removes all dir/.. occurences from path
	static void removeUnnecessaryDots(bf::path& path)
	{
	retry:
		for (bf::path::iterator i = path.begin(); i!=path.end(); ++i)
		{
			bf::path::iterator iplus1 = i;
			++iplus1;
			if (*i!=".." && *iplus1=="..")
			{
				// remove *i and *iplus1 from path and retry
				bf::path path2;
				for (bf::path::iterator j = path.begin(); j!=path.end(); ++j)
				{
					if (j!=i && j!=iplus1)
						path2 /= *j;
				}
				path = path2;
				goto retry;
			}
		}
	}

	// see how oldReference transformed into newReference and do the same transformation on filename
	static void relocateFilename(std::string& filename, const std::string& oldReference, const std::string& newReference)
	{
		if (!filename.empty())
		{
			if (bf::path(filename).has_root_directory())
			{
				// absolute path: relocate

				// make filename relative using old reference
	//rr::RRReporter::report(rr::INF1,"reloc1: %s\n",filename.c_str());
				bf::path oldBasePath = system_complete(bf::path(oldReference).parent_path());
				removeUnnecessaryDots(oldBasePath);
				bf::path oldAbsoluteFilename = bf::system_complete(filename);
	//rr::RRReporter::report(rr::INF1,"reloc2: %s\n",oldAbsoluteFilename.file_string().c_str());
				removeUnnecessaryDots(oldAbsoluteFilename);
	//rr::RRReporter::report(rr::INF1,"reloc3: %s\n",oldAbsoluteFilename.file_string().c_str());
				bf::path relativeFilename = getRelativeFile(oldAbsoluteFilename,oldBasePath);
	//rr::RRReporter::report(rr::INF1,"reloc4: %s\n",relativeFilename.file_string().c_str());

				// make filename absolute using new reference
				bf::path newBasePath = system_complete(bf::path(newReference).parent_path());
				removeUnnecessaryDots(newBasePath);
				bf::path newAbsoluteFilename = bf::complete(relativeFilename,newBasePath);
	//rr::RRReporter::report(rr::INF1,"reloc5: %s\n",newAbsoluteFilename.file_string().c_str());
				removeUnnecessaryDots(newAbsoluteFilename);
	//rr::RRReporter::report(rr::INF1,"reloc6: %s\n",newAbsoluteFilename.file_string().c_str());

				filename = newAbsoluteFilename.file_string();
			}
			else
			{
				// relative path: just make it absolute
				bf::path newAbsoluteFilename = bf::system_complete(filename);
				removeUnnecessaryDots(newAbsoluteFilename);
				filename = newAbsoluteFilename.file_string();
			}
		}
	}
};

#endif
