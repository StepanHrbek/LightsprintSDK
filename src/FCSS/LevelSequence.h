#ifndef LEVELSEQUENCE_H
#define LEVELSEQUENCE_H

#include <list>

class LevelSequence
{
public:
	LevelSequence()
	{
		//insertLevelBack("3ds\\candella\\c-part.3ds");
		//insertLevelBack("3ds\\candella\\c-all.3ds");
		//insertLevelBack("3ds\\candella\\candella.3ds");

		insertLevelBack("3ds\\koupelna\\koupelna4.3ds");             // ++colorbleed, LIC=OK, 2M
		insertLevelBack("bsp\\x3map\\maps\\x3map05.bsp");            // ++ext, originalni geometrie levelu, LIC=OK, 12M
		insertLevelBack("bsp\\bgmp\\maps\\bgmp8.bsp");               // ++pekne skaly a odrazy od zelene travy, int i ext, chybi malinko textur, LIC=OK, 25M/2
		insertLevelBack("bsp\\bgmp\\maps\\bgmp6.bsp");               // ++pekne drevo, int, chybi malinko textur, LIC=OK, 25M/2
		/*
		insertLevelBack("bsp\\bastir\\maps\\bastir.bsp");            // ++pekny bunkr na snehu, int i ext, jen snih neni sesmoothovany, chybi malinko textur, LIC=asine
		insertLevelBack("bsp\\qxdm3\\maps\\qxdm3.bsp");              //  +chodby a trochu terenu
		insertLevelBack("bsp\\bgmp\\maps\\bgmp5.bsp");               //  +dost chodeb
		insertLevelBack("bsp\\charon3dm12\\maps\\charon3dm12.bsp");  //  +pekna kovova sachta, ale chybi par textur
		insertLevelBack("3ds\\Trajectory\\Trajectory.3ds");          //  +original geometrie, LIC=BAD
		insertLevelBack("bsp\\trajectory\\maps\\trajectory.bsp");    //  +original geometrie, LIC=BAD
		insertLevelBack("bsp\\bgmp\\maps\\bgmp7.bsp");               //  -moc otevrene, nema podlahu (presto jsou odrazy dobre videt diky bilym zdem)
		insertLevelBack("bsp\\bgmp\\maps\\bgmp9.bsp");               //  -moc otevrene, asi chybi textury (presto jsou odrazy dobre videt diky bilym zdem)
		insertLevelBack("bsp\\x3map\\maps\\x3map03.bsp");            //  -chybi par textur
		insertLevelBack("bsp\\x3map\\maps\\x3map07.bsp");            //  -otevrene nebe, odrazy jsou videt ale model banalni hrad, problemova triangulace
		insertLevelBack("bsp\\kitfinal\\maps\\kitfinal.bsp");        //  -chodby, chybi par textur
		insertLevelBack("bsp\\qfraggel3tdm\\maps\\qfraggel3tdm.bsp");//  -chodby, chybi par textur
		insertLevelBack("3ds\\sponza\\sponza.3ds");
		insertLevelBack("3ds\\quake\\quakecomp2.3ds");
		insertLevelBack("3ds\\koupelna\\koupelna3.3ds");
		insertLevelBack("3ds\\koupelna\\koupelna5.3ds");
		insertLevelBack("3ds\\koupelna\\koupelna41.3ds");            // vse spojene do 1 objektu
		*/
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
