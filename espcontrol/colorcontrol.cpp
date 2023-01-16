
#include "colorcontrol.h"
#include "pxpattern.h"
#include "dbg.h"

#include <Arduino.h>

bool parse_gradientpkt(pattern_gradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_gradient)) {
        dbgf("Tried to parse packet smaller than min pattern_gradient: %d\n", len);
        return false;
    }

    uint16_t num_pts = data->count;

    if (num_pts * sizeof(data->pts[0]) > len) {
        dbgf("Tried to parse gradient packet with gradpoints off the end of the buffer: %d %d\n", num_pts, len);
        return false;
    }

    // save this gradient
    for (int i = 0; i < num_pts; i++) {
        //TODO
    }

    dbgf("TODO unimplemented parser");
    return false;
}

bool parse_anigradientpkt(pattern_anigradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_anigradient)) {
        dbgf("Tried to parse packet smaller than min pattern_anigradient: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

bool parse_randgradientpkt(pattern_randgradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_randgradient)) {
        dbgf("Tried to parse packet smaller than min pattern_randgradient: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

bool parse_poppingpkt(pattern_popping* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_popping)) {
        dbgf("Tried to parse packet smaller than min pattern_popping: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern)) {
        dbgf("Tried to parse packet smaller than min pattern: %d\n", len);
        return false;
    }

    pattern* pat = (pattern*)data;

    ctx->timeout = pat->timeout;

    uint8_t type = pat->type;

    if (type == PATTERN_TYPE_GRADIENT) {
        return parse_gradientpkt(&pat->grad, len - offsetof(pattern, grad), ctx);
    }
    else if (type == PATTERN_TYPE_ANIGRADIENT) {
        return parse_anigradientpkt(&pat->anigrad, len - offsetof(pattern, anigrad), ctx);
    }
    else if (type == PATTERN_TYPE_RANDGRADIENT) {
        return parse_randgradientpkt(&pat->rndgrad, len - offsetof(pattern, rndgrad), ctx);
    }
    else if (type == PATTERN_TYPE_POPPING) {
        return parse_poppingpkt(&pat->pop, len - offsetof(pattern, pop), ctx);
    }
    else {
        dbgf("Unknown pattern type: %d\n", type);
    }
    return false;
}

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx) {
    //TODO
    return 0;
}

// Doesn't free the ctx itself, just any members that need to be
void destroyctx(color_context* ctx) {

}