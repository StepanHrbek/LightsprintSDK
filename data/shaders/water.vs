// Water with planar reflection, fresnel and animated waves
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint
//
// Options:
// #define FRESNEL
// #define DIRLIGHT

varying vec4 mirrorCoord;
varying vec3 worldPos;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	mirrorCoord = gl_Position;
	worldPos = gl_Vertex.xyz;
}
