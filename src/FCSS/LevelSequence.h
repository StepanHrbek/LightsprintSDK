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
   {{{-3.266,1.236,1.230},9.120,0.100,1.3,45.0,0.3,1000.0},
    {{-0.791,1.370,1.286},3.475,0.550,1.0,70.0,1.0,20.0}},
   {{-4.176178f,0.001628f,-0.472815f,-4.379654f} ,
    {-3.208725f,0.000242f,-1.263477f,4.376968f} ,
    {-0.387714f,0.000000f,-1.316818f,-4.379654f} ,
    {-2.689580f,0.000000f,-4.894086f,4.376968f} ,
    {0.299290f,0.116022f,-0.293544f,-4.379654f} ,
    {-3.603322f,3.005856f,-0.282817f,4.376968f} ,
    {-1.964720f,0.000000f,-0.639731f,-4.379654f}},
   8
  },
  {
   {{{-2.959,1.325,2.558},8.625,0.100,1.3,55.0,0.3,1000.0},
    {{0.141,1.385,1.246},1.575,-0.650,1.0,70.0,1.0,20.0}},
   {{-1.293823f,0.000000f,-0.938927f,-4.379654f} ,
    {-0.988367f,0.000000f,0.552224f,4.376968f} ,
    {-0.387714f,0.000000f,-1.316818f,-4.379654f} ,
    {-2.689580f,0.000000f,-4.894086f,4.376968f} ,
    {0.299290f,0.116022f,-0.293544f,-4.379654f} ,
    {-3.603322f,3.005856f,-0.282817f,4.376968f} ,
    {-2.247848f,0.000000f,1.488840f,-4.379654f}},
   8
  },
  {
   {{{-2.686,1.430,2.799},8.865,0.250,1.3,45.0,0.3,1000.0},
    {{-1.048,1.764,2.429},-0.715,3.200,1.0,70.0,1.0,20.0}},
   {{-1.293823f,0.000000f,-0.938927f,-4.379654f} ,
    {-0.988367f,0.000000f,0.552224f,4.376968f} ,
    {-0.387714f,0.000000f,-1.316818f,-4.379654f} ,
    {-2.689580f,0.000000f,-4.894086f,4.376968f} ,
    {0.299290f,0.116022f,-0.293544f,-4.379654f} ,
    {-3.603322f,3.005856f,-0.282817f,4.376968f} ,
    {-2.247848f,0.000000f,1.488840f,-4.379654f}},
   8
  },
  {
   {{{-3.139,1.235,-1.917},9.100,-0.350,1.3,65.0,0.3,1000.0},
    {{-3.743,1.427,-4.326},-2.630,-0.250,1.0,70.0,1.0,20.0}},
   {{-1.293823f,0.000000f,-0.938927f,-4.379654f} ,
    {-1.395688f,0.000000f,-4.718618f,4.376968f} ,
    {-0.387714f,0.000000f,-1.316818f,-4.379654f} ,
    {-2.689580f,0.000000f,-4.894086f,4.376968f} ,
    {0.299290f,0.116022f,-0.293544f,-4.379654f} ,
    {-3.603322f,3.005856f,-0.282817f,4.376968f} ,
    {-2.322678f,0.000000f,-2.401792f,-4.379654f}},
   8
  },
  {
   {{{-0.812,1.382,2.550},9.450,0.100,1.3,15.0,0.3,1000.0},
    {{-0.503,1.493,-4.677},-4.535,-0.700,1.0,70.0,1.0,20.0}},
   {{-1.293823f,0.000000f,-0.938927f,-4.379654f} ,
    {-1.395688f,0.000000f,-4.718618f,4.376968f} ,
    {-0.387714f,0.000000f,-1.316818f,-4.379654f} ,
    {-2.689580f,0.000000f,-4.894086f,4.376968f} ,
    {0.299290f,0.116022f,-0.293544f,-4.379654f} ,
    {-3.603322f,3.005856f,-0.282817f,4.376968f} ,
    {-1.452301f,0.000000f,-2.222887f,-4.379654f}},
   8
  },
 }
};

