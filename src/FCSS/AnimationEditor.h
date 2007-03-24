#ifndef ANIMATIONEDITOR_H
#define ANIMATIONEDITOR_H

#include "AnimationScene.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"

/////////////////////////////////////////////////////////////////////////////
//
// AnimationEditor, user interface and operations doable in editor

class AnimationEditor
{
public:
	AnimationEditor(LevelSetup* levelSetup);
	~AnimationEditor();

	void renderThumbnails(de::TextureRenderer* renderer) const;

	bool keyboard(unsigned char c, int x, int y);
	bool special(unsigned char c, int x, int y);

	float getCursorTime() const;
	unsigned frameCursor; // cislo snimku nad kterym je kurzor, 0..n
private:
	LevelSetup* setup;
	de::Texture* movieClipMap;
	de::Texture* cursorMap;
};

#endif
