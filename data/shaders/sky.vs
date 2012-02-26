// Sky
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
//  #define PROJECTION_CUBE
//  #define PROJECTION_EQUIRECTANGULAR
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA

varying vec3 dir;

void main()
{
	gl_Position = gl_Vertex;
	dir = (gl_ModelViewProjectionMatrixInverse * gl_Vertex).xyz;
}
