// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Contours
//
// mXxx ... in meters
// tXxx ... in texture space 0..1
// xxx  ... none of those two

uniform sampler2D depthMap;
uniform vec2 tPixelSize; // in texture space, 1/texture resolution
varying vec2 tMapCoord;
uniform vec3 silhouetteColor;
uniform vec3 creaseColor;

#define min3(a,b,c) min(a,min(b,c))
#define max3(a,b,c) max(a,max(b,c))

vec3 detectEdge(vec2 tStep)
{
	float z_3 = texture2D(depthMap,tMapCoord-3.0*tStep).z;
	float z_2 = texture2D(depthMap,tMapCoord-2.0*tStep).z;
	float z_1 = texture2D(depthMap,tMapCoord-tStep).z;
	float z0  = texture2D(depthMap,tMapCoord).z;
	float z1  = texture2D(depthMap,tMapCoord+tStep).z;
	float z2  = texture2D(depthMap,tMapCoord+2.0*tStep).z;
	float z3  = texture2D(depthMap,tMapCoord+3.0*tStep).z;

	float dz_3 = z_2-z_3;
	float dz_2 = z_1-z_2;
	float dz_1 = z0-z_1;
	float dz1  = z1-z0;
	float dz2  = z2-z1;
	float dz3  = z3-z2;

	float adz_3 = abs(z_2-z_3);
	float adz_2 = abs(z_1-z_2);
	float adz_1 = abs(z0-z_1);
	float adz1  = abs(z1-z0);
	float adz2  = abs(z2-z1);
	float adz3  = abs(z3-z2);

	float dz_min = min3(dz_3,dz_2,dz_1);
	float dz_max = max3(dz_3,dz_2,dz_1);
	float dzmin = min3(dz3,dz2,dz1);
	float dzmax = max3(dz3,dz2,dz1);

	float ddz_2  = abs(dz_2-dz_3);
	float ddz_1  = abs(dz_1-dz_2);
	float ddz1  = abs(dz2-dz1);
	float ddz2  = abs(dz3-dz2);

	if (adz_1+adz1 > adz_3+adz_2+adz2+adz3+1e-5)
		return silhouetteColor;

	// temporary, to be replaced with proper angle calculation
	if (dz_min>dzmax || dz_max<dzmin) // all edges
	if (sign(dz_3+dz_2+dz_1)!=sign(dz1+dz2+dz3)) // edges pointing to/from camera
		return creaseColor;

	return vec3(1.0);
}

void main()
{
	vec3 color = min( detectEdge(vec2(tPixelSize.x,0.0)) , detectEdge(vec2(0.0,tPixelSize.y)) );
	if (color==vec3(1.0))
		discard;
	gl_FragColor = vec4(color,1.0);
}
