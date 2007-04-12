// LightsprintGL: Scale down filter
// Used by RRDynamicSolverGL::detectDirectIllumination()
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

uniform sampler2D lightmap;
uniform vec2 pixelDistance;
varying vec2 lightmapCoord;

void main()
{
	gl_FragColor = (
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
}
