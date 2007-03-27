#ifndef MUSIC_H
#define MUSIC_H

/////////////////////////////////////////////////////////////////////////////
//
// music

#include "fmod/fmod.hpp"

class Music
{
public:
	Music(const char* filename);
	void  poll();
	void  setPaused(bool paused);
	float getPosition();
	void  setPosition(float seconds);
	float getLength();
	~Music();
private:
	FMOD::System     *system;
	FMOD::Sound      *sound;
	FMOD::Channel    *channel;
};

#endif
