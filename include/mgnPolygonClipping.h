#ifndef __MGN_POLYGON_CLIPPING_H__
#define __MGN_POLYGON_CLIPPING_H__

#include "MapDrawing/Graphics/mgnVector.h"

// Sutherland-Hodgman clipping algorithm implementation
// This algorithm works only with convex polygons, so be careful.

namespace clipping {

const float eps = 0.0001f; // all absolute values less than eps will be threated as zero 

typedef vec2 vec_t, *vec;
 
/* === polygon stuff === */
typedef struct poly_tag { int len, alloc; vec v; } poly_t, *poly;

int poly_winding(poly p);
void poly_free(poly p);
 
poly poly_clip(poly sub, poly clip);

// Fixed polygon functionality
typedef struct fixed_poly_tag { int len; vec_t v[8]; } fixed_poly_t, *fixed_poly;

int fixed_poly_winding(fixed_poly p);
void fixed_poly_clip(fixed_poly sub, fixed_poly clip, int dir, fixed_poly out);

} // namespace clipping

#endif
