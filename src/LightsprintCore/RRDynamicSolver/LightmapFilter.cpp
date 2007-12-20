#include "LightmapFilter.h"
#include "../RRMathPrivate.h"

namespace rr
{

void filter(RRVec4* inputs,RRVec4* outputs,unsigned width,unsigned height,bool* _changed,bool _wrap)
{
	unsigned size = width*height;
	bool changed = false;
	if(_wrap)
	{
		// faster version with wrap
		#pragma omp parallel for schedule(static)
		for(int i=0;i<(int)size;i++)
		{
			RRVec4 c = inputs[i];
			if(c[3]>=1)
			{
				outputs[i] = c;
			}
			else
			{
				RRVec4 c1 = inputs[(i+1)%size];
				RRVec4 c2 = inputs[(i-1)%size];
				RRVec4 c3 = inputs[(i+width)%size];
				RRVec4 c4 = inputs[(i-width)%size];
				RRVec4 c5 = inputs[(i+1+width)%size];
				RRVec4 c6 = inputs[(i+1-width)%size];
				RRVec4 c7 = inputs[(i-1+width)%size];
				RRVec4 c8 = inputs[(i-1-width)%size];

				RRVec4 cc = ( c + (c1+c2+c3+c4)*0.25f + (c5+c6+c7+c8)*0.166f )*1.3f;

				outputs[i] = cc/MAX(cc.w,1.0f);

				changed = true;
			}
		}
	}
	else
	{
		// slower version without wrap
		#pragma omp parallel for schedule(static)
		for(int i=0;i<(int)size;i++)
		{
			RRVec4 c = inputs[i];
			if(c[3]>=1)
			{
				outputs[i] = c;
			}
			else
			{
				int x = i%width;
				int y = i/width;
				int w = (int)width;
				int h = (int)height;
				RRVec4 c1 = inputs[MIN(x+1,w-1) + y*w];
				RRVec4 c2 = inputs[MAX(x-1,0)   + y*w];
				RRVec4 c3 = inputs[x            + MIN(y+1,h-1)*w];
				RRVec4 c4 = inputs[x            + MAX(y-1,0)*w];
				RRVec4 c5 = inputs[MIN(x+1,w-1) + MIN(y+1,h-1)*w];
				RRVec4 c6 = inputs[MIN(x+1,w-1) + MAX(y-1,0)*w];
				RRVec4 c7 = inputs[MAX(x-1,0)   + MIN(y+1,h-1)*w];
				RRVec4 c8 = inputs[MAX(x-1,0)   + MAX(y-1,0)*w];

				RRVec4 cc = ( c + (c1+c2+c3+c4)*0.25f + (c5+c6+c7+c8)*0.166f )*1.3f;

				outputs[i] = cc/MAX(cc.w,1.0f);

				changed = true;
			}
		}

	}
	*_changed = changed;
}

LightmapFilter::LightmapFilter(unsigned _width, unsigned _height)
{
	width = _width;
	height = _height;
	renderedTexels = new rr::RRVec4[width*height];
	memset(renderedTexels,0,width*height*sizeof(RRVec4));
	numRenderedTexels = 0;
}

void LightmapFilter::renderTexel(const unsigned uv[2], const RRVec4& color)
{
	if(uv[0]>=width)
	{
		RR_ASSERT(0);
		return;
	}
	if(uv[1]>=height)
	{
		RR_ASSERT(0);
		return;
	}
	renderedTexels[uv[0]+uv[1]*width] += color;
	numRenderedTexels++;
}

const RRVec4* LightmapFilter::getFiltered(const RRDynamicSolver::FilteringParameters* _params)
{
	RRDynamicSolver::FilteringParameters params;
	if(_params) params = *_params;

	if(!numRenderedTexels)
	{
		if(width*height<=64*64 || width<=16 || height<=16)
			rr::RRReporter::report(rr::WARN,"No texels rendered into map, low resolution(%dx%d) or bad unwrap (see RRMesh::getTriangleMapping)?\n",width,height);
		else
			rr::RRReporter::report(rr::WARN,"No texels rendered into map, bad unwrap (see RRMesh::getTriangleMapping)?\n");
		return NULL;
	}

	unsigned numTexels = width*height;

	// normalize texels
	// texels are always sum of triangle contributions with alpha=1,
	//  so texel that intersects X triangles has alpha=X. division by X normalizes rgb
	// (when reliability was stored in alpha:
	//   magnifies noise in low reliability texels
	//   but prevents unwanted leaks from distant areas into low reliability areas)
	// outputs: normalized rgb, alpha is always 0 or 1
#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)numTexels;i++)
	{
		if(renderedTexels[i][3])
		{
			renderedTexels[i] /= renderedTexels[i][3];
		}
#ifdef DIAGNOSTIC_RED_UNRELIABLE
		else
		{
			renderedTexels[i] = RRVec4(1,0,0,0);//!!!
		}
#endif
	}

#ifdef DIAGNOSTIC
	// convert to visible colors
	unsigned diagColors[] =
	{
		0,0xff0000ff,0xff00ff00,0xffff0000,0xffffff00,0xffff00ff,0xff00ffff,0xffffffff
	};
	for(unsigned i=0;i<numTexels;i++)
	{
		renderedTexels[i].color = diagColors[CLAMPED(renderedTexels[i].color&255,0,7)];
	}
#endif

#ifndef DIAGNOSTIC_RED_UNRELIABLE
	// alloc second workspace
	RRVec4* workspaceTexels = new rr::RRVec4[numTexels];
	memset(workspaceTexels,0,numTexels*sizeof(RRVec4));

	// spread foreground color
	bool changed = true;
	for(unsigned pass=0;pass<params.spreadForegroundColor && changed;pass++)
	{
		filter(renderedTexels,workspaceTexels,width,height,&changed,params.wrap);
		filter(workspaceTexels,renderedTexels,width,height,&changed,params.wrap);
	}

	// free second workspace
	SAFE_DELETE_ARRAY(workspaceTexels);

	// set background color
	for(unsigned i=0;i<numTexels;i++)
	{
		RRReal alpha = renderedTexels[i][3];
		if(params.smoothBackground)
		{
			if(alpha<1)
			{
				renderedTexels[i] = renderedTexels[i] + params.backgroundColor*(1-alpha);
			}
		}
		else
		{
			if(alpha<=0)
			{
				renderedTexels[i] = params.backgroundColor;
			}
			else
			{
				renderedTexels[i] = renderedTexels[i]/alpha;
			}
		}
	}

	return renderedTexels;
#endif
}

LightmapFilter::~LightmapFilter()
{
	delete[] renderedTexels;
}

}; // namespace
