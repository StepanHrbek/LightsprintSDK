// LightsprintGL: Lightmap build
// Used by RRIlluminationPixelBufferInOpenGL
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2010

varying vec4 color;

void main()
{
	gl_Position = gl_Vertex;
	color = gl_Color;
}
