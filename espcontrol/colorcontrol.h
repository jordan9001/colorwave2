#ifndef COLORCONTROL_H
#define COLORCONTROL_H

#include <Adafruit_NeoPixel.h>
#include <stdint.h>

typedef struct {
    uint16_t timeout; // in seconds
} color_context;

bool parse_packet(uint8_t* data, uint16_t len, color_context* ctx);

uint16_t get_frame(Adafruit_NeoPixel* px, color_context* ctx);

void destroyctx(color_context* ctx);

#endif