#ifndef NEUMONT_TYPE_DEFS_H
#define NEUMONT_TYPE_DEFS_H

typedef unsigned short ushort;
typedef unsigned int uint;
#define PI 3.14159265359f
#define HUGE_K 1.0e10

inline float min(float x, float y)
{ return ((x > y) ? y : x); }

inline float max(float x, float y)
{ return ((x > y) ? x : y); }

#endif