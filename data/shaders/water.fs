// Water with planar reflection, fresnel and animated waves
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint
//
// Options:
// #define FRESNEL - calculates refraction intensity according to fresnel term, 30% slowdown
// #define DIRLIGHT - enables one directional light, 20% slowdown

uniform sampler2D mirrorMap;
uniform sampler2D normalMap;
uniform float time; // in seconds
varying vec4 mirrorCoord;
varying vec3 worldPos;
uniform vec4 waterColor; // a = reflectivity

#if defined(FRESNEL) || defined(DIRLIGHT)
	uniform vec3 worldEyePos;
#endif

#ifdef DIRLIGHT
	uniform vec3 worldLightDir;
	uniform vec3 lightColor;
#endif

#ifdef FRESNEL
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

void main()
{
	vec2 uv = vec2(0.5,0.5)+vec2(0.5,-0.5)*mirrorCoord.xy/mirrorCoord.w;

	vec2 worldNormal2 = (
		+ texture2D(normalMap,worldPos.zx*0.09+time*0.5*vec2(cos(time*0.05)*-0.0006-0.0049,0.0),-1.0).xz
		- texture2D(normalMap,worldPos.xz*0.12+time*0.5*vec2(0.051,0.019                      ),-1.0).xz
		) * 0.1;
	vec4 env = texture2D(mirrorMap,uv+worldNormal2);

#if defined(FRESNEL) || defined(DIRLIGHT)
	vec3 worldEyeDir = normalize(worldEyePos-worldPos);
	vec3 worldNormal3 = normalize(vec3(worldNormal2.x,0.2,worldNormal2.y));
#endif

#ifdef FRESNEL
	gl_FragColor = mix(waterColor,env,clamp(reflectance(worldEyeDir,worldNormal3),0.2,0.6));
#else
	gl_FragColor = mix(env,waterColor,waterColor.a);
#endif

#ifdef DIRLIGHT
	// add reflection from directional light
	gl_FragColor.xyz += pow(max(dot(normalize(worldEyeDir-worldLightDir),worldNormal3),0.5),100.0)*lightColor;
#endif
}
