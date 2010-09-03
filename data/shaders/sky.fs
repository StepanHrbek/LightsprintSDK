// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
//
// Options:
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA
//  #define BLEND

uniform samplerCube cube0;
varying vec3 dir;

#ifdef POSTPROCESS_BRIGHTNESS
	uniform vec4 postprocessBrightness;
#endif

#ifdef POSTPROCESS_GAMMA
	uniform float postprocessGamma;
#endif

#ifdef BLEND
	uniform samplerCube cube1;
	uniform float blendFactor;
	uniform float gamma0;
	uniform float gamma1;
#endif

void main()
{
#ifdef BLEND
	vec3 color0 = pow(textureCube(cube0,dir).rgb,vec3(gamma0,gamma0,gamma0)); // convert pixels from both LDR or HDR skyboxes to physical scale
	vec3 color1 = pow(textureCube(cube1,dir).rgb,vec3(gamma1,gamma1,gamma1));
	gl_FragColor.rgb = color0*(1.0-blendFactor)+color1*blendFactor; // to make this blending gamma correct
#else
	gl_FragColor = textureCube(cube0,dir);
#endif

#ifdef POSTPROCESS_BRIGHTNESS
	gl_FragColor.rgb *= postprocessBrightness.rgb;
#endif

#ifdef POSTPROCESS_GAMMA
	gl_FragColor.rgb = pow(gl_FragColor.rgb,vec3(postprocessGamma,postprocessGamma,postprocessGamma));
#endif

}
