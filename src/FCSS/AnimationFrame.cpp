#include "AnimationFrame.h"
#include "Lightsprint/RRDebug.h"

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
	shadowType = copy.shadowType;
	indirectType = copy.indirectType;
	volume = copy.volume;
}

AnimationFrame::AnimationFrame(unsigned _layerNumber) :
	eye(rr::RRVec3(0,1,4), rr::RRVec3(2.935000f,-0.7500f,0), 1,100,0.3f,900),
	light(rr::RRVec3(-1.233688f,3.022499f,-0.542255f), rr::RRVec3(1.239998f,6.649996f,0), 1,70,1,1000)
{
	brightness = rr::RRVec4(1);
	gamma = 1;
	transitionToNextTime = 3;
	projectorIndex = 0;
	layerNumber = _layerNumber;
	thumbnail = NULL;
	overlayFilename[0] = 0;
	overlaySeconds = 5;
	overlayMode = 0;
	overlayMap = NULL;
	shadowType = 2; // soft
	indirectType = 2; // realtime per vertex
	volume = 1;
}

AnimationFrame::~AnimationFrame()
{
	delete overlayMap;
	delete thumbnail;
}

// returns a*(1-alpha) + b*alpha;
template<class C>
C blendNormal(C a,C b,rr::RRReal alpha)
{
	// don't interpolate between identical values,
	//  results would be slightly different for different alphas (in win64, not in win32)
	//  some optimizations are enabled only when data don't change between frames (see shadowcast.cpp bool lightChanged)
	return (a==b) ? a : (a*(1-alpha) + b*alpha);
}

// returns a*(1-alpha) + b*alpha; (a and b are points on 360degree circle)
// using shortest path between a and b
rr::RRReal blendModulo(rr::RRReal a,rr::RRReal b,rr::RRReal alpha,rr::RRReal modulo)
{
	a = fmodf(a,modulo);
	b = fmodf(b,modulo);
	if (a<b-(modulo/2)) a += modulo; else
	if (a>b+(modulo/2)) a -= modulo;
	return blendNormal(a,b,alpha);
}

rr::RRVec2 blendModulo(rr::RRVec2 a,rr::RRVec2 b,rr::RRReal alpha,rr::RRReal modulo)
{
	rr::RRVec2 tmp;
	for (unsigned i=0;i<2;i++)
	{
		tmp[i] = blendModulo(a[i],b[i],alpha,modulo);
	}
	return tmp;
}

// returns blend between this and that frame
// return this for alpha=0, that for alpha=1
// returns always the same static object
const AnimationFrame* AnimationFrame::blend(const AnimationFrame& that, float alphaSmooth, float alphaRounded) const
{
	static AnimationFrame blended(0);
	// blend eye+light
	blended.eye.blendLinear(this->eye,that.eye,alphaSmooth);
	blended.light.blendLinear(this->light,that.light,alphaRounded);
	blended.brightness = blendNormal(this->brightness,that.brightness,alphaRounded);
	blended.gamma = blendNormal(this->gamma,that.gamma,alphaRounded);
	// blend dynaPosRot
	blended.dynaPosRot.clear();
	for (unsigned i=0;i<this->dynaPosRot.size() && i<that.dynaPosRot.size();i++)
	{
		DynaObjectPosRot tmp;
		tmp.pos = blendNormal(this->dynaPosRot[i].pos,that.dynaPosRot[i].pos,alphaRounded);
		tmp.rot = blendModulo(this->dynaPosRot[i].rot,that.dynaPosRot[i].rot,alphaRounded,360);
		blended.dynaPosRot.push_back(tmp);
	}
	// blend volume
	blended.volume = blendNormal(this->volume,that.volume,alphaSmooth);
	// blend projectorIndex
	blended.projectorIndex = projectorIndex;
	// blend thumbnail
	blended.thumbnail = NULL; // don't duplicate map, would be deleted more than once
	// blend overlay
	blended.overlayFilename[0] = 0;
	blended.overlaySeconds = this->overlaySeconds;
	blended.overlayMap = NULL; // don't duplicate map, would be deleted more than once
	// technique
	blended.shadowType = this->shadowType;
	blended.indirectType = this->indirectType;
	// must be updated too (used to detect animation cuts, needImmediateDDI is set when layerNumber changes)
	blended.layerNumber = this->layerNumber;
	return &blended;
}

// load frame from opened .ani file
AnimationFrame* AnimationFrame::loadNew(FILE* f)
{
	AnimationFrame* tmp = new AnimationFrame(0);
	if (!tmp->loadOver(f))
		RR_SAFE_DELETE(tmp);
	return tmp;
}

