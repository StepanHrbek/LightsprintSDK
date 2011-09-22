// --------------------------------------------------------------------------
// Basic math functions.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRMath.h"
#include <cmath>

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
// RRMatrix3x4 ctors

RRMatrix3x4::RRMatrix3x4()
{
}

RRMatrix3x4::RRMatrix3x4(float* m3x4, bool transposed)
{
	if (transposed)
	{
		m[0][0] = (RRReal)m3x4[0];
		m[0][1] = (RRReal)m3x4[4];
		m[0][2] = (RRReal)m3x4[8];
		m[0][3] = (RRReal)m3x4[12];
		m[1][0] = (RRReal)m3x4[1];
		m[1][1] = (RRReal)m3x4[5];
		m[1][2] = (RRReal)m3x4[9];
		m[1][3] = (RRReal)m3x4[13];
		m[2][0] = (RRReal)m3x4[2];
		m[2][1] = (RRReal)m3x4[6];
		m[2][2] = (RRReal)m3x4[10];
		m[2][3] = (RRReal)m3x4[14];
	}
	else
	{
		m[0][0] = (RRReal)m3x4[0];
		m[0][1] = (RRReal)m3x4[1];
		m[0][2] = (RRReal)m3x4[2];
		m[0][3] = (RRReal)m3x4[3];
		m[1][0] = (RRReal)m3x4[4];
		m[1][1] = (RRReal)m3x4[5];
		m[1][2] = (RRReal)m3x4[6];
		m[1][3] = (RRReal)m3x4[7];
		m[2][0] = (RRReal)m3x4[8];
		m[2][1] = (RRReal)m3x4[9];
		m[2][2] = (RRReal)m3x4[10];
		m[2][3] = (RRReal)m3x4[11];
	}
}

RRMatrix3x4::RRMatrix3x4(double* m3x4, bool transposed)
{
	if (transposed)
	{
		m[0][0] = (RRReal)m3x4[0];
		m[0][1] = (RRReal)m3x4[4];
		m[0][2] = (RRReal)m3x4[8];
		m[0][3] = (RRReal)m3x4[12];
		m[1][0] = (RRReal)m3x4[1];
		m[1][1] = (RRReal)m3x4[5];
		m[1][2] = (RRReal)m3x4[9];
		m[1][3] = (RRReal)m3x4[13];
		m[2][0] = (RRReal)m3x4[2];
		m[2][1] = (RRReal)m3x4[6];
		m[2][2] = (RRReal)m3x4[10];
		m[2][3] = (RRReal)m3x4[14];
	}
	else
	{
		m[0][0] = (RRReal)m3x4[0];
		m[0][1] = (RRReal)m3x4[1];
		m[0][2] = (RRReal)m3x4[2];
		m[0][3] = (RRReal)m3x4[3];
		m[1][0] = (RRReal)m3x4[4];
		m[1][1] = (RRReal)m3x4[5];
		m[1][2] = (RRReal)m3x4[6];
		m[1][3] = (RRReal)m3x4[7];
		m[2][0] = (RRReal)m3x4[8];
		m[2][1] = (RRReal)m3x4[9];
		m[2][2] = (RRReal)m3x4[10];
		m[2][3] = (RRReal)m3x4[11];
	}
}

RRMatrix3x4::RRMatrix3x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23)
{
	m[0][0] = (RRReal)m00;
	m[0][1] = (RRReal)m01;
	m[0][2] = (RRReal)m02;
	m[0][3] = (RRReal)m03;
	m[1][0] = (RRReal)m10;
	m[1][1] = (RRReal)m11;
	m[1][2] = (RRReal)m12;
	m[1][3] = (RRReal)m13;
	m[2][0] = (RRReal)m20;
	m[2][1] = (RRReal)m21;
	m[2][2] = (RRReal)m22;
	m[2][3] = (RRReal)m23;
}

RRMatrix3x4::RRMatrix3x4(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23)
{
	m[0][0] = (RRReal)m00;
	m[0][1] = (RRReal)m01;
	m[0][2] = (RRReal)m02;
	m[0][3] = (RRReal)m03;
	m[1][0] = (RRReal)m10;
	m[1][1] = (RRReal)m11;
	m[1][2] = (RRReal)m12;
	m[1][3] = (RRReal)m13;
	m[2][0] = (RRReal)m20;
	m[2][1] = (RRReal)m21;
	m[2][2] = (RRReal)m22;
	m[2][3] = (RRReal)m23;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 point transformations

RRVec3 RRMatrix3x4::getTransformedPosition(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
	  a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2] + (m)[0][3],
	  a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2] + (m)[1][3],
	  a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2] + (m)[2][3]);
}

RRVec3 RRMatrix3x4::getTransformedDirection(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
		a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2],
		a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2],
		a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2]);
}

void RRMatrix3x4::transformPosition(RRVec3& a) const
{
	RR_ASSERT(m);
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2] + (m)[0][3];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2] + (m)[1][3];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2] + (m)[2][3];
}

