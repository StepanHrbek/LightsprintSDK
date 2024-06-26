// Copyright (C) 1999-2021 Stepan Hrbek
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
in vec2 tMapCoord;
uniform vec4 silhouetteColor; // a=opacity, 1=silhouette fully visibile
uniform vec4 creaseColor; // a=opacity, 1=crease fully visibile
out vec4 fragColor;

#define min3(a,b,c) min(a,min(b,c))
#define max3(a,b,c) max(a,max(b,c))

vec4 detectEdge(vec2 tStep)
{
	float z_3 = texture(depthMap,tMapCoord-3.0*tStep);
	float z_2 = texture(depthMap,tMapCoord-2.0*tStep);
	float z_1 = texture(depthMap,tMapCoord-tStep);
	float z0  = texture(depthMap,tMapCoord);
	float z1  = texture(depthMap,tMapCoord+tStep);
	float z2  = texture(depthMap,tMapCoord+2.0*tStep);
	float z3  = texture(depthMap,tMapCoord+3.0*tStep);

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

	return vec4(0.0);
}

void main()
{
	vec4 x = detectEdge(vec2(tPixelSize.x,0.0));
	vec4 y = detectEdge(vec2(0.0,tPixelSize.y));
	vec4 color = (x.a>y.a) ? x : y;
	if (color.a==0.0)
		discard;
	fragColor = color;
}
