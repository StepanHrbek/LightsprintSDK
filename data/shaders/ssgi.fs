// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Screen space global illumination
//
// Options:
// #define PASS [1|2]

// mXxx ... in meters
// tXxx ... in texture space 0..1
// xxx  ... none of those two

uniform sampler2D colorMap;
uniform sampler2D depthMap;
uniform vec2 tPixelSize; // in texture space, 1/texture resolution
in vec2 tMapCoord;
out vec4 fragColor;

#if PASS==1

uniform vec2 mDepthRange; // from camera: near*far/(far-near), far/(far-near)
uniform float mAORange; // user defined
uniform float occlusionIntensity; // user defined
uniform float angleBias; // user defined
uniform vec2 mScreenSizeIn1mDistance; // in meters

#define NUM_STEPS 8
#define NUM_DIRS 4

void main()
{
	float noise = sin(dot(tMapCoord,vec2(52.714,112.9898))) * 43758.5453;
	float noise01 = mod(noise,1.0); // result in 0..1
	vec2 noiseSinCos = vec2(sin(noise),cos(noise));
	vec2 dir[NUM_DIRS];
	dir[0] = noiseSinCos;
	dir[1] = -noiseSinCos;
	dir[2] = vec2(noiseSinCos.y,-noiseSinCos.x);
	dir[3] = vec2(-noiseSinCos.y,noiseSinCos.x);

	// get normal
	// lots of lookups, but still cheaper than prerendering normalmap
	vec3 c0   = texture(colorMap,tMapCoord).rgb;
	vec3 cx_2 = texture(colorMap,vec2(tMapCoord.x-2.0*tPixelSize.x,tMapCoord.y)).rgb;
	vec3 cx_1 = texture(colorMap,vec2(tMapCoord.x-tPixelSize.x,tMapCoord.y)).rgb;
	vec3 cx1  = texture(colorMap,vec2(tMapCoord.x+tPixelSize.x,tMapCoord.y)).rgb;
	vec3 cx2  = texture(colorMap,vec2(tMapCoord.x+2.0*tPixelSize.x,tMapCoord.y)).rgb;
	vec3 cy_2 = texture(colorMap,vec2(tMapCoord.x,tMapCoord.y-2.0*tPixelSize.y)).rgb;
	vec3 cy_1 = texture(colorMap,vec2(tMapCoord.x,tMapCoord.y-tPixelSize.y)).rgb;
	vec3 cy1  = texture(colorMap,vec2(tMapCoord.x,tMapCoord.y+tPixelSize.y)).rgb;
	vec3 cy2  = texture(colorMap,vec2(tMapCoord.x,tMapCoord.y+2.0*tPixelSize.y)).rgb;
	float z0   = texture(depthMap,tMapCoord).z;
	if (z0>=1.0) // prevents noise on background that shows up when SSGI radius is close to camera far
	{
		fragColor = vec4(1.0);
		return;
	}
	//float zx_2 = texture(depthMap,tMapCoord-vec2(tPixelSize.x*2.0,0.0)).z;
	float zx_1 = texture(depthMap,tMapCoord-vec2(tPixelSize.x,0.0)).z;
	float zx1  = texture(depthMap,tMapCoord+vec2(tPixelSize.x,0.0)).z;
	//float zx2  = texture(depthMap,tMapCoord+vec2(tPixelSize.x*2.0,0.0)).z;
	//float zy_2 = texture(depthMap,tMapCoord-vec2(0.0,tPixelSize.y*2.0)).z;
	float zy_1 = texture(depthMap,tMapCoord-vec2(0.0,tPixelSize.y)).z;
	float zy1  = texture(depthMap,tMapCoord+vec2(0.0,tPixelSize.y)).z;
	//float zy2  = texture(depthMap,tMapCoord+vec2(0.0,tPixelSize.y*2.0)).z;
	float mLinearz0   = mDepthRange.x / (mDepthRange.y - z0);
	// a) 50% of edge 2x2 pixels assigned to wrong plane
	//vec2 dz = vec2(dFdx(z0),dFdy(z0));
	// b) 50% of edge pixels assigned to wrong plane
	//vec2 dz = vec2(abs(z0-zx_1)<abs(zx1-z0)?z0-zx_1:zx1-z0,abs(z0-zy_1)<abs(zy1-z0)?z0-zy_1:zy1-z0);
	// c) ~1% of edge pixels assigned to wrong plane
	//vec2 dz = vec2(abs(z0+zx_2-2.0*zx_1)<abs(z0+zx2-2.0*zx1)?z0-zx_1:zx1-z0,abs(z0+zy_2-2.0*zy_1)<abs(z0+zy2-2.0*zy1)?z0-zy_1:zy1-z0);
	// d) <<1% of edge pixels assigned to wrong plane (more when plane's colors don't differ)
	vec2 dz = vec2(length(c0+cx_2-2.0*cx_1)<length(c0+cx2-2.0*cx1)?z0-zx_1:zx1-z0,length(c0+cy_2-2.0*cy_1)<length(c0+cy2-2.0*cy1)?z0-zy_1:zy1-z0);
	// e) all edge pixels would get correct normal only if we prerender normals into separated texture

	vec2 mLineardz = mDepthRange.xx / (mDepthRange.yy - z0-dz) - vec2(mLinearz0,mLinearz0); // how much z changes within current pixel (m)
	float mAONoisyRange = mAORange;//*(0.5+noise01);
	vec2 tAORangeIn1mDistance = mAONoisyRange/mScreenSizeIn1mDistance;
	vec2 tAORangeInz0Distance = tAORangeIn1mDistance/mLinearz0;

	float sumOcclusion = 0.0;
	vec4 occlusionColor = vec4(0.00001);
	for (int d=0;d<NUM_DIRS;d++)
	{
		float maxOcclusion = 0.0;
		vec2 tRangeInTextureSpace = tAORangeInz0Distance*dir[d];
		for (int s=0;s<NUM_STEPS;s++)
		{
			float mid = (0.001+noise01+float(s))/float(NUM_STEPS); // randomized step prevents shadows forming strips. 0.001 prevents mysterious random blackpixels
			vec2 tMid = tMapCoord+mid*tRangeInTextureSpace;
			if (tMid.x<0.0 || tMid.x>1.0 || tMid.y<0.0 || tMid.y>1.0)
				continue; // removes shadow from viewport border
			tMid = (floor(tMid/tPixelSize)+vec2(0.5,0.5))*tPixelSize; // round tMid to center of texel
			float mExpectedLinearz1 = mLinearz0 + dot((tMid-tMapCoord)/tPixelSize,mLineardz);
			float mActualLinearz1 = mDepthRange.x / (mDepthRange.y - texture(depthMap,tMid).z);
			float r = (mExpectedLinearz1-mActualLinearz1)/mAONoisyRange;
			float att = max(1.0-r*r,0.0);
			float occlusion = r*att/mid-angleBias;
			if (occlusion>maxOcclusion)
			{
				vec4 c1 = texture(colorMap,tMid);
				c1.a = 1.0; // can be 0 after mirror
				occlusionColor += c1*(occlusion-maxOcclusion);
				maxOcclusion = occlusion;
			}
		}
		//sumOcclusion += maxOcclusion*0.8; // dela strmejsi gradient, svetlejsi kraje, tmavsi kouty. mohlo by dobre vyvazovat blur kterej kouty zesvetli
		sumOcclusion += atan(maxOcclusion); // atan(x) zajistuje mirnejsi gradient, ne tak tmavy kouty jako pri x
	}
	fragColor = vec4(1.0)-occlusionIntensity/float(NUM_DIRS)*sumOcclusion*(vec4(1.0)-occlusionColor/occlusionColor.w);
	// alpha must be 1, following blur pass expects it
}

