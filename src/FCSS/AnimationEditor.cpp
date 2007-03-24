#include "AnimationEditor.h"
#include <GL/glut.h>

/////////////////////////////////////////////////////////////////////////////
//
// AnimationEditor, user interface and operations doable in editor

AnimationEditor::AnimationEditor(LevelSetup* levelSetup)
{
	setup = levelSetup;
	movieClipMap = de::Texture::load("maps\\movie_clip.jpg", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	cursorMap = de::Texture::load("maps\\cursor.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	frameCursor = 0;
}

AnimationEditor::~AnimationEditor()
{
	delete movieClipMap;
	delete cursorMap;
}

void AnimationEditor::renderThumbnails(de::TextureRenderer* renderer)
{
	unsigned index = 0;
	unsigned count = MAX(6,setup->frames.size()+1);
	for(LevelSetup::Frames::const_iterator i=setup->frames.begin();;i++,index++)
	{
		float x = index/(float)count;
		float y = 0;
		float w = 1/(float)count;
		float h = 0.15f;
		// thumbnail
		if(i!=setup->frames.end())
		{
			float intensity = 1;//(index==frameA || (index==frameB && secondsSinceFrameA>TIME_OF_STAY_STILL))?0.1f:1;
			float color[4] = {intensity,intensity,1,1};
			renderer->render2D(movieClipMap,color,x,y,w,h);
			if((*i).thumbnail)
				renderer->render2D((*i).thumbnail,NULL,x+w*0.05f,y+h*0.15f,w*0.9f,h*0.8f);
		}
		// cursor
		if(index==frameCursor)
		{
			GLboolean blend = glIsEnabled(GL_BLEND);
			glEnable(GL_BLEND);
			//glBlendFunc(GL_ONE,GL_ONE);
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			renderer->render2D(cursorMap,NULL,x,y+h*0.6f,w*0.3f,h*0.4f);
			if(!blend)
				glDisable(GL_BLEND);
		}
		// end for cycle
		if(i==setup->frames.end())
			break;
	}
}

bool AnimationEditor::keyboard(unsigned char c, int x, int y)
{
	switch(c)
	{
		case 127: // DELETE
			{LevelSetup::Frames::iterator i=setup->frames.begin();
			for(unsigned j=0;j<frameCursor;j++) i++;
			if(i!=setup->frames.end())
				setup->frames.erase(i);
			return true;}
			//default:
			//	printf("key=%d\n",(int)c);
	}
	return false;
}

bool AnimationEditor::special(unsigned char c, int x, int y)
{
	switch(c)
	{
		case GLUT_KEY_HOME:
			frameCursor = 0;
			return true;
		case GLUT_KEY_END:
			frameCursor = setup->frames.size();
			return true;
		case GLUT_KEY_LEFT:
			if(frameCursor) frameCursor--;
			return true;
		case GLUT_KEY_RIGHT:
			if(frameCursor<setup->frames.size()) frameCursor++;
			return true;
		case GLUT_KEY_INSERT:
			{LevelSetup::Frames::iterator i=setup->frames.begin();
			for(unsigned j=0;j<frameCursor;j++) i++;
			AnimationFrame tmp;
			extern void copySceneToAnimationFrame(AnimationFrame& frame);
			copySceneToAnimationFrame(tmp);
			setup->frames.insert(i,tmp);
			return true;}
		case GLUT_KEY_PAGE_UP:
			if(frameCursor)
			{
				std::swap(*setup->getFrameByIndex(frameCursor),*setup->getFrameByIndex(frameCursor-1));
				frameCursor--;
			}
			return true;
		case GLUT_KEY_PAGE_DOWN:
			if(frameCursor+1<setup->frames.size())
			{
				std::swap(*setup->getFrameByIndex(frameCursor),*setup->getFrameByIndex(frameCursor+1));
				frameCursor++;
			}
			return true;
	}
	return false;
}
