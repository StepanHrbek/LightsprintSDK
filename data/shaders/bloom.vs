// LightsprintGL: Bloom effect
// Copyright (C) Stepan Hrbek, Lightsprint 2010

varying vec2 mapCoord;

void main()
{
	mapCoord = gl_Vertex.xy*0.5+vec2(0.5,0.5);
	gl_Position = gl_Vertex;
}
