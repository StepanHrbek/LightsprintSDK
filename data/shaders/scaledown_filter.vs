// LightsprintGL: Scale down filter
// Used by detectDirectIllumination()
// Copyright (C) 2006-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define SIZEX n
// #define SIZEY n

attribute vec2 vertexPosition;
varying vec2 lightmapCoord;

void main()
{
	lightmapCoord = vertexPosition;
	gl_Position = vec4((vertexPosition*2.0-vec2(1.0,1.0)),1.0,1.0);
}