LevelSetup x3map05 = 
{
 "bsp\\x3map\\maps\\x3map05.bsp",
 1,
 {
  {// 2, vsichni v kulaty dire mimo hlavni mapu, kamera nahore
   {{{9.764,13.828,-10.039},14.305,6.700,1.3,35.0,0.3,1000.0},
    {{13.473,10.496,-7.604},8.400,6.650,1.0,70.0,1.0,100.0}},
   {{20.054729f,3.840000f,-16.970427f,-1.354263f} ,
    {21.149405f,3.840000f,-8.506203f,1.351334f} ,
    {25.526459f,3.840000f,-11.034856f,-1.354263f} ,
    {14.969198f,8.639999f,-8.807755f,1.351334f} ,
    {21.294151f,3.840000f,-15.836219f,-1.354263f} ,
    {24.750158f,3.840000f,-14.088777f,1.351334f} ,
    {27.442495f,3.840000f,-12.915664f,-1.354263f}},
   8
  },
  {// 1, robot+stin sochy v kulaty dire mimo hlavni mapu, kamera dole
   {{{27.067,5.236,-10.088},15.930,0.700,1.3,35.0,0.3,1000.0},
    {{13.473,10.496,-7.604},8.400,6.650,1.0,70.0,1.0,100.0}},
   {{20.054729f,3.840000f,-16.970427f,-1.354263f} ,
    {21.149405f,3.840000f,-8.506203f,1.351334f} ,
    {25.526459f,3.840000f,-11.034856f,-1.354263f} ,
    {14.969198f,8.639999f,-8.807755f,1.351334f} ,
    {21.294151f,3.840000f,-15.836219f,-1.354263f} ,
    {24.750158f,3.840000f,-14.088777f,1.351334f} ,
    {27.442495f,3.840000f,-12.915664f,-1.354263f}},
   8
  },
  {// 3, logo lightsprint promitle na stenu
   {{{58.916,12.632,-38.893},17.605,3.250,1.3,35.0,0.3,1000.0},
    {{54.500,16.312,-25.990},10.710,6.250,1.0,70.0,1.0,100.0}},
   {{31.574196f,8.160000f,-24.616028f,-0.694992f} ,
    {41.050110f,0.000000f,-31.627964f,0.692062f} ,
    {38.590824f,8.160000f,-38.461494f,-0.694992f} ,
    {53.621872f,8.639999f,-38.059147f,0.692062f} ,
    {40.924988f,3.840000f,-30.724232f,-0.694992f} ,
    {40.106480f,3.840000f,-32.842548f,0.692062f} ,
    {35.806252f,8.160000f,-27.978878f,-0.694992f}},
   8
  },
  /*{ // 3, vsichni kolem mustku dole v pulce mapy
   {{{-0.945,1.492,-36.258},19.770,2.800,1.3,35.0,0.3,1000.0},
    {{19.370,4.761,-31.931},11.550,2.650,1.0,70.0,1.0,100.0}},
   {{8.848922f,-3.840000f,-34.549301f,-0.555550f} ,
    {10.617924f,0.000000f,-28.889473f,0.553596f} ,
    {2.643005f,-3.840000f,-33.317139f,-0.555550f} ,
    {8.654413f,0.000000f,-31.310789f,0.553596f} ,
    {4.598825f,-3.840000f,-30.574036f,-0.555550f} ,
    {6.418550f,-3.840000f,-29.302879f,0.553596f} ,
    {7.281435f,-3.840000f,-33.028687f,-0.555550f}},
   8
  },*/
  { // 2, svetlo v tunelu vzadu v cervene
   {{{4.989,1.276,-31.442},17.975,1.450,1.3,35.0,0.3,1000.0},
    {{0.513,-2.574,-25.880},11.650,1.900,1.0,70.0,1.0,100.0}},
   {{-2.485293f,-3.840000f,-25.285263f,-2.879768f} ,
    {-1.112324f,-3.840000f,-22.054396f,2.876839f} ,
    {-5.328533f,-3.840000f,-23.847364f,-2.879768f} ,
    {1.721917f,0.000000f,-30.031544f,2.876839f} ,
    {-4.078298f,0.000000f,-22.820820f,-2.879768f} ,
    {-3.135963f,-3.840000f,-22.395298f,2.876839f} ,
    {-0.334877f,-3.840000f,-24.657183f,-2.879768f}},
   8
  },
  {// 3, nejvyssi mustek, pohled na celou cervenou pulku mapy
   {{{6.996,6.021,-28.439},17.145,2.200,1.3,85.0,0.3,1000.0},
    {{19.093,4.761,-32.596},11.390,3.700,1.0,70.0,1.0,100.0}},
   {{-8.444609f,0.000000f,-28.241175f,-4.220192f} ,
    {-0.304005f,-3.840000f,-25.906715f,4.217262f} ,
    {0.735101f,0.000000f,-29.941442f,-4.220192f} ,
    {-4.659645f,0.000000f,-22.482759f,4.217262f} ,
    {4.598825f,-3.840000f,-30.574036f,-4.220192f} ,
    {-3.816134f,3.840000f,-35.232635f,4.217262f} ,
    {5.929975f,4.490469f,-29.656843f,-4.220192f}},
   8
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
		insertLevelBack(&x3map05);               // ++ext, originalni geometrie levelu, LIC=OK, 7M
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
