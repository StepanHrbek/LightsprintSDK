#include <cassert>
#include "AnimationFrame.h"
#include "Lightsprint/RRDebug.h"
#include <GL/glew.h>

/////////////////////////////////////////////////////////////////////////////
//
// AnimationFrame, one animation frame in editable form

AnimationFrame::AnimationFrame(AnimationFrame& copy) :
	eye(copy.eye),
	light(copy.light)
{
	brightness = copy.brightness;
	gamma = copy.gamma;
	projectorIndex = copy.projectorIndex;
	dynaPosRot = copy.dynaPosRot;
	transitionToNextTime = copy.transitionToNextTime;
	layerNumber = copy.layerNumber;
	thumbnail = NULL; // don't duplicate pointer, would be deleted twice
	strcpy(overlayFilename,copy.overlayFilename);
	overlaySeconds = copy.overlaySeconds;
	overlayMode = copy.overlayMode;
	overlayMap = copy.overlayMap; copy.overlayMap = NULL; // don't duplicate pointer, would be deleted twice
}

AnimationFrame::AnimationFrame(unsigned alayerNumber) :
	eye(0,1,4, 2.935000f,0,-0.7500f, 1,100,0.3f,900),
	light(-1.233688f,3.022499f,-0.542255f, 1.239998f,0,6.649996f, 1,70,1,1000)
{
	brightness = rr::RRVec4(1);
	gamma = 1;
	transitionToNextTime = 3;
	projectorIndex = 0;
	layerNumber = alayerNumber;
	thumbnail = NULL;
	overlayFilename[0] = 0;
	overlaySeconds = 5;
	overlayMode = 0;
	overlayMap = NULL;
}

AnimationFrame::~AnimationFrame()
{
	delete overlayMap;
	delete thumbnail;
}

// returns a*(1-alpha) + b*alpha; (a and b are points on 360degree circle)
// using shortest path between a and b
rr::RRReal blendModulo(rr::RRReal a,rr::RRReal b,rr::RRReal alpha,rr::RRReal modulo)
{
	a = fmodf(a,modulo);
	b = fmodf(b,modulo);
	if(a<b-(modulo/2)) a += modulo; else
	if(a>b+(modulo/2)) a -= modulo;
	return a*(1-alpha) + b*alpha;
}

rr::RRVec2 blendModulo(rr::RRVec2 a,rr::RRVec2 b,rr::RRReal alpha,rr::RRReal modulo)
{
	rr::RRVec2 tmp;
	for(unsigned i=0;i<2;i++)
	{
		tmp[i] = blendModulo(a[i],b[i],alpha,modulo);
	}
	return tmp;
}

// returns blend between this and that frame
// return this for alpha=0, that for alpha=1
const AnimationFrame* AnimationFrame::blend(const AnimationFrame& that, float alpha) const
{
	static AnimationFrame blended(0);
	// blend eye+light
	float* a = (float*)(&this->eye);
	float* b = (float*)(&that.eye);
	float* c = (float*)(&blended.eye);
	for(unsigned i=0;i<(sizeof(eye)+sizeof(light)+sizeof(brightness)+sizeof(gamma))/sizeof(float);i++)
		c[i] = a[i]*(1-alpha) + b[i]*alpha;
	blended.eye.angle = blendModulo(this->eye.angle,that.eye.angle,alpha,(float)(2*M_PI));
	blended.light.angle = blendModulo(this->light.angle,that.light.angle,alpha,(float)(2*M_PI));
	blended.eye.update(0);
	blended.light.update(0.3f);
	// blend dynaPosRot
	blended.dynaPosRot.clear();
	for(unsigned i=0;i<this->dynaPosRot.size() && i<that.dynaPosRot.size();i++)
	{
		DynaObjectPosRot tmp;
		tmp.pos = this->dynaPosRot[i].pos*(1-alpha) + that.dynaPosRot[i].pos*alpha;
		tmp.rot = blendModulo(this->dynaPosRot[i].rot,that.dynaPosRot[i].rot,alpha,360);
		blended.dynaPosRot.push_back(tmp);
	}
	// blend projectorIndex
	blended.projectorIndex = projectorIndex;
	// blend thumbnail
	blended.thumbnail = NULL;
	// blend overlay
	blended.overlayFilename[0] = 0;
	blended.overlaySeconds = this->overlaySeconds;
	blended.overlayMap = NULL;//this->overlayMap;
	return &blended;
}

// load frame from opened .ani file
AnimationFrame* AnimationFrame::loadNew(FILE* f)
{
	AnimationFrame* tmp = new AnimationFrame(0);
	if(!tmp->loadOver(f))
		SAFE_DELETE(tmp);
	return tmp;
}

