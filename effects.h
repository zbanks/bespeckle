#include "bespeckle.h"
#ifndef __EFFECTS_H__
#define __EFFECTS_H__

// Number of effects 
#define NUM_EFFECTS  21

#define EFFECTS_SLOWDOWN 9

#ifndef STRIP_LENGTH
// Length of LED strip
// sizeof(position_t) > STRIP_LENGTH
#define STRIP_LENGTH 50
#endif


extern EffectTable const effect_table[];

#endif /* __EFFECTS_H__ */
