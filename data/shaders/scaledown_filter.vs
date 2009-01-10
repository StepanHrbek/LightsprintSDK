// LightsprintGL: Scale down filter
// Used by detectDirectIllumination()
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
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
