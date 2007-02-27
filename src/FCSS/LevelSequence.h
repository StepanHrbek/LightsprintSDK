#ifndef LEVELSEQUENCE_H
#define LEVELSEQUENCE_H

#include <list>
#include "Autopilot.h"

LevelSetup koupelna4 =
{
	"3ds\\koupelna\\koupelna4.3ds",
	0.03f,
	{
	{
		{{{-3.168,1.357,1.196},8.820,0.100,1.3,75.0,0.3,1000.0},
		 {{-0.791,1.370,1.286},3.560,1.000,1.0,70.0,1.0,20.0}},
		{{11.55f,0.355f,-2.93f},
		 {-3.975068f,0.001701f,-1.542336f,185709.234375f},
		 {8.41f,3.555f,0.17f},
		 {8.41f,3.555f,0.17f},
		 {8.41f,3.555f,0.17f},
		 {8.41f,3.555f,0.17f},
		 {-0.830257f,0.000000f,0.267133f,185709.234375f}},
		3
	},
        {
                {{{-3.168,1.357,1.196},8.820,0.100,1.3,75.0,0.3,1000.0},
                 {{-0.791,1.370,1.286},3.560,1.000,1.0,70.0,1.0,20.0}},
                {{-1.638089f,0.000000f,0.611004f,185709.234375f} ,
                 {-0.597168f,0.000000f,-2.929566f,185709.234375f} ,
                 {-0.150905f,0.000000f,-0.254472f,185709.234375f} ,
                 {-2.351485f,0.000000f,-3.690775f,185709.234375f} ,
                 {0.505217f,0.000000f,0.722805f,185709.234375f} ,
                 {-1.687062f,0.000000f,0.695321f,185709.234375f} ,
                 {-1.503974f,0.000000f,-0.840125f,185709.234375f}},
                3
        },

        {
                {{{-3.168,1.357,1.196},8.820,0.100,1.3,75.0,0.3,1000.0},
                 {{-0.791,1.370,1.286},1.455,0.100,1.0,70.0,1.0,20.0}},
                {{-1.638089f,0.000000f,0.611004f,186726.687500f} ,
                 {-0.660017f,0.000000f,-3.098260f,186726.687500f} ,
                 {-0.192640f,0.000000f,-0.296598f,186726.687500f} ,
                 {-2.497337f,0.000000f,-3.895486f,186726.687500f} ,
                 {0.494527f,0.000000f,0.726917f,186726.687500f} ,
                 {-1.687062f,0.000000f,0.695321f,186726.687500f} ,
                 {-1.609729f,0.000000f,-0.909960f,186726.687500f}},
                3
        },

        {
                {{{-3.168,1.357,1.196},8.820,0.100,1.3,75.0,0.3,1000.0},
                 {{-0.411,1.340,-4.059},2.050,-1.400,1.0,70.0,1.0,20.0}},
                {{-1.638089f,0.000000f,0.611004f,187348.562500f} ,
                 {-0.671289f,0.000000f,-3.128515f,187348.562500f} ,
                 {-0.200125f,0.000000f,-0.304153f,187348.562500f} ,
                 {-2.523496f,0.000000f,-3.932199f,187348.562500f} ,
                 {0.492609f,0.000000f,0.727655f,187348.562500f} ,
                 {-1.687062f,0.000000f,0.695321f,187348.562500f} ,
                 {-1.628695f,0.000000f,-0.922485f,187348.562500f}},
                3
        },

        {
                {{{-3.168,1.357,1.196},8.820,0.100,1.3,75.0,0.3,1000.0},
                 {{-3.760,1.343,-4.766},3.475,-1.250,1.0,70.0,1.0,20.0}},
                {{-1.638089f,0.000000f,0.611004f,188323.937500f} ,
                 {-0.677192f,0.000000f,-3.144361f,188323.937500f} ,
                 {-0.204045f,0.000000f,-0.308110f,188323.937500f} ,
                 {-2.537197f,0.000000f,-3.951429f,188323.937500f} ,
                 {0.491605f,0.000000f,0.728041f,188323.937500f} ,
                 {-1.687062f,0.000000f,0.695321f,188323.937500f} ,
                 {-1.638628f,0.000000f,-0.929044f,188323.937500f}},
                3
        },
	}
};

class LevelSequence
{
public:
	LevelSequence()
	{
		//insertLevelBack("3ds\\candella\\c-part.3ds");
		//insertLevelBack("3ds\\candella\\c-all.3ds");
		//insertLevelBack("3ds\\candella\\candella.3ds");

		insertLevelBack(&koupelna4);             // ++colorbleed, LIC=OK, 2M
		insertLevelBack(LevelSetup::create("bsp\\x3map\\maps\\x3map05.bsp"));            // ++ext, originalni geometrie levelu, LIC=OK, 7M
		insertLevelBack(LevelSetup::create("bsp\\bgmp\\maps\\bgmp8.bsp"));               // ++pekne skaly a odrazy od zelene travy, int i ext, chybi malinko textur, LIC=OK, 25M/2
		insertLevelBack(LevelSetup::create("bsp\\bgmp\\maps\\bgmp6.bsp"));               // ++pekne drevo, int, chybi malinko textur, LIC=OK, 25M/2
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
	void insertLevelFront(const LevelSetup* level)
	{
		sequence.push_front(level);
		it = sequence.begin();
	}
	void insertLevelBack(const LevelSetup* level)
	{
		sequence.push_back(level);
		it = sequence.begin();
	}
	const LevelSetup* getNextLevel()
	{
		if(it==sequence.end()) return NULL;
		const LevelSetup* result = *it;
		it++;
		if(it==sequence.end()) it=sequence.begin();
		return result;
	}
private:
	std::list<const LevelSetup*> sequence;
	std::list<const LevelSetup*>::iterator it;
};

#endif