#elif PASS==2

uniform sampler2D aoMap;

void main()
{
	// blur AO
//	vec4 c_3 = texture(colorMap,tMapCoord-3.0*tPixelSize);
	vec4 c_2 = texture(colorMap,tMapCoord-2.0*tPixelSize);
	vec4 c_1 = texture(colorMap,tMapCoord-tPixelSize);
	vec4 c0  = texture(colorMap,tMapCoord);
	vec4 c1  = texture(colorMap,tMapCoord+tPixelSize);
	vec4 c2  = texture(colorMap,tMapCoord+2.0*tPixelSize);
//	vec4 c3  = texture(colorMap,tMapCoord+3.0*tPixelSize);

	vec4 a_3 = texture(aoMap,tMapCoord-3.0*tPixelSize);
	vec4 a_2 = texture(aoMap,tMapCoord-2.0*tPixelSize);
	vec4 a_1 = texture(aoMap,tMapCoord-tPixelSize);
	vec4 a0  = texture(aoMap,tMapCoord);
	vec4 a1  = texture(aoMap,tMapCoord+tPixelSize);
	vec4 a2  = texture(aoMap,tMapCoord+2.0*tPixelSize);
	vec4 a3  = texture(aoMap,tMapCoord+3.0*tPixelSize);

	float z_3 = texture(depthMap,tMapCoord-3.0*tPixelSize).z;
	float z_2 = texture(depthMap,tMapCoord-2.0*tPixelSize).z;
	float z_1 = texture(depthMap,tMapCoord-tPixelSize).z;
	float z0  = texture(depthMap,tMapCoord).z;
	float z1  = texture(depthMap,tMapCoord+tPixelSize).z;
	float z2  = texture(depthMap,tMapCoord+2.0*tPixelSize).z;
	float z3  = texture(depthMap,tMapCoord+3.0*tPixelSize).z;

	float dz_3 = (z_2-z_3);
	float dz_2 = (z_1-z_2);
	float dz_1 = (z0-z_1);
	float dz1  = (z1-z0);
	float dz2  = (z2-z1);
	float dz3  = (z3-z2);

	float dz = length((c0+c_2-2.0*c_1).rgb)<length((c0+c2-2.0*c1).rgb)?dz_1:dz1;
	float dzepsilon = abs(dz*0.1);
	
	vec4 col = 1.4*a0;
	if (abs(dz_1-dz)<=dzepsilon)
	{
		col += a_1;
		if (abs(dz_2-dz)<=dzepsilon)
		{
			col += 0.7*a_2;
			if (abs(dz_3-dz)<=dzepsilon)
				col += 0.4*a_3;
		}
	}
	if (abs(dz1-dz)<=dzepsilon)
	{
		col += a1;
		if (abs(dz2-dz)<=dzepsilon)
		{
			col += 0.7*a2;
			if (abs(dz3-dz)<=dzepsilon)
				col += 0.4*a3;
		}
	}
	fragColor = col / col.w;
}

#endif
