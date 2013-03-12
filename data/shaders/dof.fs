#version 120 // necessary for 'poisson' array

// LightsprintGL: Depth of field effect [work in progress, includes unused paths]
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint
//
// Options:
// PASS [1|2|3]

uniform sampler2D colorMap;
uniform sampler2D midMap;
uniform sampler2D smallMap;
uniform sampler2D depthMap;
uniform vec2 pixelSize;
uniform vec3 depthRange; // near/focusFar*far/(far-near),far/(far-near),near/focusNear*far/(far-near)
varying vec2 mapCoord;

#define MAX_SAMPLES 60
vec2 poisson[MAX_SAMPLES] = vec2[MAX_SAMPLES](
	vec2(-0.8769053, -0.1801577),
	vec2(-0.560903, 0.06491917),
	vec2(-0.8496868, -0.4068463),
	vec2(-0.6684758, -0.2965162),
	vec2(-0.7673495, -0.005029257),
	vec2(-0.9880703, 0.04116542),
	vec2(-0.7846559, 0.3813636),
	vec2(-0.7290462, 0.5805985),
	vec2(-0.3905317, 0.3820366),
	vec2(-0.4862368, 0.5637667),
	vec2(-0.9504438, 0.2511463),
	vec2(-0.4776598, -0.1798614),
	vec2(-0.604219, 0.2934287),
	vec2(-0.2081008, -0.04978198),
	vec2(-0.2508037, 0.1751049),
	vec2(-0.6864566, -0.5584068),
	vec2(-0.4704278, -0.3887475),
	vec2(-0.4935126, -0.6113455),
	vec2(-0.1219745, -0.2906288),
	vec2(-0.2514639, -0.4794517),
	vec2(0.001370483, -0.5498651),
	vec2(-0.2616202, -0.7039202),
	vec2(-0.08041584, -0.8092448),
	vec2(0.03335056, 0.2391213),
	vec2(0.005224985, 0.01072675),
	vec2(0.1313823, -0.1700258),
	vec2(-0.4821748, 0.8128408),
	vec2(0.1671764, -0.7225859),
	vec2(0.1472889, -0.9805518),
	vec2(-0.319748, -0.9090942),
	vec2(-0.2708471, 0.8782267),
	vec2(-0.2557987, 0.5722433),
	vec2(-0.02759428, 0.67807520),
	vec2(-0.08137709, 0.4126224),
	vec2(0.1456477, 0.8903562),
	vec2(0.3371713, 0.7640222),
	vec2(0.2000765, 0.4972369),
	vec2(-0.06930163, 0.964883),
	vec2(0.2733895, 0.09887506),
	vec2(0.2422106, -0.463336),
	vec2(-0.5466529, -0.8083814),
	vec2(0.6191299, 0.5104562),
	vec2(0.5362201, 0.7429689),
	vec2(0.3735984, 0.3863356),
	vec2(0.6939798, 0.1787464),
	vec2(0.4732445, 0.1937991),
	vec2(0.8838001, 0.2504165),
	vec2(0.8430692, 0.4679047),
	vec2(0.447724, -0.8855019),
	vec2(0.5618694, -0.7126608),
	vec2(0.4958969, -0.4975741),
	vec2(0.8366239, -0.3825552),
	vec2(0.717171, -0.5788124),
	vec2(0.5782395, -0.1434195),
	vec2(0.3609872, -0.1903974),
	vec2(0.7937168, -0.00281623),
	vec2(0.9551116, -0.1922427),
	vec2(0.6263707, -0.3429523),
	vec2(0.9990594, 0.01618059),
	vec2(0.3529771, -0.6476724)
);

