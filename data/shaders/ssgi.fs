// LightsprintGL: Screen space ambient occlusion with color bleeding
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint
//
// Options:
// #define PASS [1|2]

uniform sampler2D colorMap;
uniform sampler2D depthMap;
uniform vec2 pixelSize; // from resolution
varying vec2 mapCoord;

#if PASS==1

uniform vec2 depthRange; // from camera: near*far/(far-near), far/(far-near)
uniform float aoRange; // user defined
uniform float occlusionIntensity; // user defined
uniform float angleBias; // user defined
uniform vec2 aoRangeInTextureSpaceIn1mDistance; // from resolution and camera

#define NUM_STEPS 8
#define NUM_DIRS 4

void main()
{
	float noise = sin(dot(mapCoord,vec2(52.714,112.9898))) * 43758.5453;
	vec2 noiseSinCos = vec2(sin(noise),cos(noise));
	vec2 dir[NUM_DIRS];
	dir[0] = noiseSinCos;
	dir[1] = -noiseSinCos;
	dir[2] = vec2(noiseSinCos.y,-noiseSinCos.x);
	dir[3] = vec2(-noiseSinCos.y,noiseSinCos.x);

	// get normal
	vec3 c0   = texture2D(colorMap,mapCoord).rgb;
	vec3 cx_2 = texture2D(colorMap,vec2(mapCoord.x-2.0*pixelSize.x,mapCoord.y)).rgb;
	vec3 cx_1 = texture2D(colorMap,vec2(mapCoord.x-pixelSize.x,mapCoord.y)).rgb;
	vec3 cx1  = texture2D(colorMap,vec2(mapCoord.x+pixelSize.x,mapCoord.y)).rgb;
	vec3 cx2  = texture2D(colorMap,vec2(mapCoord.x+2.0*pixelSize.x,mapCoord.y)).rgb;
	vec3 cy_2 = texture2D(colorMap,vec2(mapCoord.x,mapCoord.y-2.0*pixelSize.y)).rgb;
	vec3 cy_1 = texture2D(colorMap,vec2(mapCoord.x,mapCoord.y-pixelSize.y)).rgb;
	vec3 cy1  = texture2D(colorMap,vec2(mapCoord.x,mapCoord.y+pixelSize.y)).rgb;
	vec3 cy2  = texture2D(colorMap,vec2(mapCoord.x,mapCoord.y+2.0*pixelSize.y)).rgb;
	float z0   = texture2D(depthMap,mapCoord).z;
	//float zx_2 = texture2D(depthMap,mapCoord-vec2(pixelSize.x*2.0,0.0)).z;
	float zx_1 = texture2D(depthMap,mapCoord-vec2(pixelSize.x,0.0)).z;
	float zx1  = texture2D(depthMap,mapCoord+vec2(pixelSize.x,0.0)).z;
	//float zx2  = texture2D(depthMap,mapCoord+vec2(pixelSize.x*2.0,0.0)).z;
	//float zy_2 = texture2D(depthMap,mapCoord-vec2(0.0,pixelSize.y*2.0)).z;
	float zy_1 = texture2D(depthMap,mapCoord-vec2(0.0,pixelSize.y)).z;
	float zy1  = texture2D(depthMap,mapCoord+vec2(0.0,pixelSize.y)).z;
	//float zy2  = texture2D(depthMap,mapCoord+vec2(0.0,pixelSize.y*2.0)).z;
	float linearz0   = depthRange.x / (depthRange.y - z0);
	// a) 50% of edge 2x2 pixels assigned to wrong plane
	//vec2 dz = vec2(dFdx(z0),dFdy(z0));
	// b) 50% of edge pixels assigned to wrong plane
	//vec2 dz = vec2(abs(z0-zx_1)<abs(zx1-z0)?z0-zx_1:zx1-z0,abs(z0-zy_1)<abs(zy1-z0)?z0-zy_1:zy1-z0);
	// c) ~1% of edge pixels assigned to wrong plane
	//vec2 dz = vec2(abs(z0+zx_2-2.0*zx_1)<abs(z0+zx2-2.0*zx1)?z0-zx_1:zx1-z0,abs(z0+zy_2-2.0*zy_1)<abs(z0+zy2-2.0*zy1)?z0-zy_1:zy1-z0);
	// d) <<1% of edge pixels assigned to wrong plane (more when plane's colors don't differ, but then it's much less serious error)
	vec2 dz = vec2(length(c0+cx_2-2.0*cx_1)<length(c0+cx2-2.0*cx1)?z0-zx_1:zx1-z0,length(c0+cy_2-2.0*cy_1)<length(c0+cy2-2.0*cy1)?z0-zy_1:zy1-z0);
	// e) all edge pixels would get correct normal only if we prerender normals into separated texture

/*

!steny ubihajici do dali tmavnou, ve velky dalce zas zesvetlaj
 near/far nema vliv
 2x vetsi radius posune problem do 2x vetsi dalky

!prouzky pri pohledu shora
  kdybych att pocital ne z r ale z toho o kolik se r zvysilo od minula, mozna by ubylo prouzku pri pohledu shora
  !zatim nefunguje, naopak barva z kostky vytejka i nahoru, tj. na vzdaleny pozadi

*/

	float sumOcclusion = 0.0;
	vec4 occlusionColor = vec4(0.00001);
	for (int d=0;d<NUM_DIRS;d++)
	{
		float maxOcclusion = 0.0;
		vec2 stepInTextureSpace = aoRangeInTextureSpaceIn1mDistance/(linearz0*float(NUM_STEPS))*dir[d]; //! je kruh, ma byt elipsa, zohlednit dz (if dz==0 step nechat, jinak zmensit)
		float stepInZ = dot(stepInTextureSpace/pixelSize,dz);
		for (int s=1;s<=NUM_STEPS;s++)
		{
			float linearz1 = depthRange.x / (depthRange.y - texture2D(depthMap,mapCoord+float(s)*stepInTextureSpace).z + float(s)*stepInZ);
			float r = (linearz0-linearz1)/aoRange;
			float att = max(1.0-r*r,0.0);
			float occlusion = r*att*float(NUM_STEPS)/float(s)-angleBias;
			if (occlusion>maxOcclusion)
			{
				vec4 c1 = texture2D(colorMap,mapCoord+float(s)*stepInTextureSpace);
				occlusionColor += c1*(occlusion-maxOcclusion);
				maxOcclusion = occlusion;
			}
		}
		//sumOcclusion += maxOcclusion*0.8; // dela strmejsi gradient, svetlejsi kraje, tmavsi kouty. mohlo by dobre vyvazovat blur kterej kouty zesvetli
		sumOcclusion += atan(maxOcclusion); // atan(x) zajistuje mirnejsi gradient, ne tak tmavy kouty jako pri x
	}
	gl_FragColor = vec4(1.0)-occlusionIntensity/float(NUM_DIRS)*sumOcclusion*(vec4(1.0)-occlusionColor/occlusionColor.w);
	// alpha must be 1, following blur pass expects it
}