bool AnimationFrame::loadOver(FILE* f)
{
	if (!f) return false;
	// load layerNumber
	bool loaded = false; // nothing loaded yet
	fscanf(f,"layer_number = %d\n",&layerNumber);
	//if (1!=fscanf(f,"layer_number = %d\n",&layerNumber))
	//	return false;
	// load eye+light
	float aspect, fov, anear, afar;
	rr::RRVec3 pos,yawPitchRollRad;
	if (fscanf(f,"camera = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&pos[0],&pos[1],&pos[2],&yawPitchRollRad[0],&yawPitchRollRad[2],&yawPitchRollRad[1],&aspect,&fov,&anear,&afar)==10)
	{
		yawPitchRollRad[0] += RR_PI;
		eye.setPosition(pos);
		eye.setYawPitchRollRad(yawPitchRollRad);
		eye.setAspect(aspect);
		eye.setFieldOfViewVerticalDeg(fov);
		eye.setRange(anear,afar);
		loaded = true;
	}
	if (fscanf(f,"light = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&pos[0],&pos[1],&pos[2],&yawPitchRollRad[0],&yawPitchRollRad[2],&yawPitchRollRad[1],&aspect,&fov,&anear,&afar)==10)
	{
		yawPitchRollRad[0] += RR_PI;
		light.setPosition(pos);
		light.setYawPitchRollRad(yawPitchRollRad);
		light.setAspect(aspect);
		light.setFieldOfViewVerticalDeg(fov);
		light.setRange(anear,afar);
		loaded = true;
	}
	if (fscanf(f,"brightness = {%f,%f,%f,%f}\n",&brightness[0],&brightness[1],&brightness[2],&brightness[3])==4) loaded = true;
	if (fscanf(f,"gamma = %f\n",&gamma)==1) loaded = true;
	// load dynaPosRot
	dynaPosRot.clear();
	DynaObjectPosRot tmp;
	while (5==fscanf(f,"object = {%f,%f,%f,%f,%f}\n",&tmp.pos[0],&tmp.pos[1],&tmp.pos[2],&tmp.rot[0],&tmp.rot[1]))
	{
		dynaPosRot.push_back(tmp);
		loaded = true;
	}
	// load projectorIndex
	if (fscanf(f,"projector = %d\n",&projectorIndex)==1) loaded = true;
	// load overlay
	overlayFilename[0] = 0;
	if (fscanf(f,"2d_overlay = %f,%d,%s\n",&overlaySeconds,&overlayMode,overlayFilename)==3) loaded = true;
	overlayMap = overlayFilename[0] ? rr::RRBuffer::load(overlayFilename) : NULL;
	// load technique
	if (fscanf(f,"shadow_type = %d\n",&shadowType)==1) loaded = true;
	if (fscanf(f,"indirect_type = %d\n",&indirectType)==1) loaded = true;
	// load volume
	if (fscanf(f,"volume = %f\n",&volume)==1) loaded = true;
	// load timing
	if (fscanf(f,"duration = %f\n",&transitionToNextTime)==1) loaded = true;
	//if (0!=fscanf(f,"\n"))
	//	return false;
	//rr::RRReporter::report(rr::INF1,"  frame with %d objects\n",dynaPosRot.size());
	return loaded;
}

void AnimationFrame::validate(unsigned numObjects)
{
	// generate object positions when objects were added by hand
	while (dynaPosRot.size()<numObjects)
	{
		// create positions when objects were added by hand into scene
		DynaObjectPosRot tmp;
		dynaPosRot.push_back(tmp);
	}
}

// save frame to opened .ani file
bool AnimationFrame::save(FILE* f, const AnimationFrame& prev) const
{
	if (!f) return false;
	// save layer number
	fprintf(f,"layer_number = %d\n",layerNumber);
	// save eye+light
	if (!(eye==prev.eye))
		fprintf(f,"camera = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eye.getPosition()[0],eye.getPosition()[1],eye.getPosition()[2],fmodf(eye.getYawPitchRollRad()[0]+101*RR_PI,2*RR_PI),eye.getYawPitchRollRad()[2],eye.getYawPitchRollRad()[1],eye.getAspect(),eye.getFieldOfViewVerticalDeg(),eye.getNear(),eye.getFar());
	if (!(light==prev.light))
		fprintf(f, "light = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",light.getPosition()[0],light.getPosition()[1],light.getPosition()[2],fmodf(light.getYawPitchRollRad()[0]+101*RR_PI,2*RR_PI),light.getYawPitchRollRad()[2],light.getYawPitchRollRad()[1],light.getAspect(),light.getFieldOfViewVerticalDeg(),light.getNear(),light.getFar());
	if (brightness!=prev.brightness)//brightness[0]!=1 || brightness[1]!=1 || brightness[2]!=1 || brightness[3]!=1)
		fprintf(f,"brightness = {%f,%f,%f,%f}\n",brightness[0],brightness[1],brightness[2],brightness[3]);
	if (gamma!=prev.gamma)//gamma!=1)
		fprintf(f,"gamma = %f\n",gamma);
	// save dynaPosRot
	for (DynaPosRot::const_iterator i=dynaPosRot.begin();i!=dynaPosRot.end();i++)
		fprintf(f,"object = {%.3f,%.3f,%.3f,%.3f,%.3f}\n",(*i).pos[0],(*i).pos[1],(*i).pos[2],(*i).rot[0],(*i).rot[1]);
	// save projectorIndex
	if (projectorIndex!=prev.projectorIndex)
		fprintf(f,"projector = %d\n",projectorIndex);
	// save overlay
	if (overlayFilename[0])
		fprintf(f,"2d_overlay = %.3f,%d,%s\n",overlaySeconds,overlayMode,overlayFilename);
	// save technique
	if (shadowType!=prev.shadowType)
		fprintf(f,"shadow_type = %d\n",shadowType);
	if (indirectType!=prev.indirectType)
		fprintf(f,"indirect_type = %d\n",indirectType);
	// save volume
	if (volume!=prev.volume)
		fprintf(f,"volume = %.2f\n",volume);
	// save timing
	if (transitionToNextTime!=prev.transitionToNextTime)
		fprintf(f,"duration = %.3f\n",transitionToNextTime);
	fprintf(f,"\n");
	//rr::RRReporter::report(rr::INF1,"  frame with %d objects\n",dynaPosRot.size());
	return true;
}
