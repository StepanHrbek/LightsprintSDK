#ifndef UBERPROGRAMSETUP_H
#define UBERPROGRAMSETUP_H


/////////////////////////////////////////////////////////////////////////////
//
// UberProgramSetup - options for UberShader.vp+fp

struct UberProgramSetup
{
	// these values are passed to UberProgram which sets them to shader #defines
	// UberProgram[UberProgramSetup] = Program
	unsigned SHADOW_MAPS            :8;
	unsigned SHADOW_SAMPLES         :8;
	bool     LIGHT_DIRECT           :1;
	bool     LIGHT_DIRECT_MAP       :1;
	bool     LIGHT_INDIRECT_COLOR   :1;
	bool     LIGHT_INDIRECT_MAP     :1;
	bool     MATERIAL_DIFFUSE_COLOR :1;
	bool     MATERIAL_DIFFUSE_MAP   :1;
	bool     FORCE_2D_POSITION      :1;

	UberProgramSetup()
	{
		memset(this,0,sizeof(*this));
	}

	const char* getSetupString()
	{
		static char setup[300];
		sprintf(setup,"#define SHADOW_MAPS %d\n#define SHADOW_SAMPLES %d\n%s%s%s%s%s%s%s",
			SHADOW_MAPS,
			SHADOW_SAMPLES,
			LIGHT_DIRECT?"#define LIGHT_DIRECT\n":"",
			LIGHT_DIRECT_MAP?"#define LIGHT_DIRECT_MAP\n":"",
			LIGHT_INDIRECT_COLOR?"#define LIGHT_INDIRECT_COLOR\n":"",
			LIGHT_INDIRECT_MAP?"#define LIGHT_INDIRECT_MAP\n":"",
			MATERIAL_DIFFUSE_COLOR?"#define MATERIAL_DIFFUSE_COLOR\n":"",
			MATERIAL_DIFFUSE_MAP?"#define MATERIAL_DIFFUSE_MAP\n":"",
			FORCE_2D_POSITION?"#define FORCE_2D_POSITION\n":""
			);
		return setup;
	}

	bool operator ==(const UberProgramSetup& a) const
	{
		return memcmp(this,&a,sizeof(*this))==0;
	}
	bool operator !=(const UberProgramSetup& a) const
	{
		return memcmp(this,&a,sizeof(*this))!=0;
	}
};

#endif
