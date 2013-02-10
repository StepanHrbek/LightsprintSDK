// LightsprintGL: Depth of field effect
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint

varying vec2 mapCoord;

void main()
{
	mapCoord = gl_Vertex.xy*0.5+vec2(0.5,0.5);
	gl_Position = gl_Vertex;
}