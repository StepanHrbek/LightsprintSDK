/////////////////////////////////////////////////////////////////////////////
//
// music

#pragma comment(lib,"fmodex_vc.lib")

#include "Music.h"
#include "fmod/fmod_errors.h"
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

void ERRCHECK(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}
}

Music::Music(char* filename)
{
	memset(this,0,sizeof(*this));

	FMOD_RESULT result;
	result = FMOD::System_Create(&system);
	ERRCHECK(result);

	unsigned int version;
	result = system->getVersion(&version);
	ERRCHECK(result);

	if(version < FMOD_VERSION)
	{
		printf("Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", version, FMOD_VERSION);
		return;
	}

	result = system->init(1, FMOD_INIT_NORMAL, 0);
	ERRCHECK(result);

	//result = system->createStream("music/aoki.mp3", FMOD_HARDWARE | FMOD_LOOP_NORMAL | FMOD_2D, 0, &sound);
	result = system->createSound(filename, FMOD_HARDWARE | FMOD_LOOP_NORMAL | FMOD_2D, 0, &sound);
	ERRCHECK(result);

	result = system->playSound(FMOD_CHANNEL_FREE, sound, false, &channel);
	ERRCHECK(result);
}

void Music::poll()
{
	system->update();
}

Music::~Music()
{
	FMOD_RESULT result;
	result = sound->release();
	ERRCHECK(result);
	result = system->close();
	ERRCHECK(result);
	result = system->release();
	ERRCHECK(result);
}
