// LightsprintGL: Bloom effect
// Copyright (C) 2010-2012 Stepan Hrbek, Lightsprint
//
// Options:
// #define PASS [1|2|3]

uniform sampler2D map;
uniform vec2 pixelDistance;
varying vec2 mapCoord;

void main()
{
#if PASS==1
	// downscale source image
	vec4 color = (
		+texture2D(map,mapCoord+pixelDistance*vec2(-1.0,-1.0))
		+texture2D(map,mapCoord+pixelDistance*vec2(-1.0,+1.0))
		+texture2D(map,mapCoord+pixelDistance*vec2(+1.0,-1.0))
		+texture2D(map,mapCoord+pixelDistance*vec2(+1.0,+1.0))
		) / 4.0;
//	gl_FragColor = step(vec4(1.0,1.0,1.0,1.0),color);
	gl_FragColor = (color.r+color.g+color.b>=3.0)?color:vec4(0.0,0.0,0.0,0.0);
#elif PASS==2
	// blur downscaled image
	gl_FragColor = (
		+texture2D(map,mapCoord-8.5*pixelDistance)*0.2
		+texture2D(map,mapCoord-6.5*pixelDistance)*0.4
		+texture2D(map,mapCoord-4.5*pixelDistance)*0.6
		+texture2D(map,mapCoord-2.5*pixelDistance)*0.8
		+texture2D(map,mapCoord-0.5*pixelDistance)
		+texture2D(map,mapCoord+1.5*pixelDistance)*0.9
		+texture2D(map,mapCoord+3.5*pixelDistance)*0.7
		+texture2D(map,mapCoord+5.5*pixelDistance)*0.5
		+texture2D(map,mapCoord+7.5*pixelDistance)*0.3
		+texture2D(map,mapCoord+9.5*pixelDistance)*0.1
		) / 5.5;
#else
	// blend blurred image into source
	gl_FragColor = texture2D(map,mapCoord);
#endif
}
