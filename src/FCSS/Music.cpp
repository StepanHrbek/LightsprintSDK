/////////////////////////////////////////////////////////////////////////////
//
// music

#pragma comment(lib,"fmodex_vc.lib")

#include "Music.h"
#include "fmod/fmod_errors.h"
#include "Lightsprint/RRDebug.h"
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

void ERRCHECK(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		rr::RRReporter::report(rr::ERRO,"FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		rr::RRReporter::report(rr::INF1,"Press Enter to end...");
		fgetc(stdin);
		exit(-1);
	}
}

Music::Music(const char* filename)
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

	result = system->createSound(filename, FMOD_HARDWARE | FMOD_LOOP_NORMAL | FMOD_2D | FMOD_ACCURATETIME, 0, &sound);
	ERRCHECK(result);

	result = system->playSound(FMOD_CHANNEL_FREE, sound, true, &channel);
	ERRCHECK(result);
}

void Music::poll()
{
	system->update();
}

void Music::setPaused(bool paused)
{
//	printf("musicPaused(%d)\n",paused?1:0);
	if(channel)
		channel->setPaused(paused);
}

float Music::getPosition()
{
	unsigned ms = 0;
	if(channel)
	{
		// kdyz loopne, zacne vracet casy od 0
		channel->getPosition(&ms,FMOD_TIMEUNIT_MS);
	}
//	printf("musicGetPos()=%f\n",ms*0.001f);
	return ms*0.001f;
}

void Music::setPosition(float seconds)
{
//	printf("musicSetPos(%f)\n",seconds);
	if(channel)
	{
		channel->setPosition((unsigned)(seconds*1000),FMOD_TIMEUNIT_MS);
	}
}

float Music::getLength()
{
	unsigned ms = 0;
	if(sound)
	{
		sound->getLength(&ms,FMOD_TIMEUNIT_MS);
	}
	return ms*0.001f;
}

Music::~Music()
{
	FMOD_RESULT result;
	if(sound)
	{
		result = sound->release();
		ERRCHECK(result);
	}
	if(system)
	{
		result = system->close();
		ERRCHECK(result);
		result = system->release();
		ERRCHECK(result);
	}
}
