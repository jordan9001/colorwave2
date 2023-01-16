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
    uint16_t timeout; // in seconds

    uint8_t type; // PATTERN_TYPE_X

    union {
        cctx_gradient gradient;
    };
} color_context;

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx);

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx);

void destroyctx(color_context* ctx);

#endif