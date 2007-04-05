// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007

uniform samplerCube cube;
uniform vec4 color;
varying vec3 dir;

void main()
{
	gl_FragColor = textureCube(cube,dir)*color;
}
