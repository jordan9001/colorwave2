
#include "colorcontrol.h"
#include "pxpattern.h"
#include "dbg.h"

#include <Arduino.h>

static void randgrad(cctx_palette* colors, cctx_gradient* grad, uint16_t numpts, uint16_t len);

static bool parse_gradient(pattern_gradient* data, uint16_t len, cctx_gradient* out, uint8_t** next) {
    if (len < sizeof(pattern_gradient)) {
        dbgf("Tried to parse packet smaller than min pattern_gradient: %d\n", len);
        return false;
    }

    uint16_t num_pts = data->count;
    uint8_t* end = ((uint8_t*)&(data->pts)) + (num_pts * sizeof(data->pts[0]));

    if (end > (((uint8_t*)data) + len)) {
        dbgf("Tried to parse pattern_gradient with bad length: %d %d\n", num_pts, len);
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

    out->pts = arr;
    out->count = num_pts;

    if (next != NULL) {
        *next = (uint8_t*)(&data->pts[num_pts]);
    }

    return true;
}

static bool parse_gradientpkt(pattern_gradient* data, uint16_t len, color_context* ctx) {
    dbgl("Parsing gradient packet");
    return parse_gradient(data, len, &ctx->gradient, NULL);
}

static bool parse_anigradientpkt(pattern_anigradient* data, uint16_t len, color_context* ctx) {
    dbgl("Parsing anigradient packet");
    if (len < sizeof(pattern_anigradient)) {
        dbgf("Tried to parse packet smaller than min pattern_anigradient: %d\n", len);
        return false;
    }

    uint16_t count = data->framecount;
    ctx->anigradient.framecount = count;
    // check for huge counts?

    ctx->anigradient.current_frame = 0;
    ctx->anigradient.current_step = 0;
    
    cctx_frame* frames = new cctx_frame[count](); // initialized to zeros

    uint8_t* cursor = (uint8_t*)(&data->data);
    uint8_t* end = ((uint8_t*)data) + len;

    for (uint16_t i = 0; i < count; i++) {
        if ((cursor + offsetof(pattern_aniframe, grad)) >= end) {
            dbgf("Got past end while parsing frames\n");
            goto endclean;
        }

        pattern_aniframe* fr = (pattern_aniframe*)cursor;
        frames[i].duration = fr->duration;
        frames[i].blend = fr->blend;

        if (!parse_gradient(&fr->grad, (uint16_t)(end - (cursor + offsetof(pattern_aniframe, grad))), &frames[i].gradient, &cursor)) {
            goto endclean;
        }
    }

    ctx->anigradient.frames = frames;

    return true;

endclean:
    // if we fail in the middle we need to free the cctx_gradient[x]->pts
    for (uint16_t i = 0; i < count; i++) {
        delete[] frames[i].gradient.pts;
    }

    // and also free the array of cctx_gradient as well
    delete[] frames;

    return false;
}

static bool parse_randgradientpkt(pattern_randgradient* data, uint16_t len, color_context* ctx) {
    dbgl("Parsing randgradient packet");
    if (len < sizeof(pattern_randgradient)) {
        dbgf("Tried to parse packet smaller than min pattern_randgradient: %d\n", len);
        return false;
    }

    uint16_t minpts = data->gradpoints_min;
    if (minpts == 0) {
        minpts = 1;
    }
    uint16_t maxpts = data->gradpoints_max + 1;
    if (maxpts <= minpts) {
        maxpts = minpts+1;
    }
    uint16_t mindur = data->duration_min;
    if (mindur == 0) {
        mindur = 1;
    }
    uint16_t maxdur = data->duration_max + 1;
    if (maxdur <= mindur) {
        maxdur = mindur+1;
    }

    ctx->randgradient.gradpoints_min = minpts;
    ctx->randgradient.gradpoints_max = maxpts;
    ctx->randgradient.duration_min = mindur;
    ctx->randgradient.duration_max = maxdur;

    uint16_t count = data->colors.count;
    pattern_colorrange* cursor = (pattern_colorrange*)(&data->colors.ranges);
    uint8_t* end = ((uint8_t*)data) + len;

    if ((((uint8_t*)cursor) + (count * sizeof(pattern_colorrange))) != end) {
        dbgf("Tried to parse randgradient pkt but the sizes didn't match up: %d %d\n", count, len);
        return false;
    }

    pattern_colorrange* colors = new pattern_colorrange[count];

    for (uint16_t i = 0; i < count; i++) {
        colors[i] = cursor[i];
    }

    ctx->randgradient.colors.count = count;
    ctx->randgradient.colors.ranges = colors;

    randgrad(&ctx->randgradient.colors, &ctx->randgradient.frame1, random(minpts, maxpts), NUM_PX);
    randgrad(&ctx->randgradient.colors, &ctx->randgradient.frame2, random(minpts, maxpts), NUM_PX);

    ctx->randgradient.current_step = 0;
    ctx->randgradient.current_duration = random(mindur, maxdur);

    return true;
}

static bool parse_poppingpkt(pattern_popping* data, uint16_t len, color_context* ctx) {
    dbgl("Parsing popping packet");
    if (len < sizeof(pattern_popping)) {
        dbgf("Tried to parse packet smaller than min pattern_popping: %d\n", len);
        return false;
    }

    ctx->popping.frametillspot_max = data->frametillspot_max+1;
    ctx->popping.frametillspot_min = data->frametillspot_min;
    ctx->popping.frametillspot = 0;

    ctx->popping.growspot_max = data->growspot_max+1;
    ctx->popping.growspot_min = data->growspot_min;
    ctx->popping.sizespot_max = data->sizespot_max+1;
    ctx->popping.sizespot_min = data->sizespot_min;
    ctx->popping.spot_typeflags = data->spot_typeflags;

    ctx->popping.bg = data->bg;

    ctx->popping.fadeskip = data->fadeskip;
    ctx->popping.fadeamt = data->fadeamt;
    ctx->popping.fadestep = 0;

    memset(&ctx->popping.spots, 0, sizeof(ctx->popping.spots));
    ctx->popping.spots_next = 0;
    ctx->popping.spots_start = 0;

    uint16_t count = data->colors.count;
    pattern_colorrange* cursor = (pattern_colorrange*)(&data->colors.ranges);
    uint8_t* end = ((uint8_t*)data) + len;

    if ((((uint8_t*)cursor) + (count * sizeof(pattern_colorrange))) != end) {
        dbgf("Tried to parse popping pkt but the sizes didn't match up: %d %d\n", count, len);
        return false;
    }

    pattern_colorrange* colors = new pattern_colorrange[count];

    for (uint16_t i = 0; i < count; i++) {
        colors[i] = cursor[i];
    }

    ctx->popping.colors.count = count;
    ctx->popping.colors.ranges = colors;

    return true;
}

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx) {
    if (len < offsetof(pattern, grad)) {
        dbgf("Tried to parse packet smaller than min pattern: %d\n", len);
        return false;
    }

    if (len > 0x8fff) {
        dbgf("Huge len given: %d\n", len);
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

static void randcolor(cctx_palette* colors, color* out) {
    uint16_t i = random(0, colors->count);
    lerp_color(&colors->ranges[i].c1, &colors->ranges[i].c2, out, random(0, 0x41), 0x40);
}

static void randgrad(cctx_palette* colors, cctx_gradient* grad, uint16_t numpts, uint16_t len) {
    color c;

    // need a sorted set of random numbers for the positions
    std::vector<uint16_t> positions = std::vector<uint16_t>();
    for (uint16_t i = 0; i < numpts; i++) {
        positions.push_back(random(0, len));
    }

    std::sort(positions.begin(), positions.end());

    grad->pts = new pattern_gradpoint[numpts];

    for (uint16_t i = 0; i < numpts; i++) {
        grad->pts[i].n = positions[i];
        randcolor(colors, &c);
        grad->pts[i].c = c;
    }

    grad->count = numpts;
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

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx, uint16_t deltat) {
    color line1[NUM_PX];
    color line2[NUM_PX];
    color mid;
    uint32_t clr;
    uint16_t nextframe = 0;
    uint16_t dur;
    uint16_t step;

    //TODO timeout

    // based on the current type, get_frame works differently

    if (ctx->type == PATTERN_TYPE_NONE) {
        return 0;
    }
    else if (ctx->type == PATTERN_TYPE_GRADIENT) {
        render_grad(&ctx->gradient, line1, NUM_PX);
        write_colors(px, line1, NUM_PX);
    }
    else if (ctx->type == PATTERN_TYPE_ANIGRADIENT) {
        // what are the two we are looking between
        if (ctx->anigradient.framecount == 0) {
            return 0;
        }
        
        // TODO for really slow moving fades nextframe of 1 might be overkill
        // TODO hold type frames can use nextframe all the way to the next one
        nextframe = 1;
        
        uint16_t f1 = ctx->anigradient.current_frame;
        uint16_t f2 = f1 + 1;
        if (f2 >= ctx->anigradient.framecount) {
            f2 = 0;
        }

        uint8_t blend = ctx->anigradient.frames[f1].blend;
        dur = ctx->anigradient.frames[f1].duration;
        step = ctx->anigradient.current_step;

        if (ctx->anigradient.framecount == 1 || ctx->anigradient.current_step == 0 || blend == AGBLEND_HOLD) {
            // either we only have one gradient
            // or step is zero,  or type is holdmeaning we don't have to blend with another frame
            // or blend type is hold, so no blending
            render_grad(&ctx->anigradient.frames[f1].gradient, line1, NUM_PX);
            write_colors(px, line1, NUM_PX);
        }
        else {
            //TODO handle other blend types

            //TODO cache these
            render_grad(&ctx->anigradient.frames[f1].gradient, line1, NUM_PX);
            render_grad(&ctx->anigradient.frames[f2].gradient, line2, NUM_PX);

            for (uint16_t i = 0; i < NUM_PX; i++) {
                lerp_color(&line1[i], &line2[i], &mid, step, dur);
                px->setPixelColor(i, mid.r, mid.g, mid.b);
            }
        }

        // add to step/frame
        step += deltat;
        if (step >= dur) {
            step = 0;
            ctx->anigradient.current_frame = f2;
        }
        ctx->anigradient.current_step = step;
    }
    else if (ctx->type == PATTERN_TYPE_RANDGRADIENT) {
        nextframe = 1;

        step = ctx->randgradient.current_step;
        dur = ctx->randgradient.current_duration;
        
        if (step >= dur) {
            // alloc new frame2 and just display frame1
            delete[] ctx->randgradient.frame1.pts;
            ctx->randgradient.frame1 = ctx->randgradient.frame2;

            randgrad(&ctx->randgradient.colors, &ctx->randgradient.frame2, random(ctx->randgradient.gradpoints_min, ctx->randgradient.gradpoints_max), NUM_PX);

            step = 0;
            ctx->randgradient.current_duration = random(ctx->randgradient.duration_min, ctx->randgradient.duration_max);

            
            render_grad(&ctx->randgradient.frame1, line1, NUM_PX);
            write_colors(px, line1, NUM_PX);
        }
        else {
            // lerp
            render_grad(&ctx->randgradient.frame1, line1, NUM_PX);
            render_grad(&ctx->randgradient.frame2, line2, NUM_PX);

            for (uint16_t i = 0; i < NUM_PX; i++) {
                lerp_color(&line1[i], &line2[i], &mid, step, dur);
                px->setPixelColor(i, mid.r, mid.g, mid.b);
            }
        }

        step += deltat;
        ctx->randgradient.current_step = step;
    }
    else if (ctx->type == PATTERN_TYPE_POPPING) {
        nextframe = 1;

        // fade frame (but don't go below bg)
        if (ctx->popping.fadestep == 0) {

            uint8_t fd = ctx->popping.fadeamt;
            color bg = ctx->popping.bg;

            for (uint16_t i = 0; i < NUM_PX; i++) {
                clr = px->getPixelColor(i);
                mid.b = clr & 0xff;
                mid.g = (clr>>8) & 0xff;
                mid.r = (clr>>16) & 0xff;

                // possible overflow if fd is too high
                if (mid.g > (bg.g + fd)) {
                    mid.g -= fd;
                } else {
                    mid.g = bg.g;
                }
                if (mid.r > (bg.r + fd)) {
                    mid.r -= fd;
                } else {
                    mid.r = bg.r;
                }
                if (mid.b > (bg.b + fd)) {
                    mid.b -= fd;
                } else {
                    mid.b = bg.b;
                }

                px->setPixelColor(i, mid.r, mid.g, mid.b);
            }

            ctx->popping.fadestep = ctx->popping.fadeskip;
        } else {
            ctx->popping.fadestep--;
        }

        // if we are due to pop one in, do that
        uint16_t next = ctx->popping.spots_next;
        uint16_t pushnext = next + 1;
        if (pushnext == MAX_SPOTS) {
            pushnext = 0;
        }
        uint16_t start = ctx->popping.spots_start;

        cctx_spot* spt;
        if (ctx->popping.frametillspot == 0 && pushnext != start) {
            spt = &ctx->popping.spots[next];
            next = pushnext;

            ctx->popping.frametillspot = random(ctx->popping.frametillspot_min, ctx->popping.frametillspot_max);
            
            spt->pos = random(0, NUM_PX+1);

            // types
            uint8_t sptype = (ctx->popping.spot_typeflags & (SPOT_FUZZ | SPOT_SOLID));
            if (sptype == (SPOT_FUZZ | SPOT_SOLID)) {
                if (random(0,0x100) & 0x1) {
                    sptype = SPOT_FUZZ;
                } else {
                    sptype = SPOT_SOLID;
                }
            }
            spt->type = sptype;

            randcolor(&ctx->popping.colors, &spt->c);

            spt->sz = random(ctx->popping.sizespot_min, ctx->popping.sizespot_max);
            spt->off = spt->sz / 2;
            spt->growtime = random(ctx->popping.growspot_min, ctx->popping.growspot_max);
            // scale color by growtime
            if (spt->growtime > 0) {
                spt->c.g /= spt->growtime;
                spt->c.r /= spt->growtime;
                spt->c.b /= spt->growtime;
            }
        } else if (ctx->popping.frametillspot != 0) {
            ctx->popping.frametillspot--;
        }

        // grow spots and get rid of live ones
        for (uint16_t i = start; i != next; i = (i+1 >= MAX_SPOTS) ? 0 : i+1) {
            spt = &ctx->popping.spots[i];

            // add it's growing
            int16_t p = spt->pos;
            int16_t o = spt->off;
            int16_t n = p - o;
            int16_t e = n + spt->sz;
            for (; n < e; n++) {
                
                if (n < 0) {
                    continue;
                }
                if (n >= NUM_PX) {
                    break;
                }

                clr = px->getPixelColor(n);
                mid.b = clr & 0xff;
                mid.g = (clr>>8) & 0xff;
                mid.r = (clr>>16) & 0xff;

                if (spt->type == SPOT_SOLID || o == 0) {
                    mid.g += spt->c.g;
                    mid.r += spt->c.r;
                    mid.b += spt->c.b;
                } else {
                    // need to feather to center
                    int16_t d = n - p;
                    if (d < 0) {
                        d = -d;
                    }

                    mid.g += spt->c.g * d / o;
                    mid.r += spt->c.r * d / o;
                    mid.b += spt->c.b * d / o;
                }

                px->setPixelColor(n, mid.r, mid.g, mid.b);
            } 

            // clear this one if it is done
            if (spt->growtime == 0) {
                if (i != start) {
                    // gotta swap the live one into this spot
                    *spt = ctx->popping.spots[start];
                }

                // progress the ring
                start++;
                if (start >= MAX_SPOTS) {
                    start = 0;
                }

            } else {
                spt->growtime--;
            }
        }

        ctx->popping.spots_start = start;
        ctx->popping.spots_next = next;
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
    else if (ctx->type == PATTERN_TYPE_ANIGRADIENT) {
        // all the pts and then the frames
        for (uint16_t i = 0; i < ctx->anigradient.framecount; i++) {
            delete[] ctx->anigradient.frames[i].gradient.pts;
        }
        delete[] ctx->anigradient.frames;
    }
    else if (ctx->type == PATTERN_TYPE_RANDGRADIENT) {
        delete[] ctx->randgradient.colors.ranges;
        delete[] ctx->randgradient.frame1.pts;
        delete[] ctx->randgradient.frame2.pts;
    }
    else if (ctx->type == PATTERN_TYPE_POPPING) {
        //TODO
        // palette
    }
    else {
        dbgf("Got destroy call for unknown ctx type %d\n", ctx->type);
    }
}