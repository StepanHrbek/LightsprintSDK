// Sky
// Copyright (C) Stepan Hrbek, Lightsprint 2007

uniform samplerCube cube;
varying vec3 dir;

void main()
{
	gl_FragColor = textureCube(cube,dir);
}
