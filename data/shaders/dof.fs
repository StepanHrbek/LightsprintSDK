// Depth of field effect [work in progress, includes unused paths]
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint
//
// Options:
// PASS [1|2|3]

// mXxx ... in meters
// tXxx ... in texture space 0..1
// xxx  ... none of those two

uniform sampler2D colorMap;
//uniform sampler2D midMap;
uniform sampler2D smallMap;
uniform sampler2D depthMap;
uniform vec2 tPixelSize;
uniform vec3 depthRange; // near/focusFar*far/(far-near),far/(far-near),near/focusNear*far/(far-near)
varying vec2 tMapCoord;
#define MAX_SAMPLES 128 // [#26]
uniform int num_samples; // 1..MAX_SAMPLES
uniform vec2 samples[MAX_SAMPLES]; // samples in -1..1 range

void main()
{
//#define BLUR_RGB
//#define BLUR_A_COC
#define BLUR_A_COC_NEAR
#define BLUR_R_COC_FAR
#define COC_BOOST 5.0
#define MAX_COC 10.0 // less = even extreme cases have very small blur, more = when edge goes to distance, there is bigger step at the end of sharp area. smallmap with 2 half channels would solve it
#if PASS==1
	// create downscaled color+CoC
	#ifdef BLUR_RGB
		#if 1
			vec4 color = (
				+ pow(texture2D(colorMap,tMapCoord+vec2(+0.5,+0.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(-0.5,-0.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(+2.5,+0.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(+0.5,+2.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(-0.5,-2.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(-2.5,-0.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(+1.5,-1.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,tMapCoord+vec2(-1.5,+1.5)*tPixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				)*0.125;
		#else
			vec4 color = (
				+ texture2D(colorMap,tMapCoord+vec2(+0.5,+1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-0.5,-1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(+1.5,-0.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-1.5,+0.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(+2.5,+1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-2.5,-1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(+1.5,-2.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-1.5,+2.5)*tPixelSize)
				)*0.125;
			vec4 color = (
				+ texture2D(colorMap,tMapCoord+vec2(+0.5,+1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-0.5,-1.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(+1.5,-0.5)*tPixelSize)
				+ texture2D(colorMap,tMapCoord+vec2(-1.5,+0.5)*tPixelSize)
				)*0.25;
			vec4 color = texture2D(colorMap,tMapCoord);
		#endif
		gl_FragColor.rgb = color.rgb;
	#endif
	float depth1 = texture2D(depthMap,tMapCoord+vec2(+0.5,+0.5)*tPixelSize).x; // we read from centers, NEAREST might be faster
	float depth2 = texture2D(depthMap,tMapCoord+vec2(+0.5,-0.5)*tPixelSize).x;
	float depth3 = texture2D(depthMap,tMapCoord+vec2(-0.5,+0.5)*tPixelSize).x;
	float depth4 = texture2D(depthMap,tMapCoord+vec2(-0.5,-0.5)*tPixelSize).x;
	#ifdef BLUR_A_COC
		float depthMin = min(min(depth1,depth2),min(depth3,depth4));
		float depthMax = max(max(depth1,depth2),max(depth3,depth4));
		gl_FragColor.a = (max((depthRange.y - depthMax) / depthRange.z, depthRange.x / (depthRange.y - depthMin))-1.0)/MAX_COC;
	#endif
	#ifdef BLUR_A_COC_NEAR
		float depth = min(min(depth1,depth2),min(depth3,depth4));
		gl_FragColor.a = ((depthRange.y - depth) / depthRange.z - 1.0)/MAX_COC;
	#endif
	#ifdef BLUR_R_COC_FAR
		// if we don't blur far coc, there are occassional sharp edges visible between far objects, e.g. slightly blurred trees and highly blurred background
		{
		float depth = max(max(depth1,depth2),max(depth3,depth4));
		gl_FragColor.r = (depthRange.x / (depthRange.y - depth) - 1.0)/MAX_COC;
		}
	#endif
#endif
#if PASS==2
	// blur color+CoC
	gl_FragColor = (
		#if 1
			+texture2D(smallMap,tMapCoord-8.5*tPixelSize)*0.2
			+texture2D(smallMap,tMapCoord-6.5*tPixelSize)*0.4
			+texture2D(smallMap,tMapCoord-4.5*tPixelSize)*0.6
			+texture2D(smallMap,tMapCoord-2.5*tPixelSize)*0.8
			+texture2D(smallMap,tMapCoord-0.5*tPixelSize)
			+texture2D(smallMap,tMapCoord+1.5*tPixelSize)*0.9
			+texture2D(smallMap,tMapCoord+3.5*tPixelSize)*0.7
			+texture2D(smallMap,tMapCoord+5.5*tPixelSize)*0.5
			+texture2D(smallMap,tMapCoord+7.5*tPixelSize)*0.3
			+texture2D(smallMap,tMapCoord+9.5*tPixelSize)*0.1
			) / 5.5;
		#else
			+texture2D(smallMap,tMapCoord-4.5*tPixelSize)*0.36
			+texture2D(smallMap,tMapCoord-2.5*tPixelSize)*0.68
			+texture2D(smallMap,tMapCoord-0.5*tPixelSize)
			+texture2D(smallMap,tMapCoord+1.5*tPixelSize)*0.84
			+texture2D(smallMap,tMapCoord+3.5*tPixelSize)*0.52
			+texture2D(smallMap,tMapCoord+5.5*tPixelSize)*0.20
			) / 3.6;
		#endif
	// blurability keeps sharp edges sharp in channel r (farCoc), blur does not spread inside
	float localFarCoc = texture2D(smallMap,tMapCoord).r;
	float blurability = smoothstep(0.0,1.0,localFarCoc*30.0);
	gl_FragColor.r *= blurability;
#endif
#if PASS==3
	// apply downscaled blurred color+CoC
	//vec4 color1 = pow(texture2D(colorMap,tMapCoord),vec4(2.22222,2.22222,2.22222,2.22222));
	//vec4 color2 = texture2D(midMap,tMapCoord);
	vec4 color3 = texture2D(smallMap,tMapCoord);

	#if 1
		float nearCoc = color3.a*MAX_COC;
	#else
		float nearCoc = (max(color2.a,color3.a)*2.0-color2.a)*MAX_COC;
	#endif
	#if 0
		color2 = ( color1 * 0.25
			+ texture2D(colorMap,tMapCoord+tPixelSize*vec2(0.5,1.5))
			+ texture2D(colorMap,tMapCoord+tPixelSize*vec2(-0.5,-1.5))
			+ texture2D(colorMap,tMapCoord+tPixelSize*vec2(1.5,-0.5))
			+ texture2D(colorMap,tMapCoord+tPixelSize*vec2(-1.5,0.5))
			) / 4.25;
	#endif
	#ifdef BLUR_A_COC
		float coc = nearCoc*COC_BOOST;
	#endif
	#ifdef BLUR_A_COC_NEAR
		#ifdef BLUR_R_COC_FAR
			// in contrast to simple farCoc = color3.r*MAX_COC; following code makes edges in focus sharper against background (remember, color3.r is halfres)
			float farCoc = depthRange.x / (depthRange.y - texture2D(depthMap,tMapCoord).x) - 1.0;
			farCoc = (farCoc<=0.0) ? 0.0 : color3.r*MAX_COC;
		#else
			float farCoc = depthRange.x / (depthRange.y - texture2D(depthMap,tMapCoord).x) - 1.0;
		#endif
		float maxCoc = max(nearCoc,farCoc);
		float coc = maxCoc*COC_BOOST;
	#endif

	#if 0
		// average single sample from 3 textures (fullres, small, blurry..)
		#define COC1 1.0
		#define COC2 3.0
		#define COC3 16.0
		float a1 = clamp((COC2-coc)/(COC2-COC1),0.0,1.0);
		float a3 = clamp((coc-COC2)/(COC3-COC2),0.0,1.0);
		float a2 = 1.0-a1-a3;
		vec4 color = color1*a1+color2*a2+color3*a3;
		gl_FragColor = color;
		gl_FragColor = pow(color,vec4(0.45,0.45,0.45,0.45));
		//gl_FragColor = vec4(coc)/10.0;
	#else
		// average many samples from single (fullres) texture
		coc = clamp(coc,0.0,50.0);
		vec4 color = pow(texture2D(colorMap,tMapCoord),vec4(2.22222,2.22222,2.22222,2.22222))*0.01;
		float colors = 0.01;
		float noise = sin(dot(tMapCoord,vec2(52.714,112.9898))) * 43758.5453;
		mat2 rot = mat2(cos(noise)*coc*tPixelSize.x,-sin(noise)*coc*tPixelSize.y,sin(noise)*coc*tPixelSize.x,cos(noise)*coc*tPixelSize.y);
		for (int i=0;i<num_samples;i++)
		{
			vec2 sampleCoord = tMapCoord + rot*samples[i];
			//vec2 sampleCoord = tMapCoord + samples[i]*coc*tPixelSize; // it is 5x faster without rot
			#if defined(BLUR_A_COC_NEAR) && !defined(BLUR_R_COC_FAR)
				float sampleNearCoc = texture2D(smallMap,sampleCoord).a*MAX_COC;
				float sampleFarCoc = depthRange.x / (depthRange.y - texture2D(depthMap,sampleCoord).x) - 1.0;
				float sampleMaxCoc = max(sampleNearCoc,sampleFarCoc);
				if (sampleMaxCoc>=maxCoc*length(samples[i]))
			#endif
			#if defined(BLUR_A_COC_NEAR) && defined(BLUR_R_COC_FAR)
				vec4 sample = texture2D(smallMap,sampleCoord)*MAX_COC;
				float sampleNearCoc = sample.a;
				float sampleFarCoc = sample.r;
				float sampleMaxCoc = max(sampleNearCoc,sampleFarCoc);
				if (sampleMaxCoc>=maxCoc*length(samples[i]))
			#endif
			{
				color += pow(texture2D(colorMap,sampleCoord),vec4(2.22222,2.22222,2.22222,2.22222));
				colors += 1.0;
			}
		}
		gl_FragColor = pow(color/colors,vec4(0.45,0.45,0.45,0.45));
	#endif
#endif
}
