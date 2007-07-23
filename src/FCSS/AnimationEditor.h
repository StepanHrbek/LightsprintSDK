#ifndef ANIMATIONEDITOR_H
#define ANIMATIONEDITOR_H

#include "AnimationScene.h"
#include "Lightsprint/GL/TextureRenderer.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationEditor, user interface and operations doable in editor

class AnimationEditor
{
public:
	AnimationEditor(LevelSetup* levelSetup);
	~AnimationEditor();

	void renderThumbnails(rr_gl::TextureRenderer* renderer) const;

	bool keyboard(unsigned char c, int x, int y);
	bool special(unsigned char c, int x, int y);

	float getCursorTime() const;
	unsigned frameCursor; // cislo snimku nad kterym je kurzor, 0..n
private:
	LevelSetup* setup;
	rr_gl::Texture* movieClipMap;
	rr_gl::Texture* cursorMap;
};

#endif
