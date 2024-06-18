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
in vec2 lightmapCoord;
out vec4 fragColor;

void main()
{
	fragColor =
#if SIZEX==4 && SIZEY==4
		+ 0.4 * (
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-1.0))
			)
		+ 0.3 * (
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,+1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-1.0))
			);
#endif
#if SIZEX==8 && SIZEY==8
		+ 4.0 / 36.0 * (
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,-3.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,-1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,+1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-3.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,-1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-3.0))
			)
		+ 3.0 / 36.0 * (
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-3.0,+3.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(-1.0,+1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(+1.0,-1.0))
			+texture(lightmap,lightmapCoord+pixelDistance*vec2(+3.0,-3.0))
			);
#endif
}
