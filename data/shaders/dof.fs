// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Depth of field effect
//
// Options:
// PASS [1|2|3]

// mXxx ... in meters
// tXxx ... in texture space 0..1
// xxx  ... none of those two

#define MAX_COC 100.0 // max CoC diameter in pixels. less = extreme cases don't have enough blur, more = when edge goes to distance, there is bigger step at the end of sharp area. smallmap with 2 half channels would solve it

uniform sampler2D colorMap;
//uniform sampler2D midMap;
uniform sampler2D smallMap;
uniform sampler2D depthMap;
uniform vec2 tPixelSize;
//uniform vec3 depthRange; // near/focusFar*far/(far-near),far/(far-near),near/focusNear*far/(far-near)
uniform vec2 mDepthRange;
uniform vec2 mScreenSizeIn1mDistance;
uniform float mApertureDiameter;
uniform float mFocusDistanceNear;
uniform float mFocusDistanceFar;
in vec2 tMapCoord;
#define MAX_SAMPLES 128 // [#26]
uniform int num_samples; // 1..MAX_SAMPLES
uniform vec2 samples[MAX_SAMPLES]; // samples in -1..1 range
out vec4 fragColor;

void main()
{
#if PASS==1
	// create downscaled CoC
	float depth1 = texture(depthMap,tMapCoord+vec2(+0.5,+0.5)*tPixelSize).x; // we read from centers, NEAREST might be faster
	float depth2 = texture(depthMap,tMapCoord+vec2(+0.5,-0.5)*tPixelSize).x;
	float depth3 = texture(depthMap,tMapCoord+vec2(-0.5,+0.5)*tPixelSize).x;
	float depth4 = texture(depthMap,tMapCoord+vec2(-0.5,-0.5)*tPixelSize).x;

	float zNear = min(min(depth1,depth2),min(depth3,depth4));
	float zFar = max(max(depth1,depth2),max(depth3,depth4));
	float mZFar = mDepthRange.x / (mDepthRange.y - zFar);
	float mZNear = mDepthRange.x / (mDepthRange.y - zNear);
	//float mCoc = mApertureDiameter * abs(mZ-mFocusDistance) / mFocusDistance; // coc diameter in meters, in mZ plane
	//float tCoc = mCoc/mScreenSizeIn1mDistance/mZ = mApertureDiameter * (mZ-mFocusDistance) / mFocusDistance / mScreenSizeIn1mDistance /mZ; // coc diameter in 0..1 space
	vec2 tCocNear = mApertureDiameter * (mFocusDistanceNear-mZNear) / mFocusDistanceNear / mScreenSizeIn1mDistance / mZNear; // coc diameter in 0..1 space
	vec2 tCocFar = mApertureDiameter * (mZFar-mFocusDistanceFar) / mFocusDistanceFar / mScreenSizeIn1mDistance / mZFar; // coc diameter in 0..1 space
	vec2 cocNear = tCocNear / tPixelSize; // coc diameter in pixels
	vec2 cocFar = tCocFar / tPixelSize; // coc diameter in pixels
	fragColor = vec4(cocNear,cocFar)/MAX_COC;

#endif

#if PASS==2
	// blur CoC
	fragColor = (
		#if 1
			+texture(smallMap,tMapCoord-8.5*tPixelSize)*0.2
			+texture(smallMap,tMapCoord-6.5*tPixelSize)*0.4
			+texture(smallMap,tMapCoord-4.5*tPixelSize)*0.6
			+texture(smallMap,tMapCoord-2.5*tPixelSize)*0.8
			+texture(smallMap,tMapCoord-0.5*tPixelSize)
			+texture(smallMap,tMapCoord+1.5*tPixelSize)*0.9
			+texture(smallMap,tMapCoord+3.5*tPixelSize)*0.7
			+texture(smallMap,tMapCoord+5.5*tPixelSize)*0.5
			+texture(smallMap,tMapCoord+7.5*tPixelSize)*0.3
			+texture(smallMap,tMapCoord+9.5*tPixelSize)*0.1
			) / 5.5;
		#else
			+texture(smallMap,tMapCoord-4.5*tPixelSize)*0.36
			+texture(smallMap,tMapCoord-2.5*tPixelSize)*0.68
			+texture(smallMap,tMapCoord-0.5*tPixelSize)
			+texture(smallMap,tMapCoord+1.5*tPixelSize)*0.84
			+texture(smallMap,tMapCoord+3.5*tPixelSize)*0.52
			+texture(smallMap,tMapCoord+5.5*tPixelSize)*0.20
			) / 3.6;
		#endif
	// blurability keeps sharp edges sharp in channel r (farCoc), blur does not spread inside
	float localFarCoc = texture(smallMap,tMapCoord).r;
	float blurability = smoothstep(0.0,1.0,localFarCoc*30.0);
	fragColor.r *= blurability;
#endif

#if PASS==3
	// sample from original color texture using CoC
	vec4 tCocNearFar = (MAX_COC*0.5*texture(smallMap,tMapCoord))*tPixelSize.xyxy; // coc radius in 0..1 space
	vec2 tCocNear = tCocNearFar.xy; // coc radius in 0..1 space
	vec2 tCocFar = tCocNearFar.zw; // coc radius in 0..1 space

	// optional, makes edges in focus sharper against background (remember, smallMap is halfres, we use fullres depthMap here)
	float mZ = mDepthRange.x / (mDepthRange.y - texture(depthMap,tMapCoord).x);
	if (mZ<=mFocusDistanceFar)
		tCocFar = vec2(0.0,0.0);

	vec2 tCoc = max(tCocNear,tCocFar); // coc radius in 0..1 space
	vec4 color = pow(texture(colorMap,tMapCoord),vec4(2.22222,2.22222,2.22222,2.22222))*0.01;
	float colors = 0.01;
	float noise = sin(dot(tMapCoord,vec2(52.714,112.9898))) * 43758.5453;
	mat2 rot = mat2(cos(noise)*tCoc.x,-sin(noise)*tCoc.y,sin(noise)*tCoc.x,cos(noise)*tCoc.y);
	for (int i=0;i<num_samples;i++)
	{
		vec2 sampleCoord = tMapCoord + rot*samples[i];
		//vec2 sampleCoord = tMapCoord + samples[i]*tCoc; // it is 5x faster without rot

		vec4 tSampleCocNearFar = MAX_COC*0.5*texture(smallMap,sampleCoord)*tPixelSize.xyxy; // coc radius in 0..1 space
		vec2 tSampleCoc = max(tSampleCocNearFar.xy,tSampleCocNearFar.zw);
		if (tSampleCoc.x>=tCoc.x*length(samples[i]))
		{
			color += pow(texture(colorMap,sampleCoord),vec4(2.22222,2.22222,2.22222,2.22222));
			colors += 1.0;
		}
	}
	fragColor = pow(color/colors,vec4(0.45,0.45,0.45,0.45));
#endif
}
