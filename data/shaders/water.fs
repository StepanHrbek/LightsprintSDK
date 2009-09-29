// Water with planar reflection, fresnel and scale independent waves
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//
// Options:
// #define FRESNEL - enables refraction, water color, 20% slowdown
// #define DIRLIGHT - enables one directional light, 15% slowdown

uniform sampler2D mirrorMap;
uniform sampler2D normalMap;
uniform float time; // in seconds
varying vec4 mirrorCoord;
varying vec3 worldPos;
uniform vec3 worldEyePos;

#ifdef DIRLIGHT
	uniform vec3 worldLightDir;
	uniform vec3 lightColor;
#endif

#ifdef FRESNEL
	uniform vec4 waterColor;

	float reflectance(in vec3 incom, in vec3 normal)
	{
		float index_internal = 1.333; // water
		float eta = 1.0/index_internal;
		float cos_theta1 = dot(incom, normal);
		float cos_theta2 = sqrt(1.0 - ((eta * eta) * ( 1.0 - (cos_theta1 * cos_theta1))));
		float fresnel_rs = (cos_theta1 - index_internal * cos_theta2 ) / (cos_theta1 + index_internal * cos_theta2);
		float fresnel_rp = (index_internal * cos_theta1 - cos_theta2 ) / (index_internal * cos_theta1 + cos_theta2);
		return (fresnel_rs * fresnel_rs + fresnel_rp * fresnel_rp) * 0.5;
	}
#endif

//vec2 sampleNormal(float step)
//{
//	float a = pow(6.0,step-3.0);
//	float b = pow(10.0,1.0-step);
//	return texture2D(normalMap,vec2(worldPos.x+time*a,worldPos.z)*b).xy + vec2(-0.5,-0.5);
//}

void main()
{
	// mirror reflection
	vec2 uv = vec2(0.5,0.5)+vec2(0.5,-0.5)*mirrorCoord.xy/mirrorCoord.w;

	float aalevel = length(dFdy(worldPos.xz))*100.0;
	float depth = 0.0;
	while (aalevel>1.0)
	{
		depth += 1.0;
		aalevel *= 0.1;
	}
//	vec2 worldNormal2 = mix(sampleNormal(depth),sampleNormal(depth+1.0),(aalevel-0.1)*1.0/0.9) * 0.2;
	float a = pow(6.0,depth-3.0);
	float b = pow(10.0,1.0-depth);
	vec2 m = texture2D(normalMap,vec2(worldPos.x+time*a,worldPos.z)*b).xy;
	vec2 n = texture2D(normalMap,vec2(worldPos.x+time*a*6.0,worldPos.z)*b*0.1).xy;
	vec2 worldNormal2 = ( mix(m,n,(aalevel-0.1)*1.0/0.9) + vec2(-0.5,-0.5) ) * 0.2;

	vec4 env = texture2D(mirrorMap,uv+worldNormal2);
	vec3 worldEyeDir = normalize(worldEyePos-worldPos);
	vec3 worldNormal3 = normalize(vec3(worldNormal2.x,0.2,worldNormal2.y));

#ifdef FRESNEL
	gl_FragColor = mix(waterColor,env,clamp(reflectance(worldEyeDir,worldNormal3),0.0,1.0));
#else
	gl_FragColor = env;
#endif

#ifdef DIRLIGHT
	// add reflection from directional light
	gl_FragColor.xyz += pow(max(dot(normalize(worldEyeDir-worldLightDir),worldNormal3),0.5),100.0)*lightColor;
#endif
}
