// Lightmap postprocess
// Copyright (C) Lightsprint 2006

uniform sampler2D lightmap;
varying vec2 lightmapCoord;
uniform vec2 pixelDistance;

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
		vec4 d1 = texture2D(lightmap, lightmapCoord-vec2(2*pixelDistance.x,0));
		vec4 d2 = texture2D(lightmap, lightmapCoord-vec2(0,2*pixelDistance.y));
		vec4 d3 = texture2D(lightmap, lightmapCoord+vec2(2*pixelDistance.x,0));
		vec4 d4 = texture2D(lightmap, lightmapCoord+vec2(0,2*pixelDistance.y));

		vec4 cc = c1*c1.a + c2*c2.a + c3*c3.a + c4*c4.a + 0.66*( c5*c5.a + c6*c6.a + c7*c7.a + c8*c8.a ) + 0.33*( d1*d1.a + d2*d2.a + d3*d3.a + d4*d4.a );

		gl_FragColor = (cc.a==0.0) ? c : cc/cc.a;
	}
}
