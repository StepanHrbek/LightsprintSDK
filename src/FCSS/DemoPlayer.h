#ifndef DEMOPLAYER_H
#define DEMOPLAYER_H

class DemoPlayer
{
public:
	DemoPlayer(const char* musicFilename);
	~DemoPlayer();
	void advance(float seconds);
	void setPaused(bool paused);
	bool getPaused();
	float getDemoPosition();
	float getPartPosition();
	void  setPartPosition(float seconds);
private:
	bool paused;
	class Music* music;
	float demoTime; // 0..demo duration in seconds
	float partStart; // 0..demo duration in seconds, time when current part started
};

#endif