void main()
{
//#define BLUR_RGB
//#define BLUR_A_COC
#define BLUR_A_COC_NEAR
#define BLUR_R_COC_FAR
#define COC_BOOST 5.0
#define MAX_COC 10.0
#if PASS==1
	// create downscaled color+CoC
	#ifdef BLUR_RGB
		#if 1
			vec4 color = (
				+ pow(texture2D(colorMap,mapCoord+vec2(+0.5,+0.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(-0.5,-0.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(+2.5,+0.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(+0.5,+2.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(-0.5,-2.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(-2.5,-0.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(+1.5,-1.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				+ pow(texture2D(colorMap,mapCoord+vec2(-1.5,+1.5)*pixelSize),vec4(2.22222,2.22222,2.22222,2.22222))
				)*0.125;
		#else
			vec4 color = (
				+ texture2D(colorMap,mapCoord+vec2(+0.5,+1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-0.5,-1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(+1.5,-0.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-1.5,+0.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(+2.5,+1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-2.5,-1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(+1.5,-2.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-1.5,+2.5)*pixelSize)
				)*0.125;
			vec4 color = (
				+ texture2D(colorMap,mapCoord+vec2(+0.5,+1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-0.5,-1.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(+1.5,-0.5)*pixelSize)
				+ texture2D(colorMap,mapCoord+vec2(-1.5,+0.5)*pixelSize)
				)*0.25;
			vec4 color = texture2D(colorMap,mapCoord);
		#endif
		gl_FragColor.rgb = color.rgb;
	#endif
	float depth1 = texture2D(depthMap,mapCoord+vec2(+0.5,+0.5)*pixelSize).x;
	float depth2 = texture2D(depthMap,mapCoord+vec2(+0.5,-0.5)*pixelSize).x;
	float depth3 = texture2D(depthMap,mapCoord+vec2(-0.5,+0.5)*pixelSize).x;
	float depth4 = texture2D(depthMap,mapCoord+vec2(-0.5,-0.5)*pixelSize).x;
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
			+texture2D(smallMap,mapCoord-8.5*pixelSize)*0.2
			+texture2D(smallMap,mapCoord-6.5*pixelSize)*0.4
			+texture2D(smallMap,mapCoord-4.5*pixelSize)*0.6
			+texture2D(smallMap,mapCoord-2.5*pixelSize)*0.8
			+texture2D(smallMap,mapCoord-0.5*pixelSize)
			+texture2D(smallMap,mapCoord+1.5*pixelSize)*0.9
			+texture2D(smallMap,mapCoord+3.5*pixelSize)*0.7
			+texture2D(smallMap,mapCoord+5.5*pixelSize)*0.5
			+texture2D(smallMap,mapCoord+7.5*pixelSize)*0.3
			+texture2D(smallMap,mapCoord+9.5*pixelSize)*0.1
			) / 5.5;
		#else
			+texture2D(smallMap,mapCoord-4.5*pixelSize)*0.36
			+texture2D(smallMap,mapCoord-2.5*pixelSize)*0.68
			+texture2D(smallMap,mapCoord-0.5*pixelSize)
			+texture2D(smallMap,mapCoord+1.5*pixelSize)*0.84
			+texture2D(smallMap,mapCoord+3.5*pixelSize)*0.52
			+texture2D(smallMap,mapCoord+5.5*pixelSize)*0.20
			) / 3.6;
		#endif
#endif
#if PASS==3
	// apply downscaled blurred color+CoC
	//vec4 color1 = pow(texture2D(colorMap,mapCoord),vec4(2.22222,2.22222,2.22222,2.22222));
	vec4 color2 = texture2D(midMap,mapCoord);
	vec4 color3 = texture2D(smallMap,mapCoord);

	#if 1
		float nearCoc = color3.a*MAX_COC;
	#else
		float nearCoc = (max(color2.a,color3.a)*2.0-color2.a)*MAX_COC;
	#endif
	#if 0
		color2 = ( color1 * 0.25
			+ texture2D(colorMap,mapCoord+pixelSize*vec2(0.5,1.5))
			+ texture2D(colorMap,mapCoord+pixelSize*vec2(-0.5,-1.5))
			+ texture2D(colorMap,mapCoord+pixelSize*vec2(1.5,-0.5))
			+ texture2D(colorMap,mapCoord+pixelSize*vec2(-1.5,0.5))
			) / 4.25;
	#endif
	#ifdef BLUR_A_COC
		float coc = nearCoc*COC_BOOST;
	#endif
	#ifdef BLUR_A_COC_NEAR
		#ifdef BLUR_R_COC_FAR
			// for any unblurred farcoc<=0.01, we make pixel sharp, otherwise we use blurred farcoc
			// why?
			//  pass 2 blurred all edges. here we resharp edges between pixels in focus and background
			//  edges between pixels in focus and foreground must stay blurry because foreground overlaps pixels in focus
			// why 0.01?
			//  <=0.0 would let sharp objects have blurry edges, <=0.03 would make it visible where far blur begins
			float farCoc = (color2.r<=0.01) ? 0.0 : color3.r*MAX_COC;
		#else
			float farCoc = depthRange.x / (depthRange.y - texture2D(depthMap,mapCoord).x) - 1.0;
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
		// average 60 samples from single (fullres) texture
		coc = clamp(coc,0.0,50.0);
		vec4 color = texture2D(colorMap,mapCoord)*0.01;
		float colors = 0.01;
		float noise = sin(dot(mapCoord,vec2(52.714,112.9898))) * 43758.5453;
		mat2 rot = mat2(cos(noise)*coc*pixelSize.x,-sin(noise)*coc*pixelSize.y,sin(noise)*coc*pixelSize.x,cos(noise)*coc*pixelSize.y);
		for (int i=0;i<MAX_SAMPLES;i++)
		{
			vec2 sampleCoord = mapCoord + rot*poisson[i];
			//vec2 sampleCoord = mapCoord + poisson[i]*coc*pixelSize; // it is 5x faster without rot
			#if defined(BLUR_A_COC_NEAR) && !defined(BLUR_R_COC_FAR)
				float sampleNearCoc = texture2D(smallMap,sampleCoord).a*MAX_COC;
				float sampleFarCoc = depthRange.x / (depthRange.y - texture2D(depthMap,sampleCoord).x) - 1.0;
				float sampleMaxCoc = max(sampleNearCoc,sampleFarCoc);
				if (sampleMaxCoc>=maxCoc*length(poisson[i]))
			#endif
			#if defined(BLUR_A_COC_NEAR) && defined(BLUR_R_COC_FAR)
				vec4 sample = texture2D(smallMap,sampleCoord)*MAX_COC;
				float sampleNearCoc = sample.a;
				float sampleFarCoc = sample.r;
				float sampleMaxCoc = max(sampleNearCoc,sampleFarCoc);
				if (sampleMaxCoc>=maxCoc*length(poisson[i]))
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
