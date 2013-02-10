#version 120 // necessary for 'poisson' array

// LightsprintGL: Depth of field effect
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint
//
// Options:

uniform sampler2D colorMap;
uniform sampler2D depthMap;
uniform float zNear;
uniform float zFar;
uniform float focalLength;
uniform vec2 radiusAtTwiceFocalLength;
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
	float depth = texture2D(depthMap,mapCoord).x;
	float linearDepth = 2.0 * zNear * zFar / (zFar + zNear - (2.0*depth-1.0) * (zFar - zNear));
	vec2 radius = min(max(linearDepth/focalLength,focalLength/linearDepth)-1.0,10.0) * radiusAtTwiceFocalLength;
	vec4 color = texture2D(colorMap,mapCoord)*0.01;
	float colors = 0.01;
	float noise = sin(dot(mapCoord,vec2(52.714,112.9898))) * 43758.5453;
	mat2 rot = mat2(cos(noise),-sin(noise),sin(noise),cos(noise));
	for (int i=0;i<MAX_SAMPLES;i++)
	{
		vec2 sampleCoord = mapCoord + rot*poisson[i]*radius;
		//vec2 sampleCoord = mapCoord + poisson[i]*radius; // it is 5x faster without rot
		//float sampleDepth = texture2D(depthMap,sampleCoord).x;
		//if (sampleDepth>=depth-0.01)
		{
			color += pow(texture2D(colorMap,sampleCoord),vec4(2.22222,2.22222,2.22222,2.22222));
			colors += 1.0;
		}
	}
	gl_FragColor = pow(color/colors,vec4(0.45,0.45,0.45,0.45));
}
