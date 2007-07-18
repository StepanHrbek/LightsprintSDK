#include "AnimationEditor.h"
#include <GL/glut.h>
#include <time.h>

/////////////////////////////////////////////////////////////////////////////
//
// AnimationEditor, user interface and operations doable in editor

AnimationEditor::AnimationEditor(LevelSetup* levelSetup)
{
	setup = levelSetup;
	movieClipMap = rr_gl::Texture::load("maps\\movie_clip.jpg", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	cursorMap = rr_gl::Texture::load("maps\\cursor.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	frameCursor = 0;
}

AnimationEditor::~AnimationEditor()
{
	delete movieClipMap;
	delete cursorMap;
}

void AnimationEditor::renderThumbnails(rr_gl::TextureRenderer* renderer) const
{
	unsigned index = 0;
	unsigned count = MAX(6,setup->frames.size()+1);
	//for(LevelSetup::Frames::const_iterator i=setup->frames.begin();i!=setup->frames.end();i++)
	//	if((*i).thumbnail) assert((*i).thumbnail->__vfptr!=0xfeeefeee);
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
			if((*i)->thumbnail)
				renderer->render2D((*i)->thumbnail,NULL,x+w*0.05f,y+h*0.15f,w*0.9f,h*0.8f);
		}
		// cursor
		if(index==frameCursor)
		{
			GLboolean blend = glIsEnabled(GL_BLEND);
			glEnable(GL_BLEND);
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			float move = fabs((clock()%CLOCKS_PER_SEC)/(float)CLOCKS_PER_SEC-0.5f);
			renderer->render2D(cursorMap,NULL,x,y+h*(0.1f+move),w*0.3f,h*0.4f);
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
		case 'S':
			setup->save();
			break;
	}
	return false;
}

bool AnimationEditor::special(unsigned char c, int x, int y)
{
	int modif = glutGetModifiers();
	switch(c)
	{
		case GLUT_KEY_HOME:
			frameCursor = 0;
			return true;
		case GLUT_KEY_END:
			frameCursor = setup->frames.size();
			return true;
		case GLUT_KEY_LEFT:
			if(modif)
			{
				AnimationFrame* frame=setup->getFrameByIndex(frameCursor);
				if(frame)
				{
					frame->transitionToNextTime = MAX(0.02f,frame->transitionToNextTime-((modif&GLUT_ACTIVE_CTRL)?0.5f:0.05f));
				}
			}
			else
			{
				if(frameCursor) frameCursor--;
			}
			return true;
		case GLUT_KEY_RIGHT:
			if(modif)
			{
				AnimationFrame* frame=setup->getFrameByIndex(frameCursor);
				if(frame)
				{
					if(frame->transitionToNextTime<0.03f) frame->transitionToNextTime=0;
					frame->transitionToNextTime += ((modif&GLUT_ACTIVE_CTRL)?0.5f:0.05f);
				}
			}
			else
			{
				if(frameCursor<setup->frames.size()) frameCursor++;
			}
			return true;
		case GLUT_KEY_INSERT:
			{LevelSetup::Frames::iterator i=setup->getFrameIterByIndex(frameCursor);
			AnimationFrame* tmp = new AnimationFrame(setup->newLayerNumber());
			extern void copySceneToAnimationFrame(AnimationFrame& frame, const LevelSetup* setup);
			copySceneToAnimationFrame(*tmp,setup);
			tmp->transitionToNextTime = (i!=setup->frames.end()) ? (*i)->transitionToNextTime : 3;
			setup->frames.insert(i,tmp);
			frameCursor++;
			return true;}
		case GLUT_KEY_PAGE_UP:
			if(frameCursor && frameCursor<setup->frames.size())
			{
				std::swap(*setup->getFrameIterByIndex(frameCursor),*setup->getFrameIterByIndex(frameCursor-1));
				frameCursor--;
			}
			return true;
		case GLUT_KEY_PAGE_DOWN:
			if(frameCursor+1<setup->frames.size())
			{
				std::swap(*setup->getFrameIterByIndex(frameCursor),*setup->getFrameIterByIndex(frameCursor+1));
				frameCursor++;
			}
			return true;
	}
	return false;
}

float AnimationEditor::getCursorTime() const
{
	return setup->getFrameTime(frameCursor);
}
