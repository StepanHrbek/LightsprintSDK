#ifndef LEVELSEQUENCE_H
#define LEVELSEQUENCE_H

#include <list>

class LevelSequence
{
public:
	LevelSequence()
	{
		insertLevelBack("3ds\\koupelna\\koupelna4.3ds");
		insertLevelBack("3ds\\Quake3\\Trajectory\\Trajectory.3ds"); // pekne
		insertLevelBack("3ds\\koupelna\\koupelna3.3ds");
		insertLevelBack("3ds\\koupelna\\koupelna5.3ds");
		insertLevelBack("3ds\\sponza\\sponza.3ds");
		insertLevelBack("3ds\\koupelna\\koupelna41.3ds");// vse spojene do 1 objektu
		insertLevelBack("3ds\\Quake3\\X3maps\\X3map04.3ds"); // pekna geometrie, ale hodne prace s texturami (asi 400, bud vse prejmenovat nebo nacitat rovnou bsp)
		insertLevelBack("3ds\\Quake3\\ramp3ctf1\\ramp3ctf1.3ds"); // rampy v prostoru, je tezsi najit indirect, chybi par textur
		insertLevelBack("3ds\\quake\\quakecomp2.3ds");
	}
	void insertLevelFront(const char* filename)
	{
		sequence.push_front(filename);
		it = sequence.begin();
	}
	void insertLevelBack(const char* filename)
	{
		sequence.push_back(filename);
		it = sequence.begin();
	}
	const char* getNextLevel()
	{
		if(it==sequence.end()) return NULL;
		const char* result = *it;
		it++;
		if(it==sequence.end()) it=sequence.begin();
		return result;
	}
private:
	std::list<const char*> sequence;
	std::list<const char*>::iterator it;
};

#endif
