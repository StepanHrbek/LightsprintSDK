// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Bloom effect

attribute vec2 vertexPosition;
varying vec2 mapCoord;

void main()
{
	mapCoord = vertexPosition*0.5+vec2(0.5,0.5);
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