#else // PASS==2

uniform sampler2D aoMap;

void main()
{
	// blur AO
//	vec4 c_3 = texture2D(colorMap,mapCoord-3.0*pixelSize);
	vec4 c_2 = texture2D(colorMap,mapCoord-2.0*pixelSize);
	vec4 c_1 = texture2D(colorMap,mapCoord-pixelSize);
	vec4 c0  = texture2D(colorMap,mapCoord);
	vec4 c1  = texture2D(colorMap,mapCoord+pixelSize);
	vec4 c2  = texture2D(colorMap,mapCoord+2.0*pixelSize);
//	vec4 c3  = texture2D(colorMap,mapCoord+3.0*pixelSize);

	vec4 a_3 = texture2D(aoMap,mapCoord-3.0*pixelSize);
	vec4 a_2 = texture2D(aoMap,mapCoord-2.0*pixelSize);
	vec4 a_1 = texture2D(aoMap,mapCoord-pixelSize);
	vec4 a0  = texture2D(aoMap,mapCoord);
	vec4 a1  = texture2D(aoMap,mapCoord+pixelSize);
	vec4 a2  = texture2D(aoMap,mapCoord+2.0*pixelSize);
	vec4 a3  = texture2D(aoMap,mapCoord+3.0*pixelSize);

	float z_3 = texture2D(depthMap,mapCoord-3.0*pixelSize).z;
	float z_2 = texture2D(depthMap,mapCoord-2.0*pixelSize).z;
	float z_1 = texture2D(depthMap,mapCoord-pixelSize).z;
	float z0  = texture2D(depthMap,mapCoord).z;
	float z1  = texture2D(depthMap,mapCoord+pixelSize).z;
	float z2  = texture2D(depthMap,mapCoord+2.0*pixelSize).z;
	float z3  = texture2D(depthMap,mapCoord+3.0*pixelSize).z;

	float dz_3 = (z_2-z_3);
	float dz_2 = (z_1-z_2);
	float dz_1 = (z0-z_1);
	float dz1  = (z1-z0);
	float dz2  = (z2-z1);
	float dz3  = (z3-z2);

	float dz = length((c0+c_2-2.0*c_1).rgb)<length((c0+c2-2.0*c1).rgb)?dz_1:dz1;
	float dzepsilon = abs(dz*0.1);
	
	vec4 col = 1.4*a0;
	if (abs(dz_1-dz)<=dzepsilon)
	{
		col += a_1;
		if (abs(dz_2-dz)<=dzepsilon)
		{
			col += 0.7*a_2;
			if (abs(dz_3-dz)<=dzepsilon)
				col += 0.4*a_3;
		}
	}
	if (abs(dz1-dz)<=dzepsilon)
	{
		col += a1;
		if (abs(dz2-dz)<=dzepsilon)
		{
			col += 0.7*a2;
			if (abs(dz3-dz)<=dzepsilon)
				col += 0.4*a3;
		}
	}
	gl_FragColor = col / col.w;
}

#endif

/*

!AO aplikuju i na leskly a emisivni matrose
  a) pridat extra render sceny do textury, A=dif/(dif+spec+emi)=jak moc je pixel citlivej na AO

Q: jaky pro/proti ma prechod na texturu s normalama
A: -pomalejsi a slozitejsi protoze musim vyrobit texturu s normalama
     a) extra render sceny jednoduchym shaderem gl_FragColor = (normal+vec4(1.0))*0.5; (Z textura, normal textura)
	    (neeliminuje kopirovani Z ani COLOR, ty musim dal kopirovat az po final renderu)
	 b) ve final rendru mit dva render targety, do druhyho zapsat normalu
		jde od GL 2, GLES 3
		-nejde v GL ES 2.0
		 (ale vadi to? stejne bude na mobilu nepouzitelne pomaly)
   +rychlejsi tim ze PASS 1 muze jet do halfresu, naslednej fullres blur zahladi badpixely u hran
   +zmizej badpixely na hranach u kterejch ted nejde urcit do kteryho facu patrej
   +pujde blurovat aniz by barva pretejkala pres hranu
     ?jak poznam ze barva nema pretejkat?
	   dovolit blur jen kdyz je podobna normala a podobny z (3 lookupy na kazdej pixel v kernelu)
	 ?jak moc bude vadit kdyz blur separuju do X a Y passu?
	   skoro vubec

*/