bool AnimationFrame::loadOver(FILE* f)
{
	if(!f) return false;
	// load layerNumber
	bool loaded = false; // nothing loaded yet
	fscanf(f,"layer_number = %d\n",&layerNumber);
	//if(1!=fscanf(f,"layer_number = %d\n",&layerNumber))
	//	return false;
	// load eye+light
	if(fscanf(f,"camera = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&eye.pos[0],&eye.pos[1],&eye.pos[2],&eye.angle,&eye.leanAngle,&eye.angleX,&eye.aspect,&eye.fieldOfView,&eye.anear,&eye.afar)==10) loaded = true;
	if(fscanf(f,"light = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&light.pos[0],&light.pos[1],&light.pos[2],&light.angle,&light.leanAngle,&light.angleX,&light.aspect,&light.fieldOfView,&light.anear,&light.afar)==10) loaded = true;
	if(fscanf(f,"brightness = {%f,%f,%f,%f}\n",&brightness[0],&brightness[1],&brightness[2],&brightness[3])==4) loaded = true;
	if(fscanf(f,"gamma = %f\n",&gamma)==1) loaded = true;
	// load dynaPosRot
	dynaPosRot.clear();
	DynaObjectPosRot tmp;
	while(5==fscanf(f,"object = {%f,%f,%f,%f,%f}\n",&tmp.pos[0],&tmp.pos[1],&tmp.pos[2],&tmp.rot[0],&tmp.rot[1]))
	{
		dynaPosRot.push_back(tmp);
		loaded = true;
	}
	// load projectorIndex
	if(fscanf(f,"projector = %d\n",&projectorIndex)==1) loaded = true;
	// load overlay
	overlayFilename[0] = 0;
	if(fscanf(f,"2d_overlay = %f,%d,%s\n",&overlaySeconds,&overlayMode,overlayFilename)==2) loaded = true;
	overlayMap = overlayFilename[0] ? rr_gl::Texture::load(overlayFilename, NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP) : NULL;
	// load timing
	if(fscanf(f,"duration = %f\n",&transitionToNextTime)==1) loaded = true;
	//if(0!=fscanf(f,"\n"))
	//	return false;
	//rr::RRReporter::report(rr::INF1,"  frame with %d objects\n",dynaPosRot.size());
	return loaded;
}

void AnimationFrame::validate(unsigned numObjects)
{
	// generate object positions when objects were added by hand
	while(dynaPosRot.size()<numObjects)
	{
		// create positions when objects were added by hand into scene
		DynaObjectPosRot tmp;
		dynaPosRot.push_back(tmp);
	}
}

// save frame to opened .ani file
bool AnimationFrame::save(FILE* f, const AnimationFrame& prev) const
{
	if(!f) return false;
	// save layer number
	fprintf(f,"layer_number = %d\n",layerNumber);
	// save eye+light
	if(!(eye==prev.eye))
		fprintf(f,"camera = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eye.pos[0],eye.pos[1],eye.pos[2],fmodf(eye.angle+100*3.14159265f,2*3.14159265f),eye.leanAngle,eye.angleX,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
	if(!(light==prev.light))
		fprintf(f, "light = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",light.pos[0],light.pos[1],light.pos[2],fmodf(light.angle+100*3.14159265f,2*3.14159265f),light.leanAngle,light.angleX,light.aspect,light.fieldOfView,light.anear,light.afar);
	if(brightness!=prev.brightness)//brightness[0]!=1 || brightness[1]!=1 || brightness[2]!=1 || brightness[3]!=1)
		fprintf(f,"brightness = {%f,%f,%f,%f}\n",brightness[0],brightness[1],brightness[2],brightness[3]);
	if(gamma!=prev.gamma)//gamma!=1)
		fprintf(f,"gamma = %f\n",gamma);
	// save dynaPosRot
	for(DynaPosRot::const_iterator i=dynaPosRot.begin();i!=dynaPosRot.end();i++)
		fprintf(f,"object = {%.3f,%.3f,%.3f,%.3f,%.3f}\n",(*i).pos[0],(*i).pos[1],(*i).pos[2],(*i).rot[0],(*i).rot[1]);
	// save projectorIndex
	if(projectorIndex!=prev.projectorIndex)
		fprintf(f,"projector = %d\n",projectorIndex);
	// save overlay
	if(overlayFilename[0])
		fprintf(f,"2d_overlay = %.3f,%d,%s\n",overlaySeconds,overlayMode,overlayFilename);
	// save timing
	if(transitionToNextTime!=prev.transitionToNextTime)
		fprintf(f,"duration = %.3f\n",transitionToNextTime);
	fprintf(f,"\n");
	//rr::RRReporter::report(rr::INF1,"  frame with %d objects\n",dynaPosRot.size());
	return true;
}
