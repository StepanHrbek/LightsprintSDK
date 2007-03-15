#ifndef LEVELSEQUENCE_H
#define LEVELSEQUENCE_H

#include <list>
#include "Autopilot.h"

LevelSetup koupelna4 =
{
 "3ds\\koupelna\\koupelna4.3ds",
 0.03f,
 {
  /*{screenshoty s porovnanim hard/soft/penumbra
   {{{-4.710,0.868,0.373},2.657+2*3.14159f,0.100,1.3,45.0,0.3,1000.0},
    {{-1.650,1.075,-0.317},3.745,2.200,1.0,70.0,1.0,20.0}},
   {{-4.554447f,0.000843f,-0.327654f,-0.331139f} ,
    {-1.889560f,0.000000f,-3.365015f,0.331139f} ,
    {-0.716842f,0.000000f,-1.187382f,-0.331139f} ,
    {-3.029553f,0.000000f,-4.786990f,0.331139f} ,
    {-0.026819f,0.338692f,-0.159613f,-0.331139f} ,
    {-3.946579f,3.005856f,-0.148839f,0.331139f} ,
    {-2.973941f,0.000000f,-1.097908f,-0.331139f}},
   8
  },*/
  { // prime svetlo na postavy
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
  { // cervena
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

// Triangulation
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

// Mortal Wounds
LevelSetup bgmp8 =
{
 "bsp\\bgmp\\maps\\bgmp8.bsp",
 1,
 {
  { // 3, v chodbe
   {{{12.031,8.272,-6.493},9.360-6.2831853f,0.700,1.3,45.0,0.3,1000.0},
    {{11.857,8.618,-8.386},2.585,2.450,1.0,70.0,1.0,100.0}},
   {{12.028995f,6.600000f,-14.647240f,-0.271744f} ,
    {11.250498f,6.600000f,-9.611723f,0.268814f} ,
    {14.798590f,6.774337f,-27.702135f,-0.271744f} ,
    {11.142448f,6.600000f,-13.550617f,0.268814f} ,
    {12.917421f,6.600000f,-8.886457f,-0.271744f} ,
    {15.019566f,6.600000f,-22.939419f,0.268814f} ,
    {13.006920f,6.600000f,-17.979858f,-0.271744f}},
   8
  },
  { // 3, ve velke jeskyni
   {{{26.849,5.241,-22.568},-4.625+6.2831853f,0.850,1.3,95.0,0.3,1000.0},
    {{30.429,7.401,-14.609},3.275,0.350,1.0,70.0,1.0,100.0}},
   {{27.804386f,3.840000f,-22.663649f,-2.532074f} ,
    {28.983326f,4.320000f,-24.296406f,2.528168f} ,
    {31.012312f,3.840000f,-22.271511f,-2.532074f} ,
    {33.645584f,5.400000f,-25.791416f,2.528168f} ,
    {30.490063f,3.840000f,-21.539434f,-2.532074f} ,
    {31.045425f,3.840000f,-20.534435f,2.528168f} ,
    {32.372364f,3.840000f,-16.792316f,-2.532074f}},
   8
  },
  { // dole v rokli, odraz od zelene travy
   {{{24.680,4.916,-35.354},2.790,-0.100,1.3,75.0,0.3,1000.0},
    {{27.356,6.453,-41.578},4.990,13.000,1.0,70.0,1.0,100.0}},
   {{32.108135f,3.840000f,-43.793198f,-3.117790f} ,
    {26.802246f,3.840000f,-39.846256f,3.115837f} ,
    {32.774014f,3.840000f,-46.012383f,-3.117790f} ,
    {24.939678f,3.840000f,-40.654442f,3.115837f} ,
    {27.111662f,3.840000f,-46.452461f,-3.117790f} ,
    {25.978649f,3.840000f,-36.091148f,3.115837f} ,
    {30.214142f,3.840000f,-40.532864f,-3.117790f}},
   8
  },
  { // nahore na mustku
   {{{27.320,12.388,-47.417},-5.810+6.2831853f,2.350,1.3,85.0,0.3,1000.0},
    {{20.794,14.483,-43.869},1.697,5.200,1.0,70.0,1.0,100.0}},
   {{37.107140f,6.720000f,-44.976089f,-5.358192f} ,
    {27.667862f,10.559999f,-43.542294f,5.356239f} ,
    {33.976505f,3.840000f,-45.121532f,-5.358192f} ,
    {25.725399f,9.303043f,-32.357391f,5.356239f} ,
    {34.075291f,3.840000f,-42.491749f,-5.358192f} ,
    {25.665192f,3.840000f,-40.024876f,5.356239f} ,
    {31.826147f,3.840000f,-41.866234f,-5.358192f}},
   8
  },
  { // pohled do polochodby kolem rokle
   {{{36.192,7.876,-45.090},6.258,0.950,1.3,75.0,0.3,1000.0},
    {{27.356,6.453,-41.578},0.562,-4.550,1.0,70.0,1.0,100.0}},
   {{26.403290f,3.840000f,-35.195183f,-5.442795f} ,
    {29.946533f,8.016913f,-32.484879f,5.440353f} ,
    {31.316612f,7.599471f,-32.138241f,-5.442795f} ,
    {38.010372f,6.720000f,-33.055439f,5.440353f} ,
    {36.885181f,6.720000f,-44.409294f,-5.442795f} ,
    {37.959641f,6.720000f,-40.010818f,5.440353f} ,
    {37.357349f,6.720000f,-47.347706f,-5.442795f}},
   8
  },
 }
};

// The Soremill
LevelSetup bgmp6 =
{
 "bsp\\bgmp\\maps\\bgmp6.bsp",
 1,
 {
  { // muzi nasviceni pred plakatem, zeny ve tme pred kamerou
   {{{-10.683,8.997,2.685},1.379,1.000,1.3,45.0,0.3,1000.0},
    {{7.570,17.921,-4.285},6.083,7.900,1.0,70.0,1.0,100.0}},
   {{-2.819999f,6.600000f,5.884839f,-2.702072f} ,
    {4.204948f,5.760000f,2.153754f,2.699142f} ,
    {-1.029008f,6.960000f,5.678192f,-2.702072f} ,
    {-6.610936f,5.760000f,5.479778f,2.699142f} ,
    {-0.577775f,6.960000f,3.120854f,-2.702072f} ,
    {-0.706023f,6.960000f,4.341853f,2.699142f} ,
    {-9.199325f,8.222093f,2.479325f,-2.702072f}},
   8
  },
  { // na brevnech nad zapustenou chodbou
   {{{-5.548,8.925,-1.371},5.289,3.550,1.3,45.0,0.3,1000.0},
    {{-7.941,10.874,-2.230},5.602,8.500,1.0,70.0,1.0,100.0}},
   {{-14.670853f,5.760000f,6.338285f,-4.356453f} ,
    {-16.446150f,5.760000f,3.516719f,4.353798f} ,
    {-14.815092f,0.000000f,3.954745f,-4.356453f} ,
    {-8.419514f,5.760000f,3.269313f,4.353798f} ,
    {-10.052445f,5.760000f,-0.980157f,-4.356453f} ,
    {-11.675663f,5.760000f,-0.054854f,4.353798f} ,
    {-8.948483f,8.400000f,2.632319f,-4.356453f}},
   8
  },
  { // pohled z dalku na osviceny plakat
   {{{8.336,7.393,-6.206},6.027,1.900,1.3,55.0,0.3,1000.0},
    {{7.570,17.921,-4.285},6.003,13.000,1.0,70.0,1.0,100.0}},
   {{5.276146f,0.480000f,0.692151f,-1.288681f} ,
    {5.833324f,5.760000f,1.474134f,1.286057f} ,
    {0.215287f,6.960000f,5.296309f,-1.288681f} ,
    {9.012029f,5.760000f,6.339234f,1.286057f} ,
    {7.003012f,5.760000f,0.192239f,-1.288681f} ,
    {9.078769f,5.760000f,-3.581309f,1.286057f} ,
    {1.949829f,5.760000f,0.330906f,-1.288681f}},
   8
  },
  { // skupinka se stiny v zapustene chodbe
   {{{4.476,1.189,-0.362},4.114,0.100,1.3,55.0,0.3,1000.0},
    {{8.200,2.979,-3.694},4.785,1.150,1.0,70.0,1.0,100.0}},
   {{2.518140f,0.000000f,-4.231451f,-4.974480f} ,
    {0.509009f,0.000000f,-6.399400f,4.972038f} ,
    {-7.620471f,0.000000f,-1.351727f,-4.974480f} ,
    {-3.896290f,0.000000f,-2.519807f,4.972038f} ,
    {-3.840000f,0.720000f,-5.049809f,-4.974480f} ,
    {1.339654f,0.000000f,-0.836588f,4.972038f} ,
    {0.491399f,0.000000f,-2.730603f,-4.974480f}},
   8
  },
  { // skupinka se stiny v jedne zapustene mistnosti bokem
   {{{7.807,7.139,-21.023},6.127,1.450,1.3,45.0,0.3,1000.0},
    {{-3.083,9.333,-11.283},1.498,4.150,1.0,70.0,1.0,100.0}},
   {{6.241113f,5.760000f,-9.137688f,-4.802826f} ,
    {3.127544f,5.760000f,-9.986009f,4.800873f} ,
    {1.725544f,1.920000f,-13.040113f,-4.802826f} ,
    {8.310306f,5.760000f,-19.162029f,4.800873f} ,
    {3.214567f,2.880000f,-11.418232f,-4.802826f} ,
    {6.765731f,1.920000f,-12.086202f,4.800873f} ,
    {1.064710f,5.760000f,-8.997874f,-4.802826f}},
   8
  },
 }
};

class LevelSequence
{
public:
	LevelSequence()
	{
		//insertLevelBack(LevelSetup::create("3ds\\candella\\seen mesh.3ds"));
		//insertLevelBack("3ds\\candella\\c-part.3ds");
		//insertLevelBack("3ds\\candella\\c-all.3ds");
		//insertLevelBack("3ds\\candella\\candella.3ds");

		//insertLevelBack(LevelSetup::create("collada\\cube.dae"));

		insertLevelBack(&koupelna4);  // ++colorbleed, LIC=OK, 2M
		insertLevelBack(&x3map05);    // ++ext, originalni geometrie levelu, LIC=OK, 7M
		insertLevelBack(&bgmp8);      // ++pekne skaly a odrazy od zelene travy, int i ext, chybi malinko textur, LIC=OK, 25M/2
		insertLevelBack(&bgmp6);      // ++pekne drevo, int, chybi malinko textur, LIC=OK, 25M/2
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
