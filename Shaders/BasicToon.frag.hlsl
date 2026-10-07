// The scene shader with toon shading (M79): the sun's light in three
// steps instead of a smooth Lambert. A variant rather than a branch, so
// other materials run the plain shader exactly.
#define ATOM_TOON 1
#include "Basic.frag.hlsl"
