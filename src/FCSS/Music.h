#ifndef MUSIC_H
#define MUSIC_H

/////////////////////////////////////////////////////////////////////////////
//
// music

#include "fmod/fmod.hpp"

class Music
{
public:
	Music(char* filename);
	void poll();
	~Music();
private:
	FMOD::System     *system;
	FMOD::Sound      *sound;
	FMOD::Channel    *channel;
};

#endif
