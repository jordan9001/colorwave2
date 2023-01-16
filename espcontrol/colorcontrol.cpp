
#include "colorcontrol.h"
#include "pxpattern.h"
#include "dbg.h"

#include <Arduino.h>

static bool parse_gradientpkt(pattern_gradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_gradient)) {
        dbgf("Tried to parse packet smaller than min pattern_gradient: %d\n", len);
        return false;
    }

    uint16_t num_pts = data->count;

    uint8_t* end = ((uint8_t*)&(data->pts)) + (num_pts * sizeof(data->pts[0]));
    if (end != (((uint8_t*)data) + len)) {
        dbgf("Tried to parse gradient packet with bad length: %d %d\n", num_pts, len);
        return false;
    }

    // save this gradient
    // since this isn't animated, we just need to save the gradpoints
    pattern_gradpoint* arr = new pattern_gradpoint[num_pts];
    uint16_t highest = 0;
    for (int i = 0; i < num_pts; i++) {
        arr[i] = data->pts[i];
        // check they are in order
        if (arr[i].n >= highest) {
            highest = arr[i].n;
        } else {
            dbgf("Got a gradpoint that is not in order! %d %d\n", arr[i].n, highest);
            delete[] arr;
            return false;
        }
    }

    ctx->gradient.pts = arr;
    ctx->gradient.count = num_pts;

    return true;
}

static bool parse_anigradientpkt(pattern_anigradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_anigradient)) {
        dbgf("Tried to parse packet smaller than min pattern_anigradient: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

static bool parse_randgradientpkt(pattern_randgradient* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_randgradient)) {
        dbgf("Tried to parse packet smaller than min pattern_randgradient: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

static bool parse_poppingpkt(pattern_popping* data, uint16_t len, color_context* ctx) {
    if (len < sizeof(pattern_popping)) {
        dbgf("Tried to parse packet smaller than min pattern_popping: %d\n", len);
        return false;
    }

    dbgf("TODO unimplemented parser");
    return false;
}

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx) {
    if (len < offsetof(pattern, grad)) {
        dbgf("Tried to parse packet smaller than min pattern: %d\n", len);
        return false;
    }

    pattern* pat = (pattern*)data;
    uint8_t type = pat->type;

    ctx->timeout = pat->timeout;
    ctx->type = type;

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

static void lerp_color(color* c1, color* c2, color* out, uint16_t step, uint16_t len) {
    // we don't do any special color lerp with hsv right now, but that would probably be better than this
    // maybe too expensive though?

    int16_t c1g = (int16_t)c1->g;
    int16_t c1r = (int16_t)c1->r;
    int16_t c1b = (int16_t)c1->b;

    int16_t dg = ((int16_t)c2->g) - c1g;
    int16_t dr = ((int16_t)c2->r) - c1r;
    int16_t db = ((int16_t)c2->b) - c1b;

    // could we optimize this? I hate to have the divide there
    dg = ((dg * step) / len);
    dr = ((dr * step) / len);
    db = ((db * step) / len);

    out->g = (uint8_t)(c1g + dg);
    out->r = (uint8_t)(c1r + dr);
    out->b = (uint8_t)(c1b + db);
}

static void render_grad(cctx_gradient* grad, color* colorarr, uint16_t numpx) {
    // renders the gradient to the colorarr
    if (grad->count == 0) {
        return;
    }

    pattern_gradpoint p1 = grad->pts[0];
    pattern_gradpoint p2;
    color pointcolor;
    int16_t cur = 0;
    uint16_t seglen;

    // first just flood fill up to the first point
    while (cur <= p1.n) {
        colorarr[cur] = p1.c;
        cur++;
    }

    // lerp between points
    for (int i = 1; i < grad->count; i++) {
        p2 = grad->pts[i];
        seglen = p2.n - p1.n;

        while (cur < p2.n) {
            if (cur >= numpx) {
                return;
            }

            // instead of using normal lerp, we could optimize the steps for each channel across the line
            lerp_color(&p1.c, &p2.c, &pointcolor, cur - p1.n, seglen);
            colorarr[cur] = pointcolor;
            cur++;
        }

        if (cur < numpx) {
            // fill true color for the point
            colorarr[cur] = p2.c;
            cur++;
        } else {
            return;
        }

        p1 = p2;
    }

    // flood fill past the last point

    while (cur < numpx) {
        colorarr[cur] = p1.c;
        cur++;
    }
}

static void write_colors(Adafruit_NeoPixel* px, color* colorarr, uint16_t numpx) {
    for (uint16_t i = 0; i < numpx; i++, colorarr++) {
        px->setPixelColor(i, colorarr->r, colorarr->g, colorarr->b);
    }
}

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx) {
    color line1[NUM_PX];
    color line2[NUM_PX];
    uint16_t nextframe = 0;

    // based on the current type, get_frame works differently

    if (ctx->type == PATTERN_TYPE_NONE) {
        return 0;
    }
    else if (ctx->type == PATTERN_TYPE_GRADIENT) {
        render_grad(&ctx->gradient, line1, NUM_PX);
        write_colors(px, line1, NUM_PX);
    }
    else {
        dbgf("Unimplemented get_frame for type %d\n", ctx->type);
        return 0;
    }

    px->show();
    return nextframe;
}

// Doesn't free the ctx itself, just any members that need to be
void destroyctx(color_context* ctx) {
    if (ctx->type == PATTERN_TYPE_GRADIENT) {
        delete[] ctx->gradient.pts;
    }
}