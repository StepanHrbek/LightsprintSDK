// Water with planar reflection, fresnel and waves
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//
// Options:
// #define FRESNEL
// #define BOOST_SUN

uniform sampler2D mirrorMap;
uniform float time; // in seconds
varying vec4 mirrorCoord;
varying vec3 worldPos;
uniform vec3 worldEyePos;


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

void main()
{
	// mirror reflection
	vec2 uv = vec2(0.5,0.5)+vec2(0.5,-0.5)*mirrorCoord.xy/mirrorCoord.w;

	// waves
	vec3 worldNormal = vec3(
		0.003*sin(worldPos.x*6.3+time*5.4) + 0.002*sin(worldPos.x*33.0+worldPos.z*46.0+time*9.3),
		1.0,
		0.080*sin(worldPos.z*8.0+time*3.3) + 0.030*sin(worldPos.x*53.0+worldPos.z*33.0+time*12.3));

	// antialiasing
	worldNormal.xz *= min(0.03/length(dFdy(worldPos.xz)),1.0);

	vec4 env = texture2D(mirrorMap,uv+worldNormal.xz);

#ifdef FRESNEL
	float refl=reflectance(normalize(worldEyePos-worldPos),normalize(worldNormal));
#ifdef BOOST_SUN
	// correction of sun intensity
	if(env.x>0.96 && env.y>0.93 && env.z>0.9)
	{
		gl_FragColor = env;
	}
	else
#endif
	{
		gl_FragColor = refl*env+(1.0-refl)*vec4(0.1,0.25,0.35,0.0);
	}
#else
	gl_FragColor = env;
#endif

}
