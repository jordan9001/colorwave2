#ifndef COLORCONTROL_H
#define COLORCONTROL_H

#include "pxpattern.h"

#include <Adafruit_NeoPixel.h>
#include <stdint.h>

#define NUM_PX 109

typedef struct {
    uint16_t count;
    pattern_gradpoint* pts;
} cctx_gradient;

typedef struct {
    uint16_t duration;
    uint8_t blend;
    cctx_gradient gradient;
} cctx_frame;

typedef struct {
    uint16_t framecount;
    cctx_frame* frames;
    uint16_t current_frame;
    uint16_t current_step; // number of refreshes we have spent on this frame
} cctx_anigradient;

typedef struct {
    uint16_t timeout; // in seconds

    uint8_t type; // PATTERN_TYPE_X

    union {
        cctx_gradient gradient;
        cctx_anigradient anigradient;
    };
} color_context;

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx);

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx, uint16_t deltat);

void destroyctx(color_context* ctx);

#endif