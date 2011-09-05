// --------------------------------------------------------------------------
// Basic math functions.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRMath.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RGB <-> HSV

RRVec3 RRVec3::getHsvFromRgb() const
{
	RRVec3 rgb = *this;
	float v = rgb.maxi();
	if (v==0)
		return RRVec3(0);
	rgb /= v;
	float rgbmin = rgb.mini();
	float rgbmax = rgb.maxi();
	float s = rgbmax-rgbmin;
	if (!s)
		return RRVec3(0,0,v);
	rgb = (rgb-RRVec3(rgbmin))/(rgbmax-rgbmin);
	rgbmin = rgb.mini();
	rgbmax = rgb.maxi();
	float h;
	if (rgbmax==rgb[0])
	{
		h = 60*(rgb[1]-rgb[2]);
		if (h<0) h+= 360;
	}
	else
	if (rgbmax==rgb[1])
	{
		h = 120+60*(rgb[2]-rgb[0]);
	}
	else
	{
		h = 240+60*(rgb[0]-rgb[1]);
	}
	return RRVec3(h,s,v);
}

RRVec3 RRVec3::getRgbFromHsv() const
{
	RRVec3 hsv = *this;
	float h = hsv[0];
	float s = RR_CLAMPED(hsv[1],0,1);
	float v = hsv[2];
	h = fmod(h,360);
	if (h<0) h+= 360;
	h /= 60;
	float i = floor(h);
	float f = h - i;
	float p1 = v * (1 - s);
	float p2 = v * (1 - (s * f));
	float p3 = v * (1 - (s * (1 - f)));
	switch ((int) i)
	{
		case 0: return RRVec3(v,p3,p1);
		case 1: return RRVec3(p2,v,p1);
		case 2: return RRVec3(p1,v,p3);
		case 3: return RRVec3(p1,p2,v);
		case 4: return RRVec3(p3,p1,v);
		case 5: return RRVec3(v,p1,p2);
	}
	RR_ASSERT(0);
	return RRVec3(0);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4

RRVec3 RRMatrix3x4::transformedPosition(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
	  a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2] + (m)[0][3],
	  a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2] + (m)[1][3],
	  a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2] + (m)[2][3]);
}

RRVec3 RRMatrix3x4::transformedDirection(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
		a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2],
		a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2],
		a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2]);
}

RRVec3& RRMatrix3x4::transformPosition(RRVec3& a) const
{
	RR_ASSERT(m);
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2] + (m)[0][3];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2] + (m)[1][3];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2] + (m)[2][3];

	return a;
}

RRVec3& RRMatrix3x4::transformDirection(RRVec3& a) const
{
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2];

	return a;
}

RRMatrix3x4 RRMatrix3x4::operator *(const RRMatrix3x4& b) const
{
	RRMatrix3x4 c;
	for (unsigned j=0;j<3;j++)
	{
		for (unsigned i=0;i<4;i++)
			c.m[j][i] = m[j][0]*b.m[0][i]+m[j][1]*b.m[1][i]+m[j][2]*b.m[2][i]+((i==3)?m[j][3]:0);
	}
	return c;
}

RRMatrix3x4& RRMatrix3x4::operator *=(const RRMatrix3x4& b)
{
	return *this = *this * b;
}

RRReal RRMatrix3x4::determinant3x3() const
{
	return m[0][0]*(m[1][1]*m[2][2]-m[2][1]*m[1][2]) + m[0][1]*(m[1][2]*m[2][0]-m[2][2]*m[1][0]) + m[0][2]*(m[1][0]*m[2][1]-m[2][0]*m[1][1]);
}


bool RRMatrix3x4::invertedTo(RRMatrix3x4& destination) const
{
	float det = determinant3x3();
	if (!det) return false;
	destination.m[0][0] = +(m[2][2]*m[1][1]-m[2][1]*m[1][2])/det;
	destination.m[0][1] = -(m[2][2]*m[0][1]-m[2][1]*m[0][2])/det;
	destination.m[0][2] = +(m[1][2]*m[0][1]-m[1][1]*m[0][2])/det;
	destination.m[1][0] = -(m[2][2]*m[1][0]-m[2][0]*m[1][2])/det;
	destination.m[1][1] = +(m[2][2]*m[0][0]-m[2][0]*m[0][2])/det;
	destination.m[1][2] = -(m[1][2]*m[0][0]-m[1][0]*m[0][2])/det;
	destination.m[2][0] = +(m[2][1]*m[1][0]-m[2][0]*m[1][1])/det;
	destination.m[2][1] = -(m[2][1]*m[0][0]-m[2][0]*m[0][1])/det;
	destination.m[2][2] = +(m[1][1]*m[0][0]-m[1][0]*m[0][1])/det;
	destination.m[0][3] = -m[0][3]*destination.m[0][0]-m[1][3]*destination.m[0][1]-m[2][3]*destination.m[0][2];
	destination.m[1][3] = -m[0][3]*destination.m[1][0]-m[1][3]*destination.m[1][1]-m[2][3]*destination.m[1][2];
	destination.m[2][3] = -m[0][3]*destination.m[2][0]-m[1][3]*destination.m[2][1]-m[2][3]*destination.m[2][2];
	return true;
}

void RRMatrix3x4::setIdentity()
{
	m[0][0] = 1;
	m[0][1] = 0;
	m[0][2] = 0;
	m[0][3] = 0;
	m[1][0] = 0;
	m[1][1] = 1;
	m[1][2] = 0;
	m[1][3] = 0;
	m[2][0] = 0;
	m[2][1] = 0;
	m[2][2] = 1;
	m[2][3] = 0;
}

bool RRMatrix3x4::isIdentity() const
{
	return 
		m[0][0] == 1 &&
		m[0][1] == 0 &&
		m[0][2] == 0 &&
		m[0][3] == 0 &&
		m[1][0] == 0 &&
		m[1][1] == 1 &&
		m[1][2] == 0 &&
		m[1][3] == 0 &&
		m[2][0] == 0 &&
		m[2][1] == 0 &&
		m[2][2] == 1 &&
		m[2][3] == 0;
}

RRVec3 RRMatrix3x4::getTranslation() const
{
	return RRVec3(m[0][3],m[1][3],m[2][3]);
}

void RRMatrix3x4::setTranslation(const RRVec3& a)
{
	m[0][3] = a[0];
	m[1][3] = a[1];
	m[2][3] = a[2];
}

void RRMatrix3x4::translate(const RRVec3& a)
{
	m[0][3] += a[0];
	m[1][3] += a[1];
	m[2][3] += a[2];
}

RRVec3 RRMatrix3x4::getScale() const
{
	RRVec3 scale;
	for (unsigned i=0;i<3;i++)
	{
		RRReal sign = m[i][0]+m[i][1]+m[i][2];
		RRReal value = fabs(m[i][0])+fabs(m[i][1])+fabs(m[i][2]);
		scale[i] = sign>=0?value:-value;
	}
	return scale;
}

void RRMatrix3x4::setScale(const RRVec3& newScale)
{
	RRVec3 oldScale = getScale();
	for (unsigned i=0;i<3;i++)
	{
		if (_finite(newScale[i])) // work only if inputs are valid
		{
			if (_finite(newScale[i]/oldScale[i]))
			{
				// standard scaling
				for (unsigned j=0;j<3;j++)
					m[i][j] = m[i][j]/oldScale[i]*newScale[i];
			}
			else
			{
				// matrix reconstruction after previous setScale(0)
				for (unsigned j=0;j<3;j++)
					m[i][j] = (i==j)?newScale[i]:0;
			}
		}
	}
}



} // namespace
