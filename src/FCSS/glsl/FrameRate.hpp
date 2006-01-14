#ifndef FRAME_RATE_H
#define FRAME_RATE_H

#include <iostream>
#include <GL/glut.h>

class FrameRate
{
public:
  FrameRate(const char *windowName = NULL);
  ~FrameRate();
  
  void newFrame();
  float getFrameRate() { return lastFrameRate; }
  bool isFrameRateDifferent() { return frameRateChanged; }
  void displayFrameRate();
private:
  static const int MILLI;
  
  int last;
  int frameCount;
  float lastFrameRate;
  bool frameRateChanged;
  const char *windowName;
};

#endif //FRAME_RATE_H
