#ifndef ANIMATION_KEY_SET
#define ANIMATION_KEY_SET
#include <vector>
#include "glm\glm.hpp"

struct TimeKey
{    
  float time;
  glm::vec3 value;

  TimeKey()
  {       
    value.x = 0;
    value.y = 0;
    value.z = 0;
  }
};

struct RotateKey
{
  double time, value;
};

struct AnimationKeyFrameSet
{
  int index;
  std::vector<TimeKey> transKeys;
  std::vector<TimeKey> scaleKeys;
  std::vector<TimeKey> rotKeys;

  void clear()
  {
    transKeys.clear();
    scaleKeys.clear();
    rotKeys.clear();
  }
};

struct skeletonBone
{
	char boneName[16];
	short ParentId;
};

#endif