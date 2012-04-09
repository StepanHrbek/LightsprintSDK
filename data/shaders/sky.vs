// Sky
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint
//
// Options:
//  #define PROJECTION_CUBE
//  #define PROJECTION_EQUIRECTANGULAR
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA

attribute vec2 vertexPosition;
attribute vec3 vertexDirection;
varying vec3 dir;

void main()
{
	gl_Position = vec4(vertexPosition,1.0,1.0);
	dir = vertexDirection;
}
