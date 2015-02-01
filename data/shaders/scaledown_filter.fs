// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// DDI scale down filter
//
// Options:
// #define SIZEX n
// #define SIZEY n

uniform sampler2D lightmap;
uniform vec2 pixelDistance;
varying vec2 lightmapCoord;

void main()
{
	gl_FragColor = (
#if SIZEX==4 && SIZEY==4
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,+1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.5,-1.5))
		) * 0.1;
#endif
#if SIZEX==8 && SIZEY==8
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,+3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,+2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,+1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,+2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,+1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-2.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,+1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,+0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-0.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-0.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+0.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.5,-1.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+2.5,-2.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+2.5,-3.5))
		+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+3.5,-3.5))
		) * 0.0277778;
#endif
}
