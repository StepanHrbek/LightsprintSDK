#ifndef MUSIC_H
#define MUSIC_H

/////////////////////////////////////////////////////////////////////////////
//
// music

#include "fmodex/fmod.hpp"

class Music
{
public:
	static Music* load(const char* filename);
	void  poll();
	void  setPaused(bool paused);
	float getPosition();
	void  setPosition(float seconds);
	float getLength();
	void  setVolume(float volume);
	~Music();

private:
	Music();
	FMOD::System     *system;
	FMOD::Sound      *sound;
	FMOD::Channel    *channel;
};

#endif
