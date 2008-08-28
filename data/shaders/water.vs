// Water with planar reflection, fresnel and waves
// Copyright (C) Stepan Hrbek, Lightsprint 2007-2008

varying vec4 mirrorCoord;
varying vec3 worldPos;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	mirrorCoord = gl_Position;
	worldPos = gl_Vertex.xyz;
}
