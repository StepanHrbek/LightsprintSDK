#include <cassert>
#include "AnimationFrame.h"
#include "Lightsprint/RRDebug.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationFrame, one animation frame in editable form

AnimationFrame::AnimationFrame()
{
	de::Camera tmp[2] =
		{{{-3.266f,1.236f,1.230f},9.120f,0.100f,1.3f,45.0f,0.3f,1000.0f},
		{{-0.791f,1.370f,1.286f},3.475f,0.550f,1.0f,70.0f,1.0f,20.0f}};
	eyeLight[0] = tmp[0];
	eyeLight[1] = tmp[1];
	transitionToNextTime = 3;
	thumbnail = NULL;
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
	for(unsigned i=0;i<sizeof(eyeLight)/sizeof(float);i++)
		c[i] = a[i]*(1-alpha) + b[i]*alpha;
	blended.eyeLight[0].update(0);
	blended.eyeLight[1].update(0.3f);
	// blend dynaPosRot
	blended.dynaPosRot.clear();
	for(unsigned i=0;i<this->dynaPosRot.size() && i<that.dynaPosRot.size();i++)
	{
		DynaObjectPosRot tmp;
		tmp.pos = this->dynaPosRot[i].pos*(1-alpha) + that.dynaPosRot[i].pos*alpha;
		rr::RRVec2 thisRot = this->dynaPosRot[i].rot;
		rr::RRVec2 thatRot = that.dynaPosRot[i].rot;
		for(unsigned j=0;j<2;j++)
		{
			// never do blend between more than 180 degree distance
			thisRot[j] = fmodf(thisRot[j],360);
			thatRot[j] = fmodf(thatRot[j],360);
			if(thisRot[j]<thatRot[j]-180) thisRot[j] += 360; else
			if(thisRot[j]>thatRot[j]+180) thisRot[j] -= 360;
		}
		tmp.rot = thisRot*(1-alpha) + thatRot*alpha;
		blended.dynaPosRot.push_back(tmp);
	}
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
	if(9!=fscanf(f,"camera = {{%f,%f,%f},%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].height,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
		return false;
	i = 1;
	if(9!=fscanf(f,"light = {{%f,%f,%f},%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].height,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
		return false;
	// load dynaPosRot
	dynaPosRot.clear();
	DynaObjectPosRot tmp;
//	while(4==fscanf(f,"object = {%f,%f,%f,%f}\n",&tmp.pos[0],&tmp.pos[1],&tmp.pos[2],&tmp.rot[0]))
//		dynaPosRot.push_back(tmp);
	while(5==fscanf(f,"object = {%f,%f,%f,%f,%f}\n",&tmp.pos[0],&tmp.pos[1],&tmp.pos[2],&tmp.rot[0],&tmp.rot[1]))
		dynaPosRot.push_back(tmp);
	// load timing
	fscanf(f,"duration = %f\n",&transitionToNextTime);
	//if(0!=fscanf(f,"\n"))
	//	return false;
	rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
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
	fprintf(f,"camera = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].height,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
	i = 1;
	fprintf(f,"light = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].height,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
	// save dynaPosRot
	for(DynaPosRot::const_iterator i=dynaPosRot.begin();i!=dynaPosRot.end();i++)
		fprintf(f,"object = {%.3f,%.3f,%.3f,%.3f,%.3f}\n",(*i).pos[0],(*i).pos[1],(*i).pos[2],(*i).rot[0],(*i).rot[1]);
	// save timing
	fprintf(f,"duration = %.3f\n",transitionToNextTime);
	fprintf(f,"\n");
	rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
	return true;
}
