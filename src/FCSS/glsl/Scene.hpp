#ifndef SCENE_H
#define SCENE_H

class Torus;

class Scene
{
public:
  Scene();
  ~Scene();

  void draw();
private:
  void compileScene();
  
  static const float floorSize;
  
  Torus *t;
  unsigned int scene;
};

#endif //SCENE_H
