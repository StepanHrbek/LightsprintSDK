// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Sky
//
// Options:
//  #define PROJECTION_CUBE
//  #define PROJECTION_EQUIRECTANGULAR
//  #define POSTPROCESS_BRIGHTNESS
//  #define POSTPROCESS_GAMMA

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
out vec3 dir;

void main()
{
	gl_Position = vec4(vertexPosition,1.0,1.0);
	dir = vertexNormal;
}
