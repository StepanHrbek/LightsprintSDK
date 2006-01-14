#include "FrameRate.hpp"

// Public part :

FrameRate::FrameRate(const char *nWindowName)
  :last(glutGet(GLUT_ELAPSED_TIME)), frameCount(0), lastFrameRate(0),
   frameRateChanged(1), windowName(nWindowName)
{
}

FrameRate::~FrameRate()
{
}

void FrameRate::newFrame()
{
  int currentTime = glutGet(GLUT_ELAPSED_TIME);
  frameCount++;
  if(currentTime - last > 500)
    {
      lastFrameRate = (float)frameCount * MILLI / (currentTime - last);
      last = currentTime;
      frameCount = 0;
      frameRateChanged = 1;
    }
  else
    frameRateChanged = 0;
}

void FrameRate::displayFrameRate()
{
  char title[50];

  newFrame();
  if(isFrameRateDifferent())
    {
      sprintf(title, "%s   Fps : %f.", windowName, getFrameRate());
      glutSetWindowTitle(title);
    }
}

// Private part :

const int FrameRate::MILLI = 1000;
