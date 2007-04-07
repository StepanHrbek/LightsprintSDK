#include <cassert>
#include "AnimationFrame.h"
#include "Lightsprint/RRDebug.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationFrame, one animation frame in editable form

AnimationFrame::AnimationFrame()
{
	de::Camera tmp[2] =
		{{{-3.266f,1.236f,1.230f},9.120f,0,0.100f,1.3f,45.0f,0.3f,1000.0f},
		{{-0.791f,1.370f,1.286f},3.475f,0,0.550f,1.0f,70.0f,1.0f,20.0f}};
	eyeLight[0] = tmp[0];
	eyeLight[1] = tmp[1];
	brightness = rr::RRVec4(1);
	gamma = 1;
	transitionToNextTime = 3;
	projectorIndex = 0;
	thumbnail = NULL;
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
	static AnimationFrame blended;
	// blend eyeLight
	float* a = (float*)(this->eyeLight);
	float* b = (float*)(that.eyeLight);
	float* c = (float*)(blended.eyeLight);
	for(unsigned i=0;i<(sizeof(eyeLight)+sizeof(brightness)+sizeof(gamma))/sizeof(float);i++)
		c[i] = a[i]*(1-alpha) + b[i]*alpha;
	for(unsigned i=0;i<2;i++)
		blended.eyeLight[i].angle = blendModulo(this->eyeLight[i].angle,that.eyeLight[i].angle,alpha,(float)(2*M_PI));
	blended.eyeLight[0].update(0);
	blended.eyeLight[1].update(0.3f);
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
	return &blended;
}

// load frame from opened .ani file
bool AnimationFrame::load(FILE* f)
{
	if(!f) return false;
	// load eyeLight
	unsigned i = 0;
	if(10!=fscanf(f,"camera = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].leanAngle,&eyeLight[i].angleX,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
		return false;
	i = 1;
	if(10!=fscanf(f,"light = {{%f,%f,%f},%f,%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].leanAngle,&eyeLight[i].angleX,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
		return false;
	brightness[0] = 1;
	brightness[1] = 1;
	brightness[2] = 1;
	brightness[3] = 1;
	fscanf(f,"brightness = {%f,%f,%f,%f}\n",&brightness[0],&brightness[1],&brightness[2],&brightness[3]);
	gamma = 1;
	fscanf(f,"gamma = %f\n",&gamma);
	// load dynaPosRot
	dynaPosRot.clear();
	DynaObjectPosRot tmp;
	while(5==fscanf(f,"object = {%f,%f,%f,%f,%f}\n",&tmp.pos[0],&tmp.pos[1],&tmp.pos[2],&tmp.rot[0],&tmp.rot[1]))
		dynaPosRot.push_back(tmp);
	// load projectorIndex
	projectorIndex = 0;
	fscanf(f,"projector = %d\n",&projectorIndex);
	// load timing
	fscanf(f,"duration = %f\n",&transitionToNextTime);
	//if(0!=fscanf(f,"\n"))
	//	return false;
	//rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
	return true;
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
bool AnimationFrame::save(FILE* f) const
{
	if(!f) return false;
	// save eyeLight
	unsigned i=0;
	fprintf(f,"camera = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].leanAngle,eyeLight[i].angleX,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
	i = 1;
	fprintf(f, "light = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].leanAngle,eyeLight[i].angleX,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
	if(brightness[0]!=1 || brightness[1]!=1 || brightness[2]!=1 || brightness[3]!=1)
		fprintf(f,"brightness = {%f,%f,%f,%f}\n",brightness[0],brightness[1],brightness[2],brightness[3]);
	if(gamma!=1)
		fprintf(f,"gamma = %f\n",gamma);
	// save dynaPosRot
	for(DynaPosRot::const_iterator i=dynaPosRot.begin();i!=dynaPosRot.end();i++)
		fprintf(f,"object = {%.3f,%.3f,%.3f,%.3f,%.3f}\n",(*i).pos[0],(*i).pos[1],(*i).pos[2],(*i).rot[0],(*i).rot[1]);
	// save projectorIndex
	fprintf(f,"projector = %d\n",projectorIndex);
	// save timing
	fprintf(f,"duration = %.3f\n",transitionToNextTime);
	fprintf(f,"\n");
	//rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
	return true;
}
