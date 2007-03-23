#ifndef ANIMATIONSCENE_H
#define ANIMATIONSCENE_H

#include <list>
#include "AnimationFrame.h"

/////////////////////////////////////////////////////////////////////////////
//
// LevelSetup, all data from .ani file in editable form

struct LevelSetup
{
	// constant setup
	const char* filename;
	float scale;
	typedef std::list<AnimationFrame> Frames;
	Frames frames;

	LevelSetup(const char* afilename);
	~LevelSetup();

	// load all from .ani
	// afilename is name of scene, e.g. path/koupelna4.3ds
	bool load(const char* afilename);

	// save all to .ani file
	bool save() const;

	// returns iterator to n-th frame
	Frames::iterator getFrame(unsigned index);
};

#endif
