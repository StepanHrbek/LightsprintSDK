// LightsprintGL: Lightmap postprocess
// Used by RRIlluminationPixelBufferInOpenGL
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2010

varying vec2 lightmapCoord;

void main()
{
	lightmapCoord = gl_MultiTexCoord0.xy;
	gl_Position = gl_Vertex;
}
