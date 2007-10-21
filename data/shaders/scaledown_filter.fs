// LightsprintGL: Scale down filter
// Used by RRDynamicSolverGL::detectDirectIllumination()
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007
//
// Parameters:
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
