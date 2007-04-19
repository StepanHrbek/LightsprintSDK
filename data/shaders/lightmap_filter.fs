// LightsprintGL: Lightmap postprocess
// Used by RRIlluminationPixelBufferInOpenGL
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

uniform sampler2D lightmap;
uniform vec2 pixelDistance;
varying vec2 lightmapCoord;

void main()
{
	vec4 c = texture2D(lightmap, lightmapCoord);
	if(c.a>=0.99)
	{
		gl_FragColor = c;
	}
	else
	{
		vec4 c1 = texture2D(lightmap, lightmapCoord-vec2(pixelDistance.x,0));
		vec4 c2 = texture2D(lightmap, lightmapCoord-vec2(0,pixelDistance.y));
		vec4 c3 = texture2D(lightmap, lightmapCoord+vec2(pixelDistance.x,0));
		vec4 c4 = texture2D(lightmap, lightmapCoord+vec2(0,pixelDistance.y));
		vec4 c5 = texture2D(lightmap, lightmapCoord-pixelDistance);
		vec4 c6 = texture2D(lightmap, lightmapCoord+pixelDistance);
		vec4 c7 = texture2D(lightmap, lightmapCoord+vec2(pixelDistance.x,-pixelDistance.y));
		vec4 c8 = texture2D(lightmap, lightmapCoord+vec2(-pixelDistance.x,pixelDistance.y));

		vec4 cc = 1.3*( c + 0.25*(c1+c2+c3+c4) + 0.166*(c5+c6+c7+c8) );

		gl_FragColor = cc/max(cc.a,1.0);
	}
}
