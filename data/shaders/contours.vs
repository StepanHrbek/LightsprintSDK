// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Contours

layout(location = 0) in vec2 vertexPosition;
out vec2 tMapCoord;

void main()
{
	tMapCoord = vertexPosition*0.5+vec2(0.5,0.5);
	gl_Position = vec4(vertexPosition,0.0,1.0);
}
