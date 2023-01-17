#ifndef PXPATTERN_H
#define PXPATTERN_H

#include <stdint.h>

// This holds the packet definitions used to define a pixel pattern

// all these get sent in the packet, make sure they are packed

#pragma pack(push, 1)

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} color;

typedef struct {
    uint16_t n;                 // nth pixel
    color c;
} pattern_gradpoint;

// defines a gradient by having points of colors along
// the points must already be sorted linearly
// an empty gradient is black
typedef struct {
    uint16_t count;
    pattern_gradpoint pts[];
} pattern_gradient;

// ways to blend between anigradient frames
#define AGBLEND_HOLD        1
#define AGBLEND_LINEAR      2
#define AGBLEND_DISSOLVE    3
#define AGBLEND_RSLIDE      4
#define AGBLEND_LSLIDE      5

typedef struct {
    uint16_t duration;          // time till next keyframe
    uint8_t blend;              // how to fade to next keyframe
    pattern_gradient grad;
} pattern_aniframe;

// an animation that just fades between the provided gradients in a loop with blending options
typedef struct {
    uint16_t framecount;
    uint8_t data[];             // is an array of packed pattern_aniframes
} pattern_anigradient;

// represents a line of color
// because our lerp is just rgb linear it gets lots of greys
// so don't use wide ranging color ranges, prefer small ranges or ranges that just vary intensity
typedef struct {
    color c1;
    color c2;
} pattern_colorrange;

typedef struct {
    uint16_t count;
    pattern_colorrange ranges[];
} pattern_palette;

// like anigradient, but next frame is made randomly each time
typedef struct {
    uint8_t gradpoints_min;
    uint8_t gradpoints_max;
    uint16_t duration_min;
    uint16_t duration_max;
    pattern_palette colors;
} pattern_randgradient;

#define SPOT_FUZZ           1   // fade from center to edge
#define SPOT_SOLID          2   // whole spot fully lit

// spots that randomly flash up
// full frame fades over time to bg, so as soon as spot is faded in, we forget about it
typedef struct {
    uint8_t fadeamt;
    uint16_t fadeskip;
    uint16_t frametillspot_min;
    uint16_t frametillspot_max;
    uint16_t growspot_min;      // allows us to fade spots in over frames
    uint16_t growspot_max;
    uint16_t sizespot_min;      // diameter of the spot
    uint16_t sizespot_max;
    uint8_t spot_typeflags;
    color bg;
    pattern_palette colors;
} pattern_popping;

//TODO racer spots that zip around with velocity

#define PATTERN_TYPE_NONE           0
#define PATTERN_TYPE_GRADIENT       1
#define PATTERN_TYPE_ANIGRADIENT    2
#define PATTERN_TYPE_RANDGRADIENT   3
#define PATTERN_TYPE_POPPING        4

// main definition for a pattern
typedef struct {
    uint8_t type;
    uint16_t timeout;   // in seconds (max 18 hrs) (0 is no timeout)
    union {
        pattern_gradient grad;
        pattern_anigradient anigrad;
        pattern_randgradient rndgrad;
        pattern_popping pop;
    };
} pattern;
#pragma pack(pop)

#endif