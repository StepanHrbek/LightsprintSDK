// LightsprintGL: Lightmap postprocess
// Used by RRIlluminationPixelBufferInOpenGL
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2007

uniform sampler2D lightmap;
uniform vec2 pixelDistance;
varying vec2 lightmapCoord;

void main()
{
	vec4 c = texture2D(lightmap, lightmapCoord);
	if(c.a>=1.0)
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

		vec4 cc = c1*c1.a + c2*c2.a + c3*c3.a + c4*c4.a + 0.66*( c5*c5.a + c6*c6.a + c7*c7.a + c8*c8.a );

		gl_FragColor = (cc.a==0.0) ? c : cc/cc.a;
	}
}
