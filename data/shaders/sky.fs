// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//
// Options:
// #define PHYSICAL

uniform samplerCube cube;
uniform vec4 color;
varying vec3 dir;

void main()
{
	vec4 sample = textureCube(cube,dir);
#ifdef PHYSICAL
	sample = pow(sample,vec4(0.45,0.45,0.45,0.45));
#endif
	gl_FragColor = sample*color;
}
