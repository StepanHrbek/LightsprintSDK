/////////////////////////////////////////////////////////////////////////////
//
// music

#ifdef _M_X64
	#pragma comment(lib,"fmodex64_vc.lib")
#else
	#pragma comment(lib,"fmodex_vc.lib")
#endif

#include "Music.h"
#include "fmodex/fmod_errors.h"
#include "Lightsprint/RRDebug.h"
#include <stdio.h>
//#include <conio.h>
#include <string.h>
#include <stdlib.h>

Music* Music::load(const char* filename)
{
	rr::RRReportInterval report(rr::INF2,"Loading %s...\n",filename);

	Music* music = new Music();

	FMOD_RESULT result;
	result = FMOD::System_Create(&music->system);
	if(result!=FMOD_OK) goto err;

	unsigned int version;
	result = music->system->getVersion(&version);
	if(result!=FMOD_OK) goto err;
	if(version < FMOD_VERSION)
	{
		rr::RRReporter::report(rr::ERRO,"Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", version, FMOD_VERSION);
		goto err_printed;
	}


	result = music->system->setStreamBufferSize(0x40000, FMOD_TIMEUNIT_RAWBYTES);
	if(result!=FMOD_OK) goto err;

	result = music->system->init(1, FMOD_INIT_NORMAL, 0);
	if(result!=FMOD_OK) goto err;

	result = music->system->createSound(filename, FMOD_HARDWARE | FMOD_LOOP_NORMAL | FMOD_2D | FMOD_ACCURATETIME, 0, &music->sound);
	if(result!=FMOD_OK) goto err;

	result = music->system->playSound(FMOD_CHANNEL_FREE, music->sound, true, &music->channel);
	if(result!=FMOD_OK) goto err;

	return music;

err:
	rr::RRReporter::report(rr::ERRO,"FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
err_printed:
	delete music;
	return NULL;
}

Music::Music()
{
	memset(this,0,sizeof(*this));
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

void Music::setVolume(float volume)
{
	if(channel)
		channel->setVolume(volume);
}

Music::~Music()
{
	FMOD_RESULT result;
	if(sound)
	{
		result = sound->release();
	}
	if(system)
	{
		result = system->close();
		result = system->release();
	}
}
