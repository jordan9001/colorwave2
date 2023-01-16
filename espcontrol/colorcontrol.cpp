
#include "colorcontrol.h"
#include "pxpattern.h"
#include "dbg.h"

#include <Arduino.h>

void parse_gradientpkt(pattern_gradient* data, uint16_t len) {
    if (len < sizeof(pattern_gradient)) {
        dbgf("Tried to parse packet smaller than min pattern_gradient: %d\n", len);
        return;
    }

    uint16_t num_pts = data->n;

    if (num_pts * sizeof(data->pts[0]) > len) {
        dbgf("Tried to parse gradient packet with gradpoints off the end of the buffer: %d %d\n", num_pts, len);
        return;
    }

    // save this gradient
    for (int i = 0; i < num_pts; i++) {
        //TODO
    }

    dbgf("TODO unimplemented parser");
}

void parse_anigradientpkt(pattern_anigradient* data, uint16_t len) {
    if (len < sizeof(pattern_anigradient)) {
        dbgf("Tried to parse packet smaller than min pattern_anigradient: %d\n", len);
        return;
    }

    dbgf("TODO unimplemented parser");
}

void parse_randgradientpkt(pattern_randgradient* data, uint16_t len) {
    if (len < sizeof(pattern_randgradient)) {
        dbgf("Tried to parse packet smaller than min pattern_randgradient: %d\n", len);
        return;
    }

    dbgf("TODO unimplemented parser");
}

void parse_poppingpkt(pattern_popping* data, uint16_t len) {
    if (len < sizeof(pattern_popping)) {
        dbgf("Tried to parse packet smaller than min pattern_popping: %d\n", len);
        return;
    }

    dbgf("TODO unimplemented parser");
}

void parse_packet(uint8_t* data, uint16_t len) {
    if (len < sizeof(pattern)) {
        dbgf("Tried to parse packet smaller than min pattern: %d\n", len);
        return;
    }

    pattern* pat = (pattern*)data;
    uint8_t type = pat->type;
    if (type == PATTERN_TYPE_GRADIENT) {
        parse_gradientpkt(&pat->grad, len - offsetof(pattern, grad));
    }
    else if (type == PATTERN_TYPE_ANIGRADIENT) {
        parse_anigradientpkt(&pat->anigrad, len - offsetof(pattern, anigrad));
    }
    else if (type == PATTERN_TYPE_RANDGRADIENT) {
        parse_randgradientpkt(&pat->rndgrad, len - offsetof(pattern, rndgrad));
    }
    else if (type == PATTERN_TYPE_POPPING) {
        parse_poppingpkt(&pat->pop, len - offsetof(pattern, pop));
    }
    else {
        dbgf("Unknown pattern type: %d\n", type);
    }
}