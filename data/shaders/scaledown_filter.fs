// Copyright (C) 1999-2021 Stepan Hrbek
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
	gl_FragColor =
#if SIZEX==4 && SIZEY==4
		+ 0.4 * (
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-1.0))
			)
		+ 0.3 * (
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,+1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-1.0))
			);
#endif
#if SIZEX==8 && SIZEY==8
		+ 4.0 / 36.0 * (
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,-3.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,-1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,+1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-3.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-3.0))
			)
		+ 3.0 / 36.0 * (
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,+3.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,+1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-1.0))
			+texture2D(lightmap,lightmapCoord+pixelDistance*vec2(+3.0,-3.0))
			);
#endif
}
