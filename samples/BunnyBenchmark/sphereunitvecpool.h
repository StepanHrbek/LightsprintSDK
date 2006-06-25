/*=====================================================================
sphereunitvecpool.h
-------------------
File created by ClassTemplate on Mon Nov 22 02:42:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SPHEREUNITVECPOOL_H_666_
#define __SPHEREUNITVECPOOL_H_666_


#include <vector>
class rand48_t;


//bare-bones 3-vector class for storing and returning results.
class PoolVec3
{
public:
	PoolVec3(){}
	PoolVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	void operator +=(const PoolVec3& a) {x+=a.x;y+=a.y;z+=a.z;}
	void operator *=(float a) {x*=a;y*=a;z*=a;}
	PoolVec3 operator -(PoolVec3& a) {return PoolVec3(x-a.x,y-a.y,z-a.z);}

	float x,y,z;
};


/*=====================================================================
SphereUnitVecPool
-----------------
A pool for fast generation of random unit length vectors uniformly distributed
over the unit ball (surface of unit sphere).

This code is for the ray/tri-mesh benchmark:
http://homepages.paradise.net.nz/nickamy/benchmark.html

Feel free to use for any purpose, subject to the license for the included
random number generator.
=====================================================================*/
class SphereUnitVecPool
{
public:
	/*=====================================================================
	SphereUnitVecPool
	-----------------
	
	=====================================================================*/
	SphereUnitVecPool(int poolsize = 10031);

	~SphereUnitVecPool();


	//return a randomly chosen 3-vector from the pool.
	const PoolVec3 getVec();



private:
	const PoolVec3 randomVec();
	float randomUnit();

	std::vector<PoolVec3> vecs;

	rand48_t* rand_num_gen;
};



#endif //__SPHEREUNITVECPOOL_H_666_