void RRMatrix3x4::transformDirection(RRVec3& a) const
{
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2];
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 operators

bool RRMatrix3x4::operator ==(const RRMatrix3x4& a) const
{
	return 
		m[0][0] == a.m[0][0] &&
		m[0][1] == a.m[0][1] &&
		m[0][2] == a.m[0][2] &&
		m[0][3] == a.m[0][3] &&
		m[1][0] == a.m[1][0] &&
		m[1][1] == a.m[1][1] &&
		m[1][2] == a.m[1][2] &&
		m[1][3] == a.m[1][3] &&
		m[2][0] == a.m[2][0] &&
		m[2][1] == a.m[2][1] &&
		m[2][2] == a.m[2][2] &&
		m[2][3] == a.m[2][3];
}

RRMatrix3x4 RRMatrix3x4::operator *(const RRMatrix3x4& b) const
{
	RRMatrix3x4 c;
	for (unsigned i=0;i<3;i++)
		for (unsigned j=0;j<4;j++)
			c.m[i][j] = m[i][0]*b.m[0][j]+m[i][1]*b.m[1][j]+m[i][2]*b.m[2][j]+((j==3)?m[i][3]:0);
	return c;
}

RRMatrix3x4& RRMatrix3x4::operator *=(const RRMatrix3x4& b)
{
	return *this = *this * b;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 identity

RRMatrix3x4 RRMatrix3x4::identity()
{
	return RRMatrix3x4(
		1.0f,0,0,0,
		0,1,0,0,
		0,0,1,0);
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

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 translation

RRMatrix3x4 RRMatrix3x4::translation(const RRVec3& translation)
{
	return RRMatrix3x4(
		1,0,0,translation[0],
		0,1,0,translation[1],
		0,0,1,translation[2]);
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

void RRMatrix3x4::postTranslate(const RRVec3& a)
{
	m[0][3] += a[0];
	m[1][3] += a[1];
	m[2][3] += a[2];
}

RRMatrix3x4 RRMatrix3x4::centeredAround(const RRVec3& center) const
{
	return RRMatrix3x4::translation(center) * *this * RRMatrix3x4::translation(-center);
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 scale

RRMatrix3x4 RRMatrix3x4::scale(const RRVec3& scale)
{
	return RRMatrix3x4(
		scale[0],0,0,0,
		0,scale[1],0,0,
		0,0,scale[2],0);
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
				for (unsigned j=0;j<4;j++)
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

void RRMatrix3x4::postScale(const RRVec3& scale)
{
	for (unsigned i=0;i<3;i++)
		for (unsigned j=0;j<4;j++)
			m[i][j] *= scale[i];
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 rotation

RRMatrix3x4 RRMatrix3x4::rotationByAxisAngle(const RRVec3& rotationAxis, RRReal rotationAngleRad)
{
	double s = sin(rotationAngleRad);
	double c = cos(rotationAngleRad);
	double x = rotationAxis.x;
	double y = rotationAxis.y;
	double z = rotationAxis.z;

	return RRMatrix3x4(
		(RRReal)(c+x*x*(1-c)), (RRReal)(x*y*(1-c)+z*s), (RRReal)(x*z*(1-c)-y*s), 0,
		(RRReal)(x*y*(1-c)-z*s), (RRReal)(c+y*y*(1-c)), (RRReal)(y*z*(1-c)+x*s), 0,
		(RRReal)(x*z*(1-c)+y*s), (RRReal)(y*z*(1-c)-x*s), (RRReal)(c+z*z*(1-c)), 0);
}

RRMatrix3x4 RRMatrix3x4::rotationByYawPitchRoll(const RRVec3& yawPitchRoll)
{
	double c1 = cos(yawPitchRoll[0]);
	double c2 = cos(yawPitchRoll[1]);
	double c3 = cos(yawPitchRoll[2]);
	double s1 = sin(yawPitchRoll[0]);
	double s2 = sin(yawPitchRoll[1]);
	double s3 = sin(yawPitchRoll[2]);

	return RRMatrix3x4(
		(RRReal)(c1*c3+s1*s2*s3), (RRReal)(c3*s1*s2-c1*s3), (RRReal)(c2*s1), 0,
		(RRReal)(c2*s3), (RRReal)(c2*c3), (RRReal)(-s2), 0,
		(RRReal)(c1*s2*s3-c3*s1), (RRReal)(s1*s3+c1*c3*s2), (RRReal)(c1*c2), 0);
}

RRVec3 RRMatrix3x4::getYawPitchRoll() const
{
	if (abs(m[1][2])<1)
	{
		RRReal pitch = asin(-m[1][2]); // or RR_PI-pitch
		RRReal a = 1/cos(pitch);
		RRReal yaw = atan2(m[0][2]*a,m[2][2]*a);
		RRReal roll = atan2(m[1][0]*a,m[1][1]*a);
		return RRVec3(yaw,pitch,roll);
	}
	else
		return RRVec3(atan2(-m[2][0],m[0][0]),-RR_PI/2*m[1][2],0);
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMatrix3x4 inversion

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

} // namespace
