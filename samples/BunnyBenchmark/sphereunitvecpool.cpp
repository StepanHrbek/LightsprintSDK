/*=====================================================================
sphereunitvecpool.cpp
---------------------
File created by ClassTemplate on Mon Nov 22 02:42:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "sphereunitvecpool.h"


#include <math.h>


/* note: slightly doctored version from the GNU Sci. Lib. (GSL).

   Copyright (C) 1996, 1997, 1998, 1999, 2000 James Theiler, Brian Gough

   This is the Unix rand48() generator. The generator returns the
   upper 32 bits from each term of the sequence,

   x_{n+1} = (a x_n + c) mod m 

   using 48-bit unsigned arithmetic, with a = 0x5DEECE66D , c = 0xB
   and m = 2^48. The seed specifies the upper 32 bits of the initial
   value, x_1, with the lower 16 bits set to 0x330E.

   The theoretical value of x_{10001} is 244131582646046.

   The period of this generator is ? FIXME (probably around 2^48). */


class rand48_t {
//	static const unsigned short int a0 = 0xE66D, a1 = 0xDEEC, a2 = 0x0005, c0 = 0x000B;
	static const unsigned short int a0, a1, a2, c0;

	struct rand48_state_t { unsigned short int x0, x1, x2; } vstate;

	void advance() {
		/* work with unsigned long ints throughout to get correct integer
			promotions of any unsigned short ints */
		const unsigned long int x0 = (unsigned long int) vstate.x0;
		const unsigned long int x1 = (unsigned long int) vstate.x1;
		const unsigned long int x2 = (unsigned long int) vstate.x2;

		unsigned long int a ;
		a = a0 * x0 + c0 ;
		vstate.x0 = (unsigned short) (a & 0xFFFF) ;
		a >>= 16 ;

		/* although the next line may overflow we only need the top 16 bits
		in the following stage, so it does not matter */
		a += a0 * x1 + a1 * x0 ; 
		vstate.x1 = (unsigned short) (a & 0xFFFF) ;
		a >>= 16 ;
		a += a0 * x2 + a1 * x1 + a2 * x0 ;
		vstate.x2 = (unsigned short) (a & 0xFFFF) ;
	}

public:
	rand48_t(unsigned long int s = 0) { seed(s); }

	// max 0xffffffffUL, min 0
	//double get_rand_max() const { return double(0xffffffffUL); }

	void seed(unsigned long int s) {
		if (s == 0) { /* default seed */
			vstate.x0 = 0x330E ;
			vstate.x1 = 0xABCD ;
			vstate.x2 = 0x1234 ;
		}
		else  {
			vstate.x0 = 0x330E ;
			vstate.x1 = (unsigned short) (s & 0xFFFF) ;
			vstate.x2 = (unsigned short) ((s >> 16) & 0xFFFF) ;
		}
		return;
	}

	//get a random unsigned integer
	unsigned long int get_integer() {
		unsigned long int x1, x2;
		advance() ;

		x2 = (unsigned long int) vstate.x2;
		x1 = (unsigned long int) vstate.x1;

		return (x2 << 16) + x1;
	}

	// redefines the notion of slowlyness.
	double get_double() {
		advance();

		return (ldexp((double) vstate.x2, -16)
			+ ldexp((double) vstate.x1, -32) 
			+ ldexp((double) vstate.x0, -48)) ;
	}

	// let's cut some corners, should be good enough for 32 bits.
	float get_unit_float() 
	{ 
		return (float)get_integer() / (float)0xffffffffUL; 
	}

};// rand48;

const unsigned short int rand48_t::a0 = 0xE66D; 
const unsigned short int rand48_t::a1 = 0xDEEC; 
const unsigned short int rand48_t::a2 = 0x0005;
const unsigned short int rand48_t::c0 = 0x000B;



SphereUnitVecPool::SphereUnitVecPool(int poolsize)
{
	rand_num_gen = new rand48_t;

	vecs.resize(poolsize);

	for (int i=0; i<poolsize; ++i)
		vecs[i] = randomVec();

}


SphereUnitVecPool::~SphereUnitVecPool()
{
	delete rand_num_gen;
}

const PoolVec3 SphereUnitVecPool::getVec()
{
	const int index = rand_num_gen->get_integer() % vecs.size();
	return vecs[index];
}


const PoolVec3 SphereUnitVecPool::randomVec()
{
	while (1)
	{
		//generate random vector in unit cube
		PoolVec3 vec(-1.0f + randomUnit() * 2.0f, -1.0f + randomUnit() * 2.0f, -1.0f + randomUnit() * 2.0f);

		const float length_sqrd = vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;

		if (length_sqrd <= 1.0f)
		{
			//return normalised vector
			const float len = sqrt(length_sqrd);
			return PoolVec3(vec.x / len, vec.y / len, vec.z / len);
		}
	}

}


float SphereUnitVecPool::randomUnit()
{
	return rand_num_gen->get_unit_float();
}

