// LightsprintGL: Scale down filter
// Used by detectDirectIllumination()
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint
//
// Options:
// #define SIZEX n
// #define SIZEY n

varying vec2 lightmapCoord;

void main()
{
	lightmapCoord = gl_MultiTexCoord0.xy;
	gl_Position = gl_Vertex;
}
