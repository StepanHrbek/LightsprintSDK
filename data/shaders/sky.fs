// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//
// Options:
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA

uniform samplerCube cube;
varying vec3 dir;

#ifdef POSTPROCESS_BRIGHTNESS
	uniform vec4 postprocessBrightness;
#endif

#ifdef POSTPROCESS_GAMMA
	uniform float postprocessGamma;
#endif

void main()
{
	gl_FragColor = textureCube(cube,dir);

#ifdef POSTPROCESS_BRIGHTNESS
	gl_FragColor.rgb *= postprocessBrightness.rgb;
#endif

#ifdef POSTPROCESS_GAMMA
	gl_FragColor.rgb = pow(gl_FragColor.rgb,vec3(postprocessGamma,postprocessGamma,postprocessGamma));
#endif

}
