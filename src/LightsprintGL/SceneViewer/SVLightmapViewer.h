// --------------------------------------------------------------------------
// Scene viewer - lightmap viewer with unwrap, zoom, panning.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVLIGHTMAPVIEWER_H
#define SVLIGHTMAPVIEWER_H

#include "wx/wx.h"
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgram.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
//! Viewer of lightmap + unwrap, with zoom and panning.
//
//! Displays given lightmap in 2D with mapping as a vector overlay.
//! \n User controls:
//! - position = passive motion
//! - wheel = zoom
//! - left mouse button = toggle special alpha display
class SVLightmapViewer : public rr::RRUniformlyAllocated
{
public:
	SVLightmapViewer(const char* pathToShaders);
	~SVLightmapViewer();

	//! Sets object presented in viewer.
	void setObject(rr::RRBuffer* pixelBuffer, const rr::RRMesh* mesh, bool bilinear);

	//! Returns uv of point in the center of screen. Range: usually 0..1, may be out of range when user moves texture away.
	rr::RRVec2 getCenterUv(wxSize windowSize);

	void OnMouseEvent(wxMouseEvent& event, wxSize windowSize);
	void OnPaint(wxPaintEvent& event, wxSize windowSize);

private:
	bool nearest;
	bool alpha;
	rr::RRReal zoom; // 1: 1 lmap pixel has 1 screen pixel, 2: 1 lmap pixel has 0.5x0.5 screen pixels
	rr::RRVec2 center; // coord in pixels from center of lmap. texel with this coord is in center of screen
	UberProgram* uberProgram;
	Program* lmapProgram;
	Program* lmapAlphaProgram;
	Program* lineProgram;
	rr::RRBuffer* buffer;
	const rr::RRMesh* mesh;
	wxPoint previousPosition;
};

}; // namespace

#endif
