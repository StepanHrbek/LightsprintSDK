// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
//
// Options:
//  #define PROJECTION_CUBE
//  #define PROJECTION_EQUIRECTANGULAR
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA

varying vec3 dir;

#ifdef PROJECTION_CUBE
	uniform samplerCube map;
#endif

#ifdef PROJECTION_EQUIRECTANGULAR
	uniform sampler2D map;
	uniform vec4 shape;
#endif

#ifdef POSTPROCESS_BRIGHTNESS
	uniform vec3 postprocessBrightness;
#endif

#ifdef POSTPROCESS_GAMMA
	uniform float postprocessGamma;
#endif

void main()
{
#ifdef PROJECTION_CUBE
	gl_FragColor = textureCube(map,dir);
#endif

#ifdef PROJECTION_EQUIRECTANGULAR
	float PI = 3.14159265358979323846264;
	vec2 angle = vec2(asin(normalize(dir.xz).x),asin(normalize(dir).y));
	if (dir.z<0.0) angle.x = PI-angle.x;
	gl_FragColor = texture2D(map,angle*shape.xy+shape.zw);
#endif

#ifdef POSTPROCESS_BRIGHTNESS
	gl_FragColor.rgb *= postprocessBrightness;
#endif

#ifdef POSTPROCESS_GAMMA
	gl_FragColor.rgb = pow(gl_FragColor.rgb,vec3(postprocessGamma,postprocessGamma,postprocessGamma));
#endif

}
